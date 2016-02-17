// Copyright (C) 1997, 1999-2001, 2008 Nathan Lamont
// Copyright (C) 2015 The Antares Authors
//
// This file is part of Antares, a tactical space combat game.
//
// Antares is free software: you can redistribute it and/or modify it
// under the terms of the Lesser GNU General Public License as published
// by the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// Antares is distributed in the hope that it will be useful, but
// WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
// Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public
// License along with Antares.  If not, see http://www.gnu.org/licenses/

#ifndef ANTARES_GLFW_VIDEO_DRIVER_HPP_
#define ANTARES_GLFW_VIDEO_DRIVER_HPP_

#include <queue>
#include <stack>
#include <sfz/sfz.hpp>

#include "config/keys.hpp"
#include "drawing/color.hpp"
#include "math/geometry.hpp"
#include "ui/event-tracker.hpp"
#include "video/opengl-driver.hpp"

struct GLFWwindow;

namespace antares {

class Event;

class GLFWVideoDriver : public OpenGlVideoDriver {
  public:
    GLFWVideoDriver();
    virtual ~GLFWVideoDriver();

    virtual Size viewport_size() const { return _viewport_size; }
    virtual Size screen_size() const { return _screen_size; }

    virtual bool button(int which);
    virtual Point get_mouse();
    virtual void get_keys(KeyMap* k);
    virtual InputMode input_mode() const;

    virtual int ticks() const;
    virtual int usecs() const;

    void loop(Card* initial);

  private:
    void key(int key, int scancode, int action, int mods);
    void mouse_button(int button, int action, int mods);
    void mouse_move(double x, double y);
    static void key_callback(GLFWwindow* w, int key, int scancode, int action, int mods);
    static void mouse_button_callback(GLFWwindow* w, int button, int action, int mods);
    static void mouse_move_callback(GLFWwindow* w, double x, double y);

    const Size _screen_size;
    Size _viewport_size;
    GLFWwindow* _window;
    MainLoop* _loop;
    int64_t _last_click_usecs;
    int _last_click_count;

    DISALLOW_COPY_AND_ASSIGN(GLFWVideoDriver);
};

}  // namespace antares

#endif  // ANTARES_GLFW_VIDEO_DRIVER_HPP_
