/*
Ares, a tactical space combat game.
Copyright (C) 1997, 1999-2001, 2008 Nathan Lamont

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
*/

#ifndef ANTARES_NATEWORKING_HPP_
#define ANTARES_NATEWORKING_HPP_

// Natewerking.h

#include <appletalk.h>

#define BUFFER_SIZE             700

#define BUFFER_QUEUE_SIZE       30

#define TUPLE_SIZE              104
#define MATCH_NUM               10

#define FREE_QUEUE_OFFSET       20
#define USED_QUEUE_OFFSET       24
#define WRITE_DATA_STRUCT_SIZE  14L
#define DATAGRAM_HEADER_SIZE    17L

#define SOCKET_RES_ID           128
#define SOCKET_RES_TYPE         'CODE'

#define NBP_INTERVAL            0x0f
#define NBP_COUNT               0x03


struct nateQElement
{
    Ptr                 data;
    struct nateQElement *nextElement;
};

typedef struct nateQElement nateQElement;

typedef struct
{
    nateQElement    *firstElement;
    nateQElement    *lastElement;
} nateQueue;

char NatewerkInit( void);
void NatewerkDispose( char);
void NatewerkRemoveName( StringPtr, StringPtr);
Boolean NatewerkPostName( char, StringPtr, StringPtr);
int NatewerkGetName( AddrBlock *, StringPtr, StringPtr, short);
OSErr NatewerkLookupName( EntityName *, int *, Ptr);
int NatewerkSendData( AddrBlock, char, Ptr, short);
void PopTopUsedQueue( void);
Ptr GetTopUsedQueue( void);

#endif // ANTARES_NATEWORKING_HPP_