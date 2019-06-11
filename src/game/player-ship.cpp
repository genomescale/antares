// Copyright (C) 1997, 1999-2001, 2008 Nathan Lamont
// Copyright (C) 2008-2017 The Antares Authors
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

#include "game/player-ship.hpp"

#include <pn/output>
#include <sfz/sfz.hpp>

#include "config/gamepad.hpp"
#include "config/keys.hpp"
#include "config/preferences.hpp"
#include "data/base-object.hpp"
#include "data/resource.hpp"
#include "drawing/color.hpp"
#include "drawing/text.hpp"
#include "game/admiral.hpp"
#include "game/cheat.hpp"
#include "game/cursor.hpp"
#include "game/globals.hpp"
#include "game/input-source.hpp"
#include "game/instruments.hpp"
#include "game/labels.hpp"
#include "game/level.hpp"
#include "game/messages.hpp"
#include "game/minicomputer.hpp"
#include "game/non-player-ship.hpp"
#include "game/space-object.hpp"
#include "game/starfield.hpp"
#include "game/sys.hpp"
#include "game/time.hpp"
#include "lang/defines.hpp"
#include "math/fixed.hpp"
#include "math/macros.hpp"
#include "math/rotation.hpp"
#include "math/special.hpp"
#include "math/units.hpp"
#include "sound/fx.hpp"

namespace macroman = sfz::macroman;

namespace antares {

namespace {

// const int32_t kSendMessageVOffset = 20;
const int32_t kCursorBoundsSize = 16;  // should be same in instruments.c

}  // namespace

const static ticks kKlaxonInterval      = ticks(125);
const static ticks kDestKeyHoldDuration = ticks(45);
const static ticks kHotKeyHoldDuration  = ticks(51);  // Compatibility

int32_t HotKey_GetFromObject(Handle<SpaceObject> object);
void    Update_LabelStrings_ForHotKeyChange(void);

namespace {

enum DestKeyState {
    DEST_KEY_UP,       // up
    DEST_KEY_DOWN,     // down, and possibly usable for self-selection
    DEST_KEY_BLOCKED,  // down, but used for something else
};

enum HotKeyState {
    HOT_KEY_UP,
    HOT_KEY_SELECT,
    HOT_KEY_TARGET,
};

static ANTARES_GLOBAL DestKeyState gDestKeyState = DEST_KEY_UP;
static ANTARES_GLOBAL wall_time gDestKeyTime;

static ANTARES_GLOBAL HotKeyState gHotKeyState[10];
static ANTARES_GLOBAL wall_time gHotKeyTime[10];

static ANTARES_GLOBAL Zoom gPreviousZoomMode;

pn::string name_with_hot_key_suffix(Handle<SpaceObject> space_object) {
    int h = HotKey_GetFromObject(space_object);
    if (h < 0) {
        return space_object->long_name().copy();
    }

    Key keyNum = sys.prefs->key(h + kFirstHotKeyNum);
    if (keyNum == Key::NONE) {
        return space_object->long_name().copy();
    }

    return pn::format(
            "{0} < {1} >", space_object->long_name(),
            sys.key_long_names.at(static_cast<int>(keyNum)));
};

}  // namespace

bool PlayerEvent::operator==(PlayerEvent other) const {
    if (type != other.type) {
        return false;
    }
    switch (type) {
        case KEY_DOWN:
        case KEY_UP:
        case LONG_KEY_UP: return key == other.key;
    }
}

bool PlayerEvent::operator<(PlayerEvent other) const {
    if (type != other.type) {
        return type < other.type;
    }
    switch (type) {
        case KEY_DOWN:
        case KEY_UP:
        case LONG_KEY_UP: return key < other.key;
    }
}

void ResetPlayerShip() {
    g.control_label = Label::add(0, 0, 0, 10, SpaceObject::none(), true, Hue::YELLOW);
    g.target_label  = Label::add(0, 0, 0, -20, SpaceObject::none(), true, Hue::SKY_BLUE);
    g.send_label    = Label::add(200, 200, 0, 30, SpaceObject::none(), false, Hue::GREEN);
    globals()->starfield.reset();
    globals()->next_klaxon = game_ticks();
    g.key_mask             = 0;
    g.zoom                 = Zoom::FOE;
    gPreviousZoomMode      = Zoom::FOE;

    for (int h = 0; h < kHotKeyNum; h++) {
        globals()->hotKey[h].object   = SpaceObject::none();
        globals()->hotKey[h].objectID = -1;
    }
    for (auto& k : gHotKeyState) {
        k = HOT_KEY_UP;
    }
    gDestKeyState = DEST_KEY_UP;
}

PlayerShip::PlayerShip()
        : gTheseKeys(0),
          _gamepad_keys(0),
          _gamepad_state(NO_BUMPER),
          _control_active(false),
          _control_direction(0) {}

static sfz::optional<KeyNum> key_num(Key key) {
    for (int i = 0; i < kKeyExtendedControlNum; ++i) {
        if (key == sys.prefs->key(i)) {
            return sfz::make_optional(static_cast<KeyNum>(i));
        }
    }
    return sfz::nullopt;
}

static void zoom_to(Zoom zoom) {
    if (g.zoom != zoom) {
        g.zoom = zoom;
        sys.sound.click();
        Messages::zoom(g.zoom);
    }
}

static void zoom_shortcut(Zoom zoom) {
    if (g.key_mask & kShortcutZoomMask) {
        return;
    }
    Zoom previous     = gPreviousZoomMode;
    gPreviousZoomMode = g.zoom;
    if (g.zoom == zoom) {
        zoom_to(previous);
    } else {
        zoom_to(zoom);
    }
}

static void zoom_in() {
    if (g.key_mask & kZoomInKey) {
        return;
    }
    if (g.zoom > Zoom::DOUBLE) {
        zoom_to(static_cast<Zoom>(static_cast<int>(g.zoom) - 1));
    }
}

static void zoom_out() {
    if (g.key_mask & kZoomOutKey) {
        return;
    }
    if (g.zoom < Zoom::ALL) {
        zoom_to(static_cast<Zoom>(static_cast<int>(g.zoom) + 1));
    }
}

static void engage_autopilot() {
    auto player = g.ship;
    if (!(player->attributes & kOnAutoPilot)) {
        player->keysDown |= kAutoPilotKey;
    }
    player->keysDown |= kAdoptTargetKey;
}

static void pick_object(
        Handle<SpaceObject> origin_ship, int32_t direction, bool destination, int32_t attributes,
        int32_t nonattributes, Handle<SpaceObject> select_ship, Allegiance allegiance) {
    uint64_t huge_distance;
    if (select_ship.get()) {
        uint32_t difference = ABS<int>(origin_ship->location.h - select_ship->location.h);
        uint32_t dcalc      = difference;
        difference          = ABS<int>(origin_ship->location.v - select_ship->location.v);
        uint32_t distance   = difference;

        if ((dcalc > kMaximumRelevantDistance) || (distance > kMaximumRelevantDistance)) {
            huge_distance = dcalc;  // must be positive
            MyWideMul(huge_distance, huge_distance, &huge_distance);
            select_ship->distanceFromPlayer = distance;
            MyWideMul(
                    select_ship->distanceFromPlayer, select_ship->distanceFromPlayer,
                    &select_ship->distanceFromPlayer);
            select_ship->distanceFromPlayer += huge_distance;
        } else {
            select_ship->distanceFromPlayer = distance * distance + dcalc * dcalc;
        }
        huge_distance = select_ship->distanceFromPlayer;
    } else {
        huge_distance = 0;
    }

    select_ship = GetManualSelectObject(
            origin_ship, direction, attributes, nonattributes, &huge_distance, select_ship,
            allegiance);

    if (select_ship.get()) {
        if (destination) {
            SetPlayerSelectShip(select_ship, true, g.admiral);
        } else {
            SetPlayerSelectShip(select_ship, false, g.admiral);
        }
    }
}

static void select_friendly(Handle<SpaceObject> origin_ship, int32_t direction) {
    pick_object(
            origin_ship, direction, false, kCanBeDestination, kIsDestination, g.admiral->control(),
            FRIENDLY);
}

static void target_friendly(Handle<SpaceObject> origin_ship, int32_t direction) {
    pick_object(
            origin_ship, direction, true, kCanBeDestination, kIsDestination, g.admiral->target(),
            FRIENDLY);
}

static void target_hostile(Handle<SpaceObject> origin_ship, int32_t direction) {
    pick_object(
            origin_ship, direction, true, kCanBeDestination, kIsDestination, g.admiral->target(),
            HOSTILE);
}

static void select_base(Handle<SpaceObject> origin_ship, int32_t direction) {
    pick_object(origin_ship, direction, false, kIsDestination, 0, g.admiral->control(), FRIENDLY);
}

static void target_base(Handle<SpaceObject> origin_ship, int32_t direction) {
    pick_object(
            origin_ship, direction, true, kIsDestination, 0, g.admiral->target(),
            FRIENDLY_OR_HOSTILE);
}

static void target_self() { SetPlayerSelectShip(g.ship, true, g.admiral); }

void PlayerShip::key_down(const KeyDownEvent& event) {
    _keys.set(event.key(), true);

    if (!active()) {
        return;
    }

    sfz::optional<KeyNum> key = key_num(event.key());
    if (key.has_value()) {
        switch (*key) {
            case kHotKey1Num: gHotKeyTime[0] = now(); break;
            case kHotKey2Num: gHotKeyTime[1] = now(); break;
            case kHotKey3Num: gHotKeyTime[2] = now(); break;
            case kHotKey4Num: gHotKeyTime[3] = now(); break;
            case kHotKey5Num: gHotKeyTime[4] = now(); break;
            case kHotKey6Num: gHotKeyTime[5] = now(); break;
            case kHotKey7Num: gHotKeyTime[6] = now(); break;
            case kHotKey8Num: gHotKeyTime[7] = now(); break;
            case kHotKey9Num: gHotKeyTime[8] = now(); break;
            case kHotKey10Num: gHotKeyTime[9] = now(); break;
            case kDestinationKeyNum: gDestKeyTime = now(); break;
            default: break;
        }
        _player_events.push_back(PlayerEvent::key_down(*key));
    }
}

void PlayerShip::key_up(const KeyUpEvent& event) {
    _keys.set(event.key(), false);

    if (!active()) {
        return;
    }

    sfz::optional<KeyNum> key = key_num(event.key());
    if (key.has_value()) {
        bool long_hold = false;
        switch (*key) {
            case kHotKey1Num: long_hold = (now() >= gHotKeyTime[0] + kHotKeyHoldDuration); break;
            case kHotKey2Num: long_hold = (now() >= gHotKeyTime[1] + kHotKeyHoldDuration); break;
            case kHotKey3Num: long_hold = (now() >= gHotKeyTime[2] + kHotKeyHoldDuration); break;
            case kHotKey4Num: long_hold = (now() >= gHotKeyTime[3] + kHotKeyHoldDuration); break;
            case kHotKey5Num: long_hold = (now() >= gHotKeyTime[4] + kHotKeyHoldDuration); break;
            case kHotKey6Num: long_hold = (now() >= gHotKeyTime[5] + kHotKeyHoldDuration); break;
            case kHotKey7Num: long_hold = (now() >= gHotKeyTime[6] + kHotKeyHoldDuration); break;
            case kHotKey8Num: long_hold = (now() >= gHotKeyTime[7] + kHotKeyHoldDuration); break;
            case kHotKey9Num: long_hold = (now() >= gHotKeyTime[8] + kHotKeyHoldDuration); break;
            case kHotKey10Num: long_hold = (now() >= gHotKeyTime[9] + kHotKeyHoldDuration); break;
            case kDestinationKeyNum:
                long_hold = (now() >= (gDestKeyTime + kDestKeyHoldDuration));
                break;
            default: break;
        }
        if (long_hold) {
            _player_events.push_back(PlayerEvent::long_key_up(*key));
        } else {
            _player_events.push_back(PlayerEvent::key_up(*key));
        }
    }
}

void PlayerShip::mouse_down(const MouseDownEvent& event) {
    _cursor.mouse_down(event);

    Point where = event.where();
    switch (event.button()) {
        case 0:
            if (event.count() == 2) {
                PlayerShipHandleClick(where, 0);
                MiniComputerHandleDoubleClick(where);
            } else if (event.count() == 1) {
                PlayerShipHandleClick(where, 0);
                MiniComputerHandleClick(where);
            }
            break;
        case 1:
            if (event.count() == 1) {
                PlayerShipHandleClick(where, 1);
            }
            break;
    }
}

void PlayerShip::mouse_up(const MouseUpEvent& event) {
    _cursor.mouse_up(event);

    Point where = event.where();
    if (event.button() == 0) {
        MiniComputerHandleMouseStillDown(where);
        MiniComputerHandleMouseUp(where);
    }
}

void PlayerShip::mouse_move(const MouseMoveEvent& event) { _cursor.mouse_move(event); }

void PlayerShip::gamepad_button_down(const GamepadButtonDownEvent& event) {
    switch (event.button) {
        case Gamepad::Button::LB:
            if (_gamepad_state & SELECT_BUMPER) {
                _gamepad_state = TARGET_BUMPER_OVERRIDE;
            } else if (!(_gamepad_state & TARGET_BUMPER)) {
                _gamepad_state = TARGET_BUMPER;
            }
            return;
        case Gamepad::Button::RB:
            if (_gamepad_state & TARGET_BUMPER) {
                _gamepad_state = SELECT_BUMPER_OVERRIDE;
            } else if (!(_gamepad_state & SELECT_BUMPER)) {
                _gamepad_state = SELECT_BUMPER;
            }
            return;
        default: break;
    }

    if (!active()) {
        return;
    }

    auto player = g.ship;
    if (_gamepad_state) {
        switch (event.button) {
            case Gamepad::Button::A:
                if (_control_active) {
                    if (_gamepad_state & SELECT_BUMPER) {
                        select_friendly(player, _control_direction);
                    } else {
                        target_friendly(player, _control_direction);
                    }
                }
                return;
            case Gamepad::Button::B:
                if (_control_active) {
                    if (_gamepad_state & TARGET_BUMPER) {
                        target_hostile(player, _control_direction);
                    }
                }
                return;
            case Gamepad::Button::X:
                if (_control_active) {
                    if (_gamepad_state & SELECT_BUMPER) {
                        select_base(player, _control_direction);
                    } else {
                        target_base(player, _control_direction);
                    }
                }
                return;
            case Gamepad::Button::Y:
                if (_gamepad_state & SELECT_BUMPER) {
                    _player_events.push_back(PlayerEvent::key_down(kOrderKeyNum));
                } else {
                    _player_events.push_back(PlayerEvent::key_down(kAutoPilotKeyNum));
                }
                return;
            case Gamepad::Button::LSB:
                if (_gamepad_state & TARGET_BUMPER) {
                    target_self();
                } else {
                    transfer_control(g.admiral);
                }
                return;
            default: break;
        }
    }

    switch (event.button) {
        case Gamepad::Button::A: _gamepad_keys |= kUpKey; break;
        case Gamepad::Button::B: _gamepad_keys |= kDownKey; break;
        case Gamepad::Button::X: zoom_out(); break;
        case Gamepad::Button::Y: zoom_in(); break;
        case Gamepad::Button::BACK: Messages::advance(); break;
        case Gamepad::Button::LT: _gamepad_keys |= kSpecialKey; break;
        case Gamepad::Button::RT: _gamepad_keys |= (kPulseKey | kBeamKey); break;
        case Gamepad::Button::LSB:
            if (player->presenceState == kWarpingPresence) {
                _gamepad_keys &= !kWarpKey;
            } else {
                _gamepad_keys |= kWarpKey;
            }
            break;
        case Gamepad::Button::UP:
            minicomputer_handle_keys({PlayerEvent::key_down(kCompUpKeyNum)});
            break;
        case Gamepad::Button::DOWN:
            minicomputer_handle_keys({PlayerEvent::key_down(kCompDownKeyNum)});
            break;
        case Gamepad::Button::RIGHT:
            minicomputer_handle_keys({PlayerEvent::key_down(kCompAcceptKeyNum)});
            break;
        case Gamepad::Button::LEFT:
            minicomputer_handle_keys({PlayerEvent::key_down(kCompCancelKeyNum)});
            break;
        default: break;
    }
}

void PlayerShip::gamepad_button_up(const GamepadButtonUpEvent& event) {
    switch (event.button) {
        case Gamepad::Button::LB:
            _player_events.push_back(PlayerEvent::key_up(kOrderKeyNum));
            if (_gamepad_state & OVERRIDE) {
                _gamepad_state = SELECT_BUMPER;
            } else {
                _gamepad_state = NO_BUMPER;
            }
            return;
        case Gamepad::Button::RB:
            _player_events.push_back(PlayerEvent::key_up(kAutoPilotKeyNum));
            if (_gamepad_state & OVERRIDE) {
                _gamepad_state = TARGET_BUMPER;
            } else {
                _gamepad_state = NO_BUMPER;
            }
            return;
        default: break;
    }

    if (!active()) {
        return;
    }

    if (_gamepad_state) {
        switch (event.button) {
            case Gamepad::Button::A:
            case Gamepad::Button::B:
            case Gamepad::Button::X:
            case Gamepad::Button::LSB: return;
            case Gamepad::Button::Y:
                _player_events.push_back(PlayerEvent::key_up(kOrderKeyNum));
                _player_events.push_back(PlayerEvent::key_up(kAutoPilotKeyNum));
                return;
            default: break;
        }
    }

    auto player = g.ship;
    switch (event.button) {
        case Gamepad::Button::A: _gamepad_keys &= ~kUpKey; break;
        case Gamepad::Button::B: _gamepad_keys &= ~kDownKey; break;
        case Gamepad::Button::LT: _gamepad_keys &= ~kSpecialKey; break;
        case Gamepad::Button::RT: _gamepad_keys &= ~(kPulseKey | kBeamKey); break;
        case Gamepad::Button::LSB:
            if (player->presenceState != kWarpingPresence) {
                _gamepad_keys &= !kWarpKey;
            }
            break;
        case Gamepad::Button::RIGHT:
            minicomputer_handle_keys({PlayerEvent::key_up(kCompAcceptKeyNum)});
            break;
        case Gamepad::Button::LEFT:
            minicomputer_handle_keys({PlayerEvent::key_up(kCompCancelKeyNum)});
            break;
        default: break;
    }
}

void PlayerShip::gamepad_stick(const GamepadStickEvent& event) {
    bool active;
    int  direction = 0;
    if ((event.x * event.x + event.y * event.y) < 0.30) {
        active = false;
    } else {
        active    = true;
        direction = GetAngleFromVector(event.x * 32768, event.y * 32768);
        mAddAngle(direction, ROT_180);
    }
    switch (event.stick) {
        case Gamepad::Stick::LS:
            _control_active    = active;
            _control_direction = direction;
            break;
        default: break;
    }
}

bool PlayerShip::active() const {
    auto player = g.ship;
    return player.get() && player->active && (player->attributes & kIsPlayerShip);
}

static void handle_destination_key(const std::vector<PlayerEvent>& player_events) {
    for (const auto& e : player_events) {
        switch (e.type) {
            case PlayerEvent::KEY_DOWN:
                if (e.key == kDestinationKeyNum) {
                    gDestKeyState = DEST_KEY_DOWN;
                }
                break;

            case PlayerEvent::KEY_UP:
                if (e.key == kDestinationKeyNum) {
                    gDestKeyState = DEST_KEY_UP;
                }
                break;

            case PlayerEvent::LONG_KEY_UP:
                if (e.key == kDestinationKeyNum) {
                    if ((gDestKeyState == DEST_KEY_DOWN) &&
                        (g.ship->attributes & kCanBeDestination)) {
                        target_self();
                    }
                    gDestKeyState = DEST_KEY_UP;
                }
                break;
        }
    }
}

static int hot_key_index(const PlayerEvent& e) {
    switch (e.type) {
        case PlayerEvent::KEY_DOWN:
        case PlayerEvent::KEY_UP:
        case PlayerEvent::LONG_KEY_UP:
            if ((kHotKey1Num <= e.key) && (e.key <= kHotKey10Num)) {
                return e.key - kFirstHotKeyNum;
            }
            break;
    }
    return -1;
}

static void handle_hotkeys(const std::vector<PlayerEvent>& player_events) {
    for (const auto& e : player_events) {
        int i = hot_key_index(e);
        if (i < 0) {
            continue;
        }

        switch (e.type) {
            case PlayerEvent::KEY_DOWN:
                if (gDestKeyState == DEST_KEY_UP) {
                    gHotKeyState[i] = HOT_KEY_SELECT;
                } else {
                    gHotKeyState[i] = HOT_KEY_TARGET;
                }
                break;

            case PlayerEvent::KEY_UP:
                gHotKeyState[i] = HOT_KEY_UP;
                if (globals()->hotKey[i].object.get()) {
                    bool target     = (gHotKeyState[i] == HOT_KEY_TARGET);
                    auto selectShip = globals()->hotKey[i].object;
                    if ((selectShip->active) &&
                        (selectShip->id == globals()->hotKey[i].objectID)) {
                        bool is_target = (gDestKeyState != DEST_KEY_UP) ||
                                         (selectShip->owner != g.admiral) || target;
                        SetPlayerSelectShip(globals()->hotKey[i].object, is_target, g.admiral);
                    } else {
                        globals()->hotKey[i].object = SpaceObject::none();
                    }
                    if (gDestKeyState == DEST_KEY_DOWN) {
                        gDestKeyState = DEST_KEY_BLOCKED;
                    }
                }
                break;

            case PlayerEvent::LONG_KEY_UP:
                gHotKeyState[i] = HOT_KEY_UP;
                if (globals()->lastSelectedObject.get()) {
                    auto selectShip = globals()->lastSelectedObject;
                    if (selectShip->active) {
                        globals()->hotKey[i].object   = globals()->lastSelectedObject;
                        globals()->hotKey[i].objectID = globals()->lastSelectedObjectID;
                        Update_LabelStrings_ForHotKeyChange();
                        sys.sound.select();
                    }
                }
                break;
        }
    }
}

static void handle_target_keys(const std::vector<PlayerEvent>& player_events) {
    // for this we check lastKeys against theseKeys & relevent keys now being pressed
    for (const auto& e : player_events) {
        if (e.type != PlayerEvent::KEY_DOWN) {
            continue;
        }
        switch (e.key) {
            case kSelectFriendKeyNum:
                if (gDestKeyState == DEST_KEY_UP) {
                    select_friendly(g.ship, g.ship->direction);
                } else {
                    target_friendly(g.ship, g.ship->direction);
                }
                break;

            case kSelectFoeKeyNum: target_hostile(g.ship, g.ship->direction); break;

            case kSelectBaseKeyNum:
                if (gDestKeyState == DEST_KEY_UP) {
                    select_base(g.ship, g.ship->direction);
                } else {
                    target_base(g.ship, g.ship->direction);
                }
                break;

            default: continue;
        }
        if (gDestKeyState == DEST_KEY_DOWN) {
            gDestKeyState = DEST_KEY_BLOCKED;
        }
    }
}

static void handle_pilot_keys(
        Handle<SpaceObject> flagship, int32_t these_keys, int32_t gamepad_keys,
        bool gamepad_control, int32_t gamepad_control_direction) {
    if (flagship->attributes & kOnAutoPilot) {
        if ((these_keys | gamepad_keys) & (kUpKey | kDownKey | kLeftKey | kRightKey)) {
            flagship->keysDown = these_keys | kAutoPilotKey;
        }
    } else {
        flagship->keysDown = these_keys | gamepad_keys;
        if (gamepad_control) {
            int difference = mAngleDifference(gamepad_control_direction, flagship->direction);
            if (abs(difference) < 15) {
                // pass
            } else if (difference < 0) {
                flagship->keysDown |= kRightKey;
            } else {
                flagship->keysDown |= kLeftKey;
            }
        }
    }
}

static void handle_order_key(const std::vector<PlayerEvent>& player_events) {
    for (const auto& e : player_events) {
        if ((e.type == PlayerEvent::KEY_DOWN) && (e.key == kOrderKeyNum)) {
            g.ship->keysDown |= kGiveCommandKey;
        }
    }
}

static void handle_autopilot_keys(const std::vector<PlayerEvent>& player_events) {
    for (const auto& e : player_events) {
        if ((e.type == PlayerEvent::KEY_DOWN) && (e.key == kWarpKeyNum) &&
            (gDestKeyState != DEST_KEY_UP)) {
            engage_autopilot();
            g.ship->keysDown &= ~kWarpKey;
            gDestKeyState = DEST_KEY_BLOCKED;
        }
    }
}

void PlayerShip::update(bool enter_message) {
    if (!g.ship.get()) {
        return;
    }

    if (enter_message) {
        _player_events.clear();
        for (KeyNum k = kUpKeyNum; k < KEY_COUNT; k = static_cast<KeyNum>(k + 1)) {
            if (gTheseKeys & (1 << k)) {
                _player_events.push_back(PlayerEvent::key_up(k));
            }
        }
    }

    for (auto e : _player_events) {
        switch (e.type) {
            case PlayerEvent::KEY_DOWN:
                switch (e.key) {
                    case kZoomOutKeyNum: zoom_out(); continue;
                    case kZoomInKeyNum: zoom_in(); continue;
                    case kScale121KeyNum: zoom_shortcut(Zoom::ACTUAL); continue;
                    case kScale122KeyNum: zoom_shortcut(Zoom::DOUBLE); continue;
                    case kScale124KeyNum: zoom_shortcut(Zoom::QUARTER); continue;
                    case kScale1216KeyNum: zoom_shortcut(Zoom::SIXTEENTH); continue;
                    case kScaleHostileKeyNum: zoom_shortcut(Zoom::FOE); continue;
                    case kScaleObjectKeyNum: zoom_shortcut(Zoom::OBJECT); continue;
                    case kScaleAllKeyNum: zoom_shortcut(Zoom::ALL); continue;
                    case kTransferKeyNum: transfer_control(g.admiral); continue;
                    case kMessageNextKeyNum: Messages::advance(); continue;
                    default: break;
                }
                if (e.key < kKeyControlNum) {
                    gTheseKeys |= ((1 << e.key) & ~g.key_mask);
                }
                break;

            case PlayerEvent::KEY_UP:
            case PlayerEvent::LONG_KEY_UP:
                if (e.key < kKeyControlNum) {
                    gTheseKeys &= ~((1 << e.key) & ~g.key_mask);
                }
                break;
        }
    }

    /*
    while ((globals()->gKeyMapBufferBottom != globals()->gKeyMapBufferTop)) {
        bufMap = globals()->gKeyMapBuffer + globals()->gKeyMapBufferBottom;
        globals()->gKeyMapBufferBottom++;
        if (globals()->gKeyMapBufferBottom >= kKeyMapBufferNum) {
            globals()->gKeyMapBufferBottom = 0;
        }
        if (*enterMessage) {
            String* message = Label::get_string(g.send_label);
            if (message->empty()) {
                message->assign("<>");
            }
            if ((mReturnKey(*bufMap)) && (!AnyKeyButThisOne(*bufMap, Keys::RETURN))) {
                *enterMessage = false;
                StringSlice sliced = message->slice(1, message->size() - 2);
                int cheat = GetCheatNumFromString(sliced);
                if (cheat > 0) {
                    ExecuteCheat(cheat, g.admiral);
                } else if (!sliced.empty()) {
                    if (globals()->gActiveCheats[g.admiral] & kNameObjectBit)
                    {
                        SetAdmiralBuildAtName(g.admiral, sliced);
                        globals()->gActiveCheats[g.admiral] &= ~kNameObjectBit;
                    }
                }
                Label::set_position(
                        g.send_label,
                        viewport.left + ((viewport.width() / 2)),
                        viewport.top + ((play_screen.height() / 2)) +
                        kSendMessageVOffset);
                Label::recalc_size(g.send_label);
            } else {
                if ((mDeleteKey(*bufMap)) || (mLeftArrowKey(*bufMap))) {
                    if (message->size() > 2) {
                        if (mCommandKey(*bufMap)) {
                            // delete whole message
                            message->assign("<>");
                        } else {
                            message->resize(message->size() - 2);
                            message->append(1, '>');
                        }
                    }
                } else {
                    if (message->size() < 120) {
                        uint8_t ch = GetAsciiFromKeyMap(*bufMap, globals()->gLastMessageKeyMap);
                        if (ch) {
                            message->resize(message->size() - 1);
                            String s(macroman::decode(sfz::BytesSlice(&ch, 1)));
                            message->append(s);
                            message->append(1, '>');
                        }
                    }
                }
                width = sys.fonts.tactical->string_width(*message);
                strlen = viewport.left + ((viewport.width() / 2) - (width / 2));
                if ((strlen + width) > (viewport.right))
                {
                    strlen -= (strlen + width) - (viewport.right);
                }
                Label::recalc_size(g.send_label);
                Label::set_position(g.send_label, strlen, viewport.top +
                    ((play_screen.height() / 2) + kSendMessageVOffset));
            }
        } else {
            if ((mReturnKey(*bufMap)) && (!(g.key_mask & kReturnKeyMask))) {
                *enterMessage = true;
            }
        }
        globals()->gLastMessageKeyMap.copy(*bufMap);
    }
    */

    if (!g.ship->active) {
        return;
    }

    if (g.ship->health() < (g.ship->base->health >> 2L)) {
        if (g.time > globals()->next_klaxon) {
            if (globals()->next_klaxon == game_ticks()) {
                sys.sound.loud_klaxon();
            } else {
                sys.sound.klaxon();
            }
            Messages::shields_low();
            globals()->next_klaxon = g.time + kKlaxonInterval;
        }
    } else {
        globals()->next_klaxon = game_ticks();
    }

    if (!(g.ship->attributes & kIsPlayerShip)) {
        return;
    }

    Handle<SpaceObject> flagship = g.ship;  // Pilot same ship even after minicomputer transfer.
    minicomputer_handle_keys(_player_events);
    handle_destination_key(_player_events);
    handle_hotkeys(_player_events);
    if (!_cursor.active()) {
        handle_target_keys(_player_events);
    }
    handle_pilot_keys(
            flagship, gTheseKeys, _gamepad_keys, (_gamepad_state == NO_BUMPER) && _control_active,
            _control_direction);
    handle_order_key(_player_events);
    handle_autopilot_keys(_player_events);

    _player_events.clear();
}

bool PlayerShip::show_select() const {
    return _control_active && (_gamepad_state & SELECT_BUMPER);
}

bool PlayerShip::show_target() const {
    return _control_active && (_gamepad_state & TARGET_BUMPER);
}

int32_t PlayerShip::control_direction() const { return _control_direction; }

void PlayerShipHandleClick(Point where, int button) {
    if (g.key_mask & kMouseMask) {
        return;
    }

    if (g.ship.get()) {
        if ((g.ship->active) && (g.ship->attributes & kIsPlayerShip)) {
            Rect bounds = {
                    where.h - kCursorBoundsSize,
                    where.v - kCursorBoundsSize,
                    where.h + kCursorBoundsSize,
                    where.v + kCursorBoundsSize,
            };

            if ((gDestKeyState != DEST_KEY_UP) || (button == 1)) {
                auto target        = g.admiral->target();
                auto selectShipNum = GetSpritePointSelectObject(
                        &bounds, g.ship, kCanBeDestination | kIsDestination, target,
                        FRIENDLY_OR_HOSTILE);
                if (selectShipNum.get()) {
                    SetPlayerSelectShip(selectShipNum, true, g.admiral);
                }
            } else {
                auto control       = g.admiral->control();
                auto selectShipNum = GetSpritePointSelectObject(
                        &bounds, g.ship, kCanBeDestination | kIsDestination, control, FRIENDLY);
                if (selectShipNum.get()) {
                    SetPlayerSelectShip(selectShipNum, false, g.admiral);
                }
            }
        }
    }
    if (gDestKeyState == DEST_KEY_DOWN) {
        gDestKeyState = DEST_KEY_BLOCKED;
    }
}

void SetPlayerSelectShip(Handle<SpaceObject> ship, bool target, Handle<Admiral> adm) {
    Handle<SpaceObject> flagship = adm->flagship();
    Handle<Label>       label;

    if (adm == g.admiral) {
        globals()->lastSelectedObject   = ship;
        globals()->lastSelectedObjectID = ship->id;
        if (gDestKeyState == DEST_KEY_DOWN) {
            gDestKeyState = DEST_KEY_BLOCKED;
        }
    }
    if (target) {
        adm->set_target(ship);
        label = g.target_label;

        if (!(flagship->attributes & kOnAutoPilot)) {
            SetObjectDestination(flagship);
        }
    } else {
        adm->set_control(ship);
        label = g.control_label;
    }

    if (adm == g.admiral) {
        sys.sound.select();
        label->set_object(ship);
        if (ship == g.ship) {
            label->set_age(Label::kVisibleTime);
        }
        label->set_string(name_with_hot_key_suffix(ship));
    }
}

// ChangePlayerShipNumber()
// assumes that newShipNumber is the number of a valid (legal, living) ship and that
// gPlayerShip already points to the current, legal living ship

void ChangePlayerShipNumber(Handle<Admiral> adm, Handle<SpaceObject> newShip) {
    auto flagship = adm->flagship();
    if (!flagship.get()) {
        throw std::runtime_error(
                pn::format("adm: {0}, newShip: {1}", adm.number(), newShip.number()).c_str());
    }

    if (adm == g.admiral) {
        flagship->attributes &= ~kIsPlayerShip;
        if (newShip != g.ship) {
            g.ship = newShip;
            globals()->starfield.reset();
        }

        flagship = g.ship;
        if (!flagship.get()) {
            throw std::runtime_error(pn::format(
                                             "adm: {0}, newShip: {1}, gPlayerShip: {2}",
                                             adm.number(), newShip.number(), g.ship.number())
                                             .c_str());
        }

        flagship->attributes |= kIsPlayerShip;

        if (newShip == g.admiral->control()) {
            g.control_label->set_age(Label::kVisibleTime);
        }
        if (newShip == g.admiral->target()) {
            g.target_label->set_age(Label::kVisibleTime);
        }
    } else {
        flagship->attributes &= ~kIsPlayerShip;
        flagship = newShip;
        flagship->attributes |= kIsPlayerShip;
    }
    adm->set_flagship(newShip);
}

void TogglePlayerAutoPilot(Handle<SpaceObject> flagship) {
    if (flagship->attributes & kOnAutoPilot) {
        flagship->attributes &= ~kOnAutoPilot;
        if ((flagship->owner == g.admiral) && (flagship->attributes & kIsPlayerShip)) {
            Messages::autopilot(false);
        }
    } else {
        SetObjectDestination(flagship);
        flagship->attributes |= kOnAutoPilot;
        if ((flagship->owner == g.admiral) && (flagship->attributes & kIsPlayerShip)) {
            Messages::autopilot(true);
        }
    }
}

bool IsPlayerShipOnAutoPilot() { return g.ship.get() && (g.ship->attributes & kOnAutoPilot); }

void PlayerShipGiveCommand(Handle<Admiral> whichAdmiral) {
    auto control = whichAdmiral->control();

    if (control.get()) {
        SetObjectDestination(control);
        if (whichAdmiral == g.admiral) {
            sys.sound.order();
        }
    }
}

void PlayerShipBodyExpire(Handle<SpaceObject> flagship) {
    auto selectShip = flagship->owner->control();

    if (selectShip.get()) {
        if ((selectShip->active != kObjectInUse) || (!(selectShip->attributes & kCanThink)) ||
            (selectShip->attributes & kStaticDestination) ||
            (selectShip->owner != flagship->owner) ||
            (!(selectShip->attributes & kCanAcceptDestination)))
            selectShip = SpaceObject::none();
    }
    if (!selectShip.get()) {
        selectShip = g.root;
        while (selectShip.get() && ((selectShip->active != kObjectInUse) ||
                                    (selectShip->attributes & kStaticDestination) ||
                                    (!((selectShip->attributes & kCanThink) &&
                                       (selectShip->attributes & kCanAcceptDestination))) ||
                                    (selectShip->owner != flagship->owner))) {
            selectShip = selectShip->nextObject;
        }
    }
    if (selectShip.get()) {
        ChangePlayerShipNumber(flagship->owner, selectShip);
    } else {
        if (!g.game_over) {
            g.game_over    = true;
            g.game_over_at = g.time + secs(3);
        }
        switch (g.level->type()) {
            case Level::Type::SOLO: g.victory_text.emplace(g.level->solo.no_ships->copy()); break;
            case Level::Type::NET:
                if (flagship->owner == g.admiral) {
                    g.victory_text.emplace(g.level->net.own_no_ships->copy());
                } else {
                    g.victory_text.emplace(g.level->net.foe_no_ships->copy());
                }
                break;
            default: g.victory_text = sfz::nullopt; break;
        }
        if (flagship->owner.get()) {
            flagship->owner->set_flagship(SpaceObject::none());
        }
        if (flagship == g.ship) {
            g.ship = SpaceObject::none();
        }
    }
}

int32_t HotKey_GetFromObject(Handle<SpaceObject> object) {
    if (!object.get() && !object->active) {
        return -1;
    }
    for (int32_t i = 0; i < kHotKeyNum; ++i) {
        if (globals()->hotKey[i].object == object) {
            if (globals()->hotKey[i].objectID == object->id) {
                return i;
            }
        }
    }
    return -1;
}

void Update_LabelStrings_ForHotKeyChange(void) {
    auto target = g.admiral->target();
    if (target.get()) {
        g.target_label->set_object(target);
        if (target == g.ship) {
            g.target_label->set_age(Label::kVisibleTime);
        }
        g.target_label->set_string(name_with_hot_key_suffix(target));
    }

    auto control = g.admiral->control();
    if (control.get()) {
        g.control_label->set_object(control);
        if (control == g.ship) {
            g.control_label->set_age(Label::kVisibleTime);
        }
        sys.sound.select();
        g.control_label->set_string(name_with_hot_key_suffix(control));
    }
}

}  // namespace antares
