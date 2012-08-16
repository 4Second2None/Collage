
/* Copyright (c) 2012, Daniel Nachbaur <danielnachbaur@gmail.com>
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

#ifndef CO_QUEUEITEM_H
#define CO_QUEUEITEM_H

#include <co/dataOStream.h> // base class


namespace co
{
class QueueMaster;
namespace detail { class QueueItem; }

/**
 * The item of the distributed queue holding the data.
 *
 * Each enqueued item generated by QueueMaster::push() can be enhanced with
 * additional data in a stream-like manner. On the consumer side, the data can
 * be retrieved via the ObjectICommand.
 */
class QueueItem : public DataOStream
{
public:
    /** Destruct this queue item. @version 1.0 */
    CO_API ~QueueItem();

private:
    friend class QueueMaster;

    /** @internal Construct a new queue item. @version 1.0 */
    QueueItem( QueueMaster& master );

    /** @internal Copy-construct a new queue item. @version 1.0 */
    QueueItem( const QueueItem& rhs );

    virtual void sendData( const void*, const uint64_t, const bool )
        { LBDONTCALL }

    detail::QueueItem* const _impl;
};

}

#endif // CO_QUEUEITEM_H
