
/* Copyright (c) 2007-2011, Stefan Eilemann <eile@equalizergraphics.com> 
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

#include "pipeConnection.h"

#include "connectionDescription.h"
#include "node.h"
#ifdef _WIN32
#  include "namedPipeConnection.h"
#endif

#include <co/base/log.h>
#include <co/base/thread.h>

namespace co
{

PipeConnection::PipeConnection()
{
    _description->type = CONNECTIONTYPE_PIPE;
    _description->bandwidth = 1024000;
}

PipeConnection::~PipeConnection()
{
#ifdef _WIN32
    close();
    _writePipe = 0;
    _readPipe = 0;
#endif
}

//----------------------------------------------------------------------
// connect
//----------------------------------------------------------------------
bool PipeConnection::connect()
{
    EQASSERT( _description->type == CONNECTIONTYPE_PIPE );

    if( _state != STATE_CLOSED )
        return false;

    _state = STATE_CONNECTING;
    _sibling = new PipeConnection;
    _sibling->_sibling = this;

    if( !_createPipes( ))
    {
        close();
        return false;
    }

    EQVERB << "readFD " << _readFD << " writeFD " << _writeFD << std::endl;
    _state = STATE_CONNECTED;
    _sibling->_state = STATE_CONNECTED;

    _fireStateChanged();
    return true;
}

#ifdef _WIN32

Connection::Notifier PipeConnection::getNotifier() const 
{ 
    EQASSERT( _readPipe );
    return _readPipe->getNotifier(); 
}

bool PipeConnection::_createPipes()
{
    _readPipe = new NamedPipeConnection;
    _writePipe = new NamedPipeConnection;

    std::stringstream pipeName;
    pipeName << "\\\\.\\Pipe\\Collage." << co::base::UUID( true );

    _readPipe->getDescription()->setFilename( pipeName.str() );
    _readPipe->_initAIOAccept();
    if( !_readPipe->_createNamedPipe( ))
        return false;
    _readPipe->_state = STATE_CONNECTED;

    _writePipe->getDescription()->setFilename( pipeName.str() );
    if( !_writePipe->_connectNamedPipe( ))
        return false;
    _writePipe->_state = STATE_CONNECTED;
    
    _sibling->_readPipe  = _writePipe;
    _sibling->_writePipe = _readPipe;
    return true;
}

void PipeConnection::close()
{
    if( _state == STATE_CLOSED )
        return;

    _writePipe->close();
    _readPipe->close();
    _sibling = 0;
    _readPipe = 0;
    _writePipe = 0;

    _state = STATE_CLOSED;
    _fireStateChanged();
}

void PipeConnection::readNB( void* buffer, const uint64_t bytes )
{
    if( _state == STATE_CLOSED )
        return;
    _readPipe->readNB( buffer, bytes );
}

int64_t PipeConnection::readSync( void* buffer, const uint64_t bytes,
                                       const bool ignored )
{
    if( _state == STATE_CLOSED )
        return -1;

    const int64_t bytesRead = _readPipe->readSync( buffer, bytes, ignored );

    if( bytesRead == -1 )
        close();
    
    return bytesRead;
}

int64_t PipeConnection::write( const void* buffer, const uint64_t bytes )
{
    if( _state != STATE_CONNECTED )
        return -1;

    return _writePipe->write( buffer, bytes );
}

#else // !_WIN32

bool PipeConnection::_createPipes()
{
    int pipeFDs[2];
    if( ::pipe( pipeFDs ) == -1 )
    {
        EQERROR << "Could not create pipe: " << strerror( errno );
        close();
        return false;
    }

    _readFD  = pipeFDs[0];
    _sibling->_writeFD = pipeFDs[1];

    if( ::pipe( pipeFDs ) == -1 )
    {
        EQERROR << "Could not create pipe: " << strerror( errno );
        close();
        return false;
    }

    _sibling->_readFD  = pipeFDs[0];
    _writeFD = pipeFDs[1];
    return true;
}

void PipeConnection::close()
{
    if( _state == STATE_CLOSED )
        return;

    if( _writeFD > 0 )
    {
        ::close( _writeFD );
        _writeFD = 0;
    }
    if( _readFD > 0 )
    {
        ::close( _readFD );
        _readFD  = 0;
    }
    _state = STATE_CLOSED;
    _sibling = 0;
    _fireStateChanged();
}
#endif // else _WIN32

}
