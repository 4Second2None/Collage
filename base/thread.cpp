
/* Copyright (c) 2005, Stefan Eilemann <eile@equalizergraphics.com> 
   All rights reserved. */

#include "thread.h"
#include "base.h"
#include "lock.h"

#include <errno.h>
#include <pthread.h>
#include <strings.h>
#include <sys/wait.h>

using namespace eqBase;
using namespace std;

pthread_key_t Thread::_dataKey;
bool          Thread::_dataKeyCreated;

Thread::Thread( const Type type )
        : _type(type),
          _threadState(STATE_STOPPED)
{
    bzero( &_threadID, sizeof( ThreadID ));
    _lock = new Lock( type );
    _lock->set();
}

Thread::~Thread()
{
    delete _lock;
}

void* Thread::runChild( void* arg )
{
    Thread* thread = static_cast<Thread*>(arg);
    ASSERT( thread );
    thread->_runChild();
    return NULL; // not reached
}

void Thread::_runChild()
{
    _threadID      = _getLocalThreadID();
    _threadState   = STATE_RUNNING;
    ssize_t result = 0;

    if( init( ))
    {
        _lock->unset(); // sync w/ parent
        result = run();
        _threadState = STATE_STOPPING;
    }
    else
    {
        _threadState = STATE_STOPPED;
        _lock->unset();
    }

    switch( _type )
    {
        case PTHREAD:
            pthread_exit( (void*)result );
            break;

        case FORK:
            exit( result );
    }
}

// the signal handler for SIGCHILD
static void sigChildHandler( int /*signal*/ )
{
    //int status;
    //int pid = wait( &status );
    INFO << "Received SIGCHILD" << endl;
    signal( SIGCHLD, sigChildHandler );
}

bool Thread::start()
{
    if( _threadState != STATE_STOPPED )
        return false;

    _threadState = STATE_STARTING;

    switch( _type )
    {
        case PTHREAD:
        {
            pthread_attr_t attributes;
            pthread_attr_init( &attributes );
            pthread_attr_setscope( &attributes, PTHREAD_SCOPE_SYSTEM );

            int nTries = 10;
            while( nTries-- )
            {
                const int error = pthread_create( &_threadID.pthread,
                                                  &attributes, runChild, this );

                if( error == 0 ) // succeeded
                {
                    VERB << "Created pthread " << _threadID.pthread << endl;
                    break;
                }
                if( error != EAGAIN || nTries==0 )
                {
                    WARN << "Could not create thread: " << strerror( error )
                         << endl;
                    return false;
                }
            }
        } break;

        case FORK:
        {
            signal( SIGCHLD, sigChildHandler );
            const int result = fork();
            switch( result )
            {
                case 0: // child
                    VERB << "Child running" << endl;
                    _runChild(); 
                    return true; // not reached
            
                case -1: // error
                    WARN << "Could not fork child process:" 
                         << strerror( errno ) << endl;
                    return false;

                default: // parent
                    VERB << "Parent running" << endl;
                    _threadID.fork = result;
                    break;
            }
        } break;
    }

    _lock->set(); // sync with child's entry func
    // TODO: check if thread's initialised correctly. needs shmem for FORK.
    _threadState = STATE_RUNNING;
    return true;
}

void Thread::exit( ssize_t retVal )
{
    if( !isRunning( ))
        return;

    INFO << "Exiting thread" << endl;
    _threadState = STATE_STOPPING;

    switch( _type )
    {
        case PTHREAD:
            if( !pthread_equal( pthread_self(), _threadID.pthread ))
            {
                pthread_cancel( _threadID.pthread );
                return;
            }

            pthread_exit( (void*)retVal );
            break;

        case FORK:
            if( getpid() != _threadID.fork )
            {
                kill( _threadID.fork, SIGTERM );
                return;
            }
            
            ::exit( retVal );
            break;
    }
    ERROR << "Unreachable code" <<endl;
    ::abort();
}

bool Thread::join( ssize_t* retVal )
{
    if( _threadState == STATE_STOPPED )
        return false;

    switch( _type )
    {
        case PTHREAD:
        {
            VERB << "Joining pthread " << _threadID.pthread << endl;
            const int error = pthread_join( _threadID.pthread, (void**)_retVal);
            if( error != 0 )
            {
                WARN << "Error joining the thread: " << strerror(error) << endl;
                return false;
            }

            _threadState = STATE_STOPPED;
            if( retVal )
                *retVal = _retVal;
        } return true;

        case FORK:
            while( true )
            {
                int status;
                pid_t result = waitpid( _threadID.fork, &status, 0 );
                if( result == _threadID.fork )
                {
                    if( WIFEXITED( status ))
                    {
                        _retVal = WEXITSTATUS( status );
                        _threadState = STATE_STOPPED;

                        if( retVal )
                            *retVal = _retVal;
                        return true;
                    }
                    return false;
                }
                
                switch( errno )
                {
                    case EINTR:
                        break; // try again

                    default:
                        WARN << "Error joining the process: " 
                             << strerror(errno) << endl;
                        return false;
                }
            }
            return false;
    }
    return false;
}

Thread::ThreadID Thread::_getLocalThreadID()
{
    ThreadID threadID;

    switch( _type )
    {
        case PTHREAD:
            threadID.pthread = pthread_self();
            break;

        case FORK:
            threadID.fork = getpid();
            break;
    }

    return threadID;
}

bool Thread::_createDataKey()
{
    int nTries = 10;

    while( !_dataKeyCreated && nTries-- )
    {
        int result = pthread_key_create( &_dataKey, NULL );
        if( result == 0 ) // success
        {
            _dataKeyCreated = true;
            return true;
        }

        switch( result )
        {            
            case EAGAIN: // try again
                break;

            default:
                ERROR << "Can't create thread-specific data key: "
                      << strerror( result ) << endl;
                return false;
        }
    }

    ERROR << "Can't create thread-specific data key." << endl;
    return false;
}

void Thread::setSpecific( void* data )
{
    if( !_dataKeyCreated && !_createDataKey( ))
        return;

    pthread_setspecific( _dataKey, data );
}

void* Thread::getSpecific()
{
    if( !_dataKeyCreated && !_createDataKey( ))
        return NULL;

    return pthread_getspecific( _dataKey );
}
