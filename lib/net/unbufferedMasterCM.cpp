
/* Copyright (c) 2007-2010, Stefan Eilemann <eile@equalizergraphics.com> 
 *
 * This library is free software; you can redistribute it and/or modify it under
 * the terms of the GNU Lesser General Public License version 2.1 as published
 * by the Free Software Foundation.
 *  
 * This library is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public License for more
 * details.
 * 
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include "unbufferedMasterCM.h"

#include "command.h"
#include "commands.h"
#include "log.h"
#include "node.h"
#include "object.h"
#include "objectDeltaDataOStream.h"
#include "packets.h"
#include "session.h"

using namespace eq::base;
using namespace std;

namespace eq
{
namespace net
{
typedef CommandFunc<UnbufferedMasterCM> CmdFunc;

UnbufferedMasterCM::UnbufferedMasterCM( Object* object )
        : _object( object )
        , _version( 1 )
{
    registerCommand( CMD_OBJECT_COMMIT, 
                     CmdFunc( this, &UnbufferedMasterCM::_cmdCommit ), 0 );
    // sync commands are send to any instance, even the master gets the command
    registerCommand( CMD_OBJECT_DELTA,
                     CmdFunc( this, &UnbufferedMasterCM::_cmdDiscard ), 0 );
}

UnbufferedMasterCM::~UnbufferedMasterCM()
{
    if( !_slaves.empty( ))
        EQWARN << _slaves.size() 
               << " slave nodes subscribed during deregisterObject of "
               << _object->getID() << '.' << _object->getInstanceID() << endl;
    _slaves.clear();
}

uint32_t UnbufferedMasterCM::commitNB()
{
    EQASSERTINFO( _object->getChangeType() == Object::UNBUFFERED,
                  "Object type " << typeid(*this).name( ));

    NodePtr localNode = _object->getLocalNode();
    ObjectCommitPacket packet;
    packet.instanceID = _object->_instanceID;
    packet.requestID  = localNode->registerRequest();

    _object->send( localNode, packet );
    return packet.requestID;
}

uint32_t UnbufferedMasterCM::commitSync( const uint32_t commitID )
{
    NodePtr localNode = _object->getLocalNode();
    uint32_t version = VERSION_NONE;
    localNode->waitRequest( commitID, version );
    return version;
}

uint32_t UnbufferedMasterCM::addSlave( Command& command )
{
    CHECK_THREAD( _thread );
    EQASSERT( command->datatype == DATATYPE_EQNET_SESSION );
    EQASSERT( command->command == CMD_SESSION_SUBSCRIBE_OBJECT );

    NodePtr node = command.getNode();
    SessionSubscribeObjectPacket* packet =
        command.getPacket<SessionSubscribeObjectPacket>();
    const uint32_t version = packet->requestedVersion;
    const uint32_t instanceID = packet->instanceID;

    EQASSERT( version == VERSION_OLDEST || version == VERSION_NONE ||
              version == _version );

    // add to subscribers
    ++_slavesCount[ node->getNodeID() ];
    _slaves.push_back( node );
    stde::usort( _slaves );

    EQLOG( LOG_OBJECTS ) << "Object id " << _object->_id << " v" << _version
                         << ", instantiate on " << node->getNodeID() << endl;

    const bool useCache = packet->masterInstanceID == _object->getInstanceID();

    if( useCache && 
        packet->minCachedVersion <= _version && 
        packet->maxCachedVersion >= _version )
    {
#ifdef EQ_INSTRUMENT_MULTICAST
        ++_hit;
#endif
        return ( version == VERSION_OLDEST ) ? 
            packet->minCachedVersion : _version;
    }

    // send instance data
    ObjectInstanceDataOStream os( _object );
    os.setVersion( _version );
    os.setInstanceID( instanceID );
    os.setNodeID( node->getNodeID( ));

    if( version != VERSION_NONE ) // send current data
    {
        os.enable( node );
        _object->getInstanceData( os );
        os.disable();
    }

    if( !os.hasSentData( )) // if no data, send empty packet to set version
    {
        os.enable( node );
        os.writeOnce( 0, 0 );
        os.disable();
    }

#ifdef EQ_INSTRUMENT_MULTICAST
    ++_miss;
#endif
    return VERSION_INVALID; // no data was in cache
}

void UnbufferedMasterCM::removeSlave( NodePtr node )
{
    CHECK_THREAD( _thread );
    // remove from subscribers
    const NodeID& nodeID = node->getNodeID();
    EQASSERT( _slavesCount[ nodeID ] != 0 );

    --_slavesCount[ nodeID ];
    if( _slavesCount[ nodeID ] == 0 )
    {
        NodeVector::iterator i = find( _slaves.begin(), _slaves.end(), node );
        EQASSERT( i != _slaves.end( ));
        _slaves.erase( i );
        _slavesCount.erase( nodeID );
    }
}

void UnbufferedMasterCM::addOldMaster( NodePtr node, const uint32_t instanceID )
{
    // add to subscribers
    ++_slavesCount[ node->getNodeID() ];
    _slaves.push_back( node );
    stde::usort( _slaves );

    ObjectVersionPacket packet;
    packet.instanceID = instanceID;
    packet.version    = _version;
    _object->send( node, packet );
}


//---------------------------------------------------------------------------
// command handlers
//---------------------------------------------------------------------------
CommandResult UnbufferedMasterCM::_cmdCommit( Command& command )
{
    CHECK_THREAD( _thread );
    NodePtr localNode = _object->getLocalNode();
    const ObjectCommitPacket* packet = command.getPacket<ObjectCommitPacket>();
    EQLOG( LOG_OBJECTS ) << "commit v" << _version << " " << command << endl;

    if( _slaves.empty( ))
    {
        localNode->serveRequest( packet->requestID, _version );
        return COMMAND_HANDLED;
    }

    ObjectDeltaDataOStream os( _object );

    os.setVersion( _version + 1 );
    os.enable( _slaves );
    _object->pack( os );
    os.disable();

    if( os.hasSentData( ))
    {
        ++_version;
        EQASSERT( _version );
        EQLOG( LOG_OBJECTS ) << "Committed v" << _version << ", id " 
                             << _object->getID() << endl;
    }

    localNode->serveRequest( packet->requestID, _version );
    return COMMAND_HANDLED;
}
}
}
