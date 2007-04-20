
/* Copyright (c) 2006-2007, Stefan Eilemann <eile@equalizergraphics.com> 
   All rights reserved. */

#ifndef EQNET_COMMAND_H
#define EQNET_COMMAND_H

#include <eq/net/node.h>

#include <eq/base/base.h>
#include <eq/base/refPtr.h>

namespace eqNet
{    
    struct Packet;

    /**
     * A class managing commands and the ownership of packets.
     *
     * A RefPtr<Packet> can't be used, since Packets are plain C structs send
     * over the network. The Command manages the ownership of packet by
     * claiming them so that only one can hold a given packet at a time.
     * A packet passed to a holder is owned by it and will be deleted
     * automatically when necessary.
     */
    class Command 
    {
    public:
        Command() : _packet(0)   {}
        Command( const Command& from ); // makes copy of _packet
        ~Command() { release(); }
        
        Packet*       getPacket()              { return _packet; }
        const Packet* getPacket() const        { return _packet; }

        template< class P > P* getPacket()
            { EQASSERT( _packet ); return reinterpret_cast<P*>( _packet ); }
        template< class P > const P* getPacket() const
            { EQASSERT( _packet ); return reinterpret_cast<P*>( _packet ); }

        eqBase::RefPtr<Node> getNode()      const { return _node; }
        eqBase::RefPtr<Node> getLocalNode() const { return _localNode; }

        Command& operator = ( Command& rhs ); // transfers _packet
        bool     operator ! () const     { return ( _packet==0 ); }

        Packet*       operator->()       { EQASSERT(_packet); return _packet; }
        const Packet* operator->() const { EQASSERT(_packet); return _packet; }

        void allocate( eqBase::RefPtr<Node> node, 
                       eqBase::RefPtr<Node> localNode, 
                       const uint64_t packetSize );
        void release();
        bool isValid() const { return ( _packet!=0 ); }

    private:
        eqBase::RefPtr<Node> _node;
        eqBase::RefPtr<Node> _localNode;
        Packet*              _packet;

    };

    EQ_EXPORT std::ostream& operator << ( std::ostream& os, const Command& );
}

#endif // EQNET_COMMAND_H
