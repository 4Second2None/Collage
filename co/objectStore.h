
/* Copyright (c) 2005-2013, Stefan Eilemann <eile@equalizergraphics.com>
 *                    2010, Cedric Stalder <cedric.stalder@gmail.com>
 *                    2012, Daniel Nachbaur <danielnachbaur@gmail.com>
 *
 * This file is part of Collage <https://github.com/Eyescale/Collage>
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

#ifndef CO_OBJECTSTORE_H
#define CO_OBJECTSTORE_H

#include <co/dispatcher.h>    // base class
#include <co/version.h>       // enum

#include <lunchbox/lockable.h>  // member
#include <lunchbox/spinLock.h>  // member
#include <lunchbox/stdExt.h>    // member

#include "dataIStreamQueue.h"  // member

namespace co
{
    class InstanceCache;

    /** An object store manages Object mapping for a LocalNode. */
    class ObjectStore : public Dispatcher
    {
    public:
        /** Construct a new ObjectStore. */
        ObjectStore( LocalNode* localNode );

        /** Destruct this ObjectStore. */
        virtual ~ObjectStore();

        /** Remove all objects and clear all caches. */
        void clear();

        /** @name ICommand Dispatch */
        //@{
        /**
         * Dispatches an object command to the registered command queue.
         *
         * Object commands are dispatched to the appropriate objects mapped on
         * this session.
         *
         * @param command the command.
         * @return true if the command was dispatched, false otherwise.
         */
        bool dispatchObjectCommand( ICommand& command );
        //@}

        /** @name Object Registration */
        //@{
        /**
         * Register a distributed object.
         *
         * Registering a distributed object assigns a session-unique identifier
         * to this object, and makes this object the master version. The
         * identifier is used to map slave instances of the object. Master
         * versions of objects are typically writable and can commit new
         * versions of the distributed object.
         *
         * @param object the object instance.
         * @return true if the object was registered, false otherwise.
         */
        bool registerObject( Object* object );

        /**
         * Deregister a distributed object.
         *
         * @param object the object instance.
         */
        virtual void deregisterObject( Object* object );

        /** Start mapping a distributed object. */
        uint32_t mapObjectNB( Object* object, const UUID& id,
                              const uint128_t& version );

        /** Start mapping a distributed object. */
        uint32_t mapObjectNB( Object* object, const UUID& id,
                              const uint128_t& version, NodePtr master );

        /** Finalize the mapping of a distributed object. */
        bool mapObjectSync( const uint32_t requestID );

        /**
         * Unmap a mapped object.
         *
         * @param object the mapped object.
         */
        void unmapObject( Object* object );

        /**
         * Attach an object to an identifier.
         *
         * Attaching an object to an identifier enables it to receive object
         * commands though the local node. It does not establish any data
         * mapping to other object instances with the same identifier.
         *
         * @param object the object.
         * @param id the object identifier.
         * @param instanceID the node-local instance identifier, or
         *               EQ_INSTANCE_INVALID if this method should generate one.
         */
        void attachObject( Object* object, const UUID& id,
                           const uint32_t instanceID );

        /**
         * Detach an object.
         *
         * @param object the attached object.
         */
        void detachObject( Object* object );

        /** @internal swap the existing object by a new object and keep
                      the cm, id and instanceID. */
        void swapObject( Object* oldObject, Object* newObject );
        //@}

        /** @name Instance Cache. */
        //@{
        /** Expire all data older than age from the cache. */
        void expireInstanceData( const int64_t age );

        /** Remove all entries of the node from the cache. */
        void removeInstanceData( const NodeID& nodeID );

        /** Disable the instance cache of an stopped local node. */
        void disableInstanceCache();

        /** Enable sending data of newly registered objects when idle. */
        void enableSendOnRegister();

        /**
         * Disable sending data of newly registered objects when idle.
         *
         * Enable and disable are counted, that is, the last disable on a
         * matched series of enable/disable will be effective. When
         * send-on-register gets deactivated, the associated queue is cleared
         * and all data send on multicast connections is finished.
         */
        void disableSendOnRegister();

        /**
         * @internal
         * Notification - no pending commands for the command thread.
         * @return true if more work is pending.
         */
        virtual bool notifyCommandThreadIdle();

        /**
         * @internal
         * Remove a slave node in all objects
         */
        void removeNode( NodePtr node );
        //@}

    private:
        /** The local node managing the object store. */
        LocalNode* const _localNode;

        /** The identifiers for node-local instance identifiers. */
        lunchbox::a_int32_t _instanceIDs;

        /** enableSendOnRegister() invocations. */
        lunchbox::a_int32_t _sendOnRegister;

        typedef stde::hash_map< lunchbox::uint128_t, Objects > ObjectsHash;
        typedef ObjectsHash::const_iterator ObjectsHashCIter;

        /** All registered and mapped objects.
         *   - locked writes (only in receiver thread)
         *   - unlocked reads in receiver thread
         *   - locked reads in all other threads
         */
        lunchbox::Lockable< ObjectsHash, lunchbox::SpinLock > _objects;

        struct SendQueueItem
        {
            int64_t age;
            Object* object;
        };

        typedef std::deque< SendQueueItem > SendQueue;

        SendQueue _sendQueue;          //!< Object data to broadcast when idle
        InstanceCache* _instanceCache; //!< cached object mapping data
        DataIStreamQueue _pushData;    //!< Object::push() queue

        /**
         * Returns the master node id for an identifier.
         *
         * @param id the identifier.
         * @return the master node, or 0 if no master node is
         *         found for the identifier.
         */
        NodeID _findMasterNodeID( const UUID& id );

        NodePtr _connectMaster( const UUID& id );

        void _attachObject( Object* object, const UUID& id,
                            const uint32_t instanceID );
        void _detachObject( Object* object );

        /** The command handler functions. */
        bool _cmdFindMasterNodeID( ICommand& command );
        bool _cmdFindMasterNodeIDReply( ICommand& command );
        bool _cmdAttachObject( ICommand& command );
        bool _cmdDetachObject( ICommand& command );
        bool _cmdMapObject( ICommand& command );
        bool _cmdMapObjectSuccess( ICommand& command );
        bool _cmdMapObjectReply( ICommand& command );
        bool _cmdUnmapObject( ICommand& command );
        bool _cmdUnsubscribeObject( ICommand& command );
        bool _cmdInstance( ICommand& command );
        bool _cmdRegisterObject( ICommand& command );
        bool _cmdDeregisterObject( ICommand& command );
        bool _cmdDisableSendOnRegister( ICommand& command );
        bool _cmdRemoveNode( ICommand& command );
        bool _cmdObjectPush( ICommand& command );

        LB_TS_VAR( _receiverThread );
        LB_TS_VAR( _commandThread );
    };

    std::ostream& operator << ( std::ostream& os, ObjectStore* objectStore );
}
#endif // CO_OBJECTSTORE_H
