// Ares, a tactical space combat game.
// Copyright (C) 1997, 1999-2001, 2008 Nathan Lamont
// 
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
// 
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
// 
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

#include "InputSource.hpp"

#include <assert.h>

InputSource::~InputSource() { }

ReplayInputSource::ReplayInputSource(int32_t id)
        : _resource('NLRP', id),
          _bin(_resource.data(), _resource.size()) {
    _bin.read(&_random_seed);
    _bin.read(&_turn_num);
    _bin.read(&_keys);
}

uint32_t ReplayInputSource::random_seed() const {
    return _random_seed;
}

bool ReplayInputSource::next(uint32_t* key_map) {
    while (_turn_num == 0) {
        if (_bin.bytes_read() == _resource.size()) {
            return false;
        }
        _bin.read(&_turn_num);
        _bin.read(&_keys);
        ++_turn_num;
    }
    --_turn_num;
    *key_map = _keys;
    return true;
}