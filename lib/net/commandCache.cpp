
/* Copyright (c) 2006, Stefan Eilemann <eile@equalizergraphics.com> 
   All rights reserved. */

#include "commandCache.h"

#include "command.h"
#include "node.h"
#include "packets.h"

using namespace eqNet;

CommandCache::CommandCache()
{}

CommandCache::~CommandCache()
{
    while( !_freeCommands.empty( ))
    {
        Command* command = _freeCommands.front();
        _freeCommands.pop_front();
        delete command;
    }
}

Command* CommandCache::alloc( Command& inCommand )
{
    Command* outCommand;

    if( _freeCommands.empty( ))
        outCommand = new Command;
    else
    {
        outCommand = _freeCommands.front();
        _freeCommands.pop_front();
    }

    *outCommand = inCommand;
    return outCommand;
}

void CommandCache::release( Command* command )
{
    EQASSERT( command->isValid( ));

    if( (*command)->exceedsMinSize( )) // old packet was 'big', release
        command->release();
    
    _freeCommands.push_back( command );
}

