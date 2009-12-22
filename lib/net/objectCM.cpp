
/* Copyright (c) 2007-2009, Stefan Eilemann <eile@equalizergraphics.com> 
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

#include "objectCM.h"

#include "nullCM.h"
#include "node.h"

eq::net::ObjectCM* eq::net::ObjectCM::ZERO = new eq::net::NullCM;

#ifdef EQ_INSTRUMENT_MULTICAST
eq::base::a_int32_t eq::net::ObjectCM::_hit( 0 );
eq::base::a_int32_t eq::net::ObjectCM::_miss( 0 );
#endif
