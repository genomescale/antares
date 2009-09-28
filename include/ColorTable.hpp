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

#ifndef ANTARES_COLOR_TABLE_HPP_
#define ANTARES_COLOR_TABLE_HPP_

#include <stdint.h>
#include <vector>
#include <Base.h>
#include "SmartPtr.hpp"

class ColorTable {
  public:
    explicit ColorTable(int32_t id);

    ColorTable* clone() const;

    size_t size() const;

    const RGBColor& color(size_t index) const;
    void set_color(size_t index, const RGBColor& color);

  private:
    std::vector<RGBColor> _colors;

    DISALLOW_COPY_AND_ASSIGN(ColorTable);
};

#endif  // ANTARES_COLOR_TABLE_HPP_