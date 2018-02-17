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

#include "game/level.hpp"

#include <set>
#include <sfz/sfz.hpp>

#include "data/plugin.hpp"
#include "drawing/pix-table.hpp"
#include "game/action.hpp"
#include "game/admiral.hpp"
#include "game/condition.hpp"
#include "game/globals.hpp"
#include "game/initial.hpp"
#include "game/instruments.hpp"
#include "game/labels.hpp"
#include "game/messages.hpp"
#include "game/minicomputer.hpp"
#include "game/motion.hpp"
#include "game/non-player-ship.hpp"
#include "game/player-ship.hpp"
#include "game/starfield.hpp"
#include "game/sys.hpp"
#include "game/vector.hpp"
#include "lang/defines.hpp"
#include "math/macros.hpp"
#include "math/random.hpp"
#include "math/rotation.hpp"
#include "math/units.hpp"

using sfz::range;
using std::set;

namespace antares {

namespace {

#ifdef DATA_COVERAGE
ANTARES_GLOBAL set<int32_t> possible_objects;
ANTARES_GLOBAL set<int32_t> possible_actions;
#endif  // DATA_COVERAGE

void AddBaseObjectActionMedia(
        Handle<BaseObject> base, std::vector<Action>(BaseObject::*whichType), uint8_t color,
        std::bitset<16> all_colors, LoadState* state);
void AddActionMedia(
        const Action& action, uint8_t color, std::bitset<16> all_colors, LoadState* state);

void AddBaseObjectMedia(
        Handle<BaseObject> base, uint8_t color, std::bitset<16> all_colors, LoadState* state) {
#ifdef DATA_COVERAGE
    possible_objects.insert(base.number());
#endif  // DATA_COVERAGE

    if (!(base->attributes & kCanThink)) {
        color = GRAY;
    }
    state->colors_needed[base.number()][color] = true;
    for (int i = 0; i < 16; ++i) {
        if (state->colors_loaded[base.number()][i]) {
            continue;  // color already loaded.
        } else if (!state->colors_needed[base.number()][i]) {
            continue;  // color not needed.
        }
        state->colors_loaded[base.number()][i] = true;

        if (base->pixResID != kNoSpriteTable) {
            int16_t id = base->pixResID + (i << kSpriteTableColorShift);
            sys.pix.add(id);
        }

        AddBaseObjectActionMedia(base, &BaseObject::destroy, i, all_colors, state);
        AddBaseObjectActionMedia(base, &BaseObject::expire, i, all_colors, state);
        AddBaseObjectActionMedia(base, &BaseObject::create, i, all_colors, state);
        AddBaseObjectActionMedia(base, &BaseObject::collide, i, all_colors, state);
        AddBaseObjectActionMedia(base, &BaseObject::activate, i, all_colors, state);
        AddBaseObjectActionMedia(base, &BaseObject::arrive, i, all_colors, state);

        for (Handle<BaseObject> weapon : {base->pulse.base, base->beam.base, base->special.base}) {
            if (weapon.get()) {
                AddBaseObjectMedia(weapon, i, all_colors, state);
            }
        }
    }
}

void AddBaseObjectActionMedia(
        Handle<BaseObject> base, std::vector<Action>(BaseObject::*whichType), uint8_t color,
        std::bitset<16> all_colors, LoadState* state) {
    for (const auto& action : (*base.*whichType)) {
        AddActionMedia(action, color, all_colors, state);
    }
}

void AddActionMedia(
        const Action& action, uint8_t color, std::bitset<16> all_colors, LoadState* state) {
    int32_t l1, l2;
#ifdef DATA_COVERAGE
    possible_actions.insert(action.number());
#endif  // DATA_COVERAGE

    switch (action.verb) {
        case kCreateObject:
        case kCreateObjectSetDest:
            AddBaseObjectMedia(
                    action.argument.createObject.whichBaseType, color, all_colors, state);
            break;

        case kPlaySound:
            l1 = action.argument.playSound.idMinimum;
            l2 = action.argument.playSound.idMinimum + action.argument.playSound.idRange;
            for (int32_t count = l1; count <= l2; count++) {
                sys.sound.load(count);
            }
            break;

        case kAlterBaseType:
            AddBaseObjectMedia(action.argument.alterBaseType.base, color, all_colors, state);
            break;

        case kAlterOwner:
            for (auto baseObject : BaseObject::all()) {
                if (action_filter_applies_to(action, baseObject)) {
                    state->colors_needed[baseObject.number()] |= all_colors;
                }
                if (state->colors_loaded[baseObject.number()].any()) {
                    AddBaseObjectMedia(baseObject, color, all_colors, state);
                }
            }
            break;
    }
}

static coordPointType rotate_coords(int32_t h, int32_t v, int32_t rotation) {
    mAddAngle(rotation, 90);
    Fixed lcos, lsin;
    GetRotPoint(&lcos, &lsin, rotation);
    coordPointType coord;
    coord.h =
            (kUniversalCenter + (Fixed::from_val(h) * -lcos).val() -
             (Fixed::from_val(v) * -lsin).val());
    coord.v =
            (kUniversalCenter + (Fixed::from_val(h) * -lsin).val() +
             (Fixed::from_val(v) * -lcos).val());
    return coord;
}

void GetInitialCoord(
        const Level::InitialObject* initial, coordPointType* coord, int32_t rotation) {
    *coord = rotate_coords(initial->location.h, initial->location.v, rotation);
}

}  // namespace

Point Level::star_map_point() const { return Point(starMapH, starMapV); }

int32_t Level::chapter_number() const { return levelNameStrNum; }

LoadState start_construct_level(Handle<Level> level) {
    ResetAllSpaceObjects();
    reset_action_queue();
    Vectors::reset();
    ResetAllSprites();
    Label::reset();
    ResetInstruments();
    Admiral::reset();
    ResetAllDestObjectData();
    ResetMotionGlobals();
    gAbsoluteScale = kTimesTwoScale;
    g.sync         = 0;

    g.level = level;

    {
        int32_t angle = g.level->angle;
        if (angle < 0) {
            g.angle = g.random.next(ROT_POS);
        } else {
            g.angle = angle;
        }
    }

    g.victor       = Admiral::none();
    g.next_level   = -1;
    g.victory_text = "";

    SetMiniScreenStatusStrList(g.level->score_strings);

    for (int i = 0; i < g.level->playerNum; i++) {
        if (g.level->player[i].playerType == kSingleHumanPlayer) {
            auto admiral = Admiral::make(i, kAIsHuman, g.level->player[i]);
            admiral->pay(Fixed::from_long(5000));
            g.admiral = admiral;
        } else {
            auto admiral = Admiral::make(i, kAIsComputer, g.level->player[i]);
            admiral->pay(Fixed::from_long(5000));
        }
    }

    // *** END INIT ADMIRALS ***

    ///// FIRST SELECT WHAT MEDIA WE NEED TO USE:

    sys.pix.reset();
    sys.sound.reset();

    LoadState s;
    s.max = g.level->initials.size() * 3L + 1 +
            g.level->startTime.count();  // for each run through the initial num

    return s;
}

static void load_blessed_objects(std::bitset<16> all_colors, LoadState* state) {
    if (!plug.info.energyBlobID.get()) {
        throw std::runtime_error("No energy blob defined");
    }
    if (!plug.info.warpInFlareID.get()) {
        throw std::runtime_error("No warp in flare defined");
    }
    if (!plug.info.warpOutFlareID.get()) {
        throw std::runtime_error("No warp out flare defined");
    }
    if (!plug.info.playerBodyID.get()) {
        throw std::runtime_error("No player body defined");
    }

    // Load the four blessed objects.  The player's body is needed
    // in all colors; the other three are needed only as neutral
    // objects by default.
    state->colors_needed[plug.info.playerBodyID.number()] |= all_colors;
    for (int i = 0; i < g.level->playerNum; i++) {
        const auto&        info      = plug.info;
        Handle<BaseObject> blessed[] = {
                info.energyBlobID, info.warpInFlareID, info.warpOutFlareID, info.playerBodyID,
        };
        for (auto id : blessed) {
            AddBaseObjectMedia(id, GRAY, all_colors, state);
        }
    }
}

static void load_initial(int i, std::bitset<16> all_colors, LoadState* state) {
    const Level::InitialObject* initial    = &g.level->initials[i];
    Handle<Admiral>             owner      = initial->owner;
    auto                        baseObject = initial->type;
    // TODO(sfiera): remap objects in networked games.

    // Load the media for this object
    //
    // I don't think that it's necessary to treat kIsDestination
    // objects specially here.  If their ownership can change, there
    // will be a transport or something that does it, and we will
    // mark the necessity of having all colors through action
    // checking.
    if (baseObject->attributes & kIsDestination) {
        state->colors_needed[baseObject.number()] |= all_colors;
    }
    AddBaseObjectMedia(baseObject, GetAdmiralColor(owner), all_colors, state);

    // make sure we're not overriding the sprite
    if (initial->spriteIDOverride >= 0) {
        if (baseObject->attributes & kCanThink) {
            sys.pix.add(
                    initial->spriteIDOverride +
                    (GetAdmiralColor(owner) << kSpriteTableColorShift));
        } else {
            sys.pix.add(initial->spriteIDOverride);
        }
    }

    // check any objects this object can build
    for (int j = 0; j < kMaxTypeBaseCanBuild; j++) {
        if (initial->canBuild[j] != kNoClass) {
            // check for each player
            for (auto a : Admiral::all()) {
                if (a->active()) {
                    auto baseObject =
                            mGetBaseObjectFromClassRace(initial->canBuild[j], GetAdmiralRace(a));
                    if (baseObject.get()) {
                        AddBaseObjectMedia(baseObject, GetAdmiralColor(a), all_colors, state);
                    }
                }
            }
        }
    }
}

static void load_condition(
        Level::Condition* condition, std::bitset<16> all_colors, LoadState* state) {
    for (const auto& action : condition->action) {
        AddActionMedia(action, GRAY, all_colors, state);
    }
    condition->set_true_yet(condition->flags & kInitiallyTrue);
}

static void run_game_1s() {
    game_ticks start_time = game_ticks(-g.level->startTime);
    do {
        g.time += kMajorTick;
        MoveSpaceObjects(kMajorTick);
        NonplayerShipThink();
        AdmiralThink();
        execute_action_queue();
        CollideSpaceObjects();
        if (((g.time - start_time) % kConditionTick) == ticks(0)) {
            CheckLevelConditions();
        }
        CullSprites();
        Vectors::cull();
    } while ((g.time.time_since_epoch() % secs(1)) != ticks(0));
}

void construct_level(Handle<Level> level, LoadState* state) {
    int32_t         step = state->step;
    std::bitset<16> all_colors;
    all_colors[0] = true;
    for (auto adm : Admiral::all()) {
        if (adm->active()) {
            all_colors[GetAdmiralColor(adm)] = true;
        }
    }

    if (step == 0) {
        load_blessed_objects(all_colors, state);
        load_initial(step, all_colors, state);
    } else if (step < g.level->initials.size()) {
        load_initial(step, all_colors, state);
    } else if (step == g.level->initials.size()) {
        // add media for all condition actions
        step -= g.level->initials.size();
        for (auto& condition : g.level->conditions) {
            load_condition(&condition, all_colors, state);
        }
        create_initial(&g.level->initials[step]);
    } else if (step < (2 * g.level->initials.size())) {
        step -= g.level->initials.size();
        create_initial(&g.level->initials[step]);
    } else if (step < (3 * g.level->initials.size())) {
        // double back and set up any defined initial destinations
        step -= (2 * g.level->initials.size());
        set_initial_destination(&g.level->initials[step], false);
    } else if (step == (3 * g.level->initials.size())) {
        RecalcAllAdmiralBuildData();  // set up all the admiral's destination objects
        Messages::clear();
        g.time = game_ticks(-g.level->startTime);
    } else {
        run_game_1s();
    }
    ++state->step;
    if (state->step == state->max) {
        state->done = true;
    }
    return;
}

void DeclareWinner(Handle<Admiral> whichPlayer, int32_t nextLevel, pn::string_view text) {
    if (!whichPlayer.get()) {
        // if there's no winner, we want to exit immediately
        g.next_level   = nextLevel;
        g.victory_text = text.copy();
        g.game_over    = true;
        g.game_over_at = g.time;
    } else {
        if (!g.victor.get()) {
            g.victor       = whichPlayer;
            g.victory_text = text.copy();
            g.next_level   = nextLevel;
            if (!g.game_over) {
                g.game_over    = true;
                g.game_over_at = g.time + secs(3);
            }
        }
    }
}

// GetLevelFullScaleAndCorner:
//  This is really just for the mission briefing.  It calculates the best scale
//  at which to show the entire scenario.

void GetLevelFullScaleAndCorner(
        const Level* level, int32_t rotation, coordPointType* corner, int32_t* scale,
        Rect* bounds) {
    int32_t biggest, mustFit;
    Point   coord, otherCoord, tempCoord;

    mustFit = bounds->bottom - bounds->top;
    if ((bounds->right - bounds->left) < mustFit)
        mustFit = bounds->right - bounds->left;

    biggest = 0;
    for (const auto& initial : level->initials) {
        if (!(initial.attributes & kInitiallyHidden)) {
            GetInitialCoord(&initial, reinterpret_cast<coordPointType*>(&coord), g.angle);

            for (const auto& other : level->initials) {
                GetInitialCoord(&other, reinterpret_cast<coordPointType*>(&otherCoord), g.angle);

                if (ABS(otherCoord.h - coord.h) > biggest) {
                    biggest = ABS(otherCoord.h - coord.h);
                }
                if (ABS(otherCoord.v - coord.v) > biggest) {
                    biggest = ABS(otherCoord.v - coord.v);
                }
            }
        }
    }

    biggest += biggest >> 2L;

    *scale = SCALE_SCALE * mustFit;
    *scale /= biggest;

    otherCoord.h = kUniversalCenter;
    otherCoord.v = kUniversalCenter;
    coord.h      = kUniversalCenter;
    coord.v      = kUniversalCenter;
    for (const auto& initial : level->initials) {
        if (!(initial.attributes & kInitiallyHidden)) {
            GetInitialCoord(&initial, reinterpret_cast<coordPointType*>(&tempCoord), g.angle);

            if (tempCoord.h < coord.h) {
                coord.h = tempCoord.h;
            }
            if (tempCoord.v < coord.v) {
                coord.v = tempCoord.v;
            }

            if (tempCoord.h > otherCoord.h) {
                otherCoord.h = tempCoord.h;
            }
            if (tempCoord.v > otherCoord.v) {
                otherCoord.v = tempCoord.v;
            }
        }
    }

    biggest = bounds->right - bounds->left;
    biggest *= SCALE_SCALE;
    biggest /= *scale;
    biggest /= 2;
    corner->h = (coord.h + (otherCoord.h - coord.h) / 2) - biggest;
    biggest   = (bounds->bottom - bounds->top);
    biggest *= SCALE_SCALE;
    biggest /= *scale;
    biggest /= 2;
    corner->v = (coord.v + (otherCoord.v - coord.v) / 2) - biggest;
}

coordPointType Translate_Coord_To_Level_Rotation(int32_t h, int32_t v) {
    return rotate_coords(h, v, g.angle);
}

}  // namespace antares
