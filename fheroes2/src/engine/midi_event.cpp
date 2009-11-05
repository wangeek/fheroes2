/***************************************************************************
 *   Copyright (C) 2008 by Andrey Afletdinov                               *
 *   afletdinov@mail.dc.baikal.ru                                          *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
 ***************************************************************************/

#include <cstring>
#include "midi_event.h"

using namespace MIDI;

Event::Event() : delta(0), status(0), sp(0)
{
}

Event::Event(const u32 dl, const u8 st,  const u32 sz, const u8 *p) : delta(dl), status(st), data(NULL)
{
    if(sz)
    {
	data = new u8 [sz];
	size = sz;
	memcpy(data, p, size);
    }

    SetDelta(dl);
}

Event::Event(const Event & e)
{
    delta = e.delta;
    status = e.status;

    data = NULL;
    size = e.size;

    if(size)
    {
	data = new u8 [size];
	memcpy(data, e.data, size);
    }

    memcpy(pack, e.pack, 4);
    sp = e.sp;
}

Event::~Event()
{
    if(data) delete [] data;
}

Event & Event::operator= (const Event & e)
{
    if(data) delete [] data;

    delta = e.delta;
    status = e.status;

    data = NULL;
    size = e.size;

    if(size)
    {
	data = new u8 [size];
	memcpy(data, e.data, size);
    }

    memcpy(pack, e.pack, 4);
    sp = e.sp;

    return *this;
}

void Event::SetDelta(const u32 dl)
{
    sp = MIDI::PackDelta(pack, dl);
}

u32 Event::Size(void) const
{
    return 1 + sp + size;
}

bool Event::Write(u8 *p) const
{
    if(NULL == p) return false;

    memcpy(p, pack, sp);
    p+= sp;

    *p = status;

    if(size) memcpy(p + 1, data, size);

    return true;
}

bool Event::Write(std::ostream & o) const
{
    if(o.fail()) return false;

    o.write(reinterpret_cast<const char*>(pack), sp);
    o.write(&status, 1);
    if(size) o.write(reinterpret_cast<const char*>(data), size);

    return true;
}

void Event::Dump(void) const
{
    printf("          [dl:%d:st:%hhX:dt", delta, status);

    for(u32 ii = 0; ii < size; ++ii) printf(":%hhX", data[ii]);

    printf("]\n");
}
