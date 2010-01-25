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

#include "Fakes.hpp"

#include <sys/time.h>
#include <getopt.h>
#include <queue>

#include "sfz/Exception.hpp"
#include "sfz/Format.hpp"
#include "sfz/Formatter.hpp"
#include "sfz/String.hpp"
#include "sfz/StringUtilities.hpp"

#include "AresMain.hpp"
#include "AresPreferences.hpp"
#include "Error.hpp"
#include "FakeDrawing.hpp"
#include "FakeSounds.hpp"
#include "File.hpp"
#include "ImageDriver.hpp"
#include "Ledger.hpp"
#include "LibpngImageDriver.hpp"
#include "TestVideoDriver.hpp"
#include "Threading.hpp"
#include "VideoDriver.hpp"
#include "VncServer.hpp"

using sfz::Exception;
using sfz::String;
using sfz::StringPiece;
using sfz::ascii_encoding;
using sfz::print;
using sfz::quote;
using sfz::string_to_int32_t;
using sfz::utf8_encoding;

namespace antares {

void usage(const StringPiece& program_name) {
    print(
            2,
            "usage: {0} <test> [<options>]\n"
            "options:\n"
            "    -l|--level=<int>   choose a level to use in the given mode\n"
            "    -o|--output=<dir>  directory to save dumps to\n"
            "tests:\n"
            "    main-screen        dumps the main screen, then exits\n"
            "    mission-briefing   dumps the mission briefing screens for <level>\n"
            "    demo               runs the demo for <level>\n",
            program_name);
    exit(1);
}

enum Test {
    TEST_UNKNOWN = -1,
    TEST_MAIN_SCREEN,
    TEST_MISSION_BRIEFING,
    TEST_DEMO,
};

Test string_to_test(const char* string) {
    if (strcmp(string, "main-screen") == 0) {
        return TEST_MAIN_SCREEN;
    } else if (strcmp(string, "mission-briefing") == 0) {
        return TEST_MISSION_BRIEFING;
    } else if (strcmp(string, "demo") == 0) {
        return TEST_DEMO;
    } else {
        return TEST_UNKNOWN;
    }
}

void TestMain(int argc, char* const* argv) {
    StringPiece program_name(argv[0], utf8_encoding());
    int level = -1;
    String output_dir;
    option longopts[] = {
        { "level",          required_argument,  NULL,   'l' },
        { "output",         required_argument,  NULL,   'o' },
        { NULL,             0,                  NULL,   0 }
    };

    if (argc < 2) {
        usage(program_name);
    }
    Test test = string_to_test(argv[1]);
    argc -= 1;
    argv += 1;

    char ch;
    while ((ch = getopt_long(argc, argv, "l:o:", longopts, NULL)) != -1) {
        switch (ch) {
          case 'l':
            {
                StringPiece opt(optarg, utf8_encoding());
                if (!string_to_int32_t(opt, &level)) {
                    throw Exception("invalid level {0}", quote(opt));
                }
            }
            break;

          case 'o':
            if (!*optarg) {
                print(2, "{0}: --output-dir must not be empty\n", program_name);
                usage(program_name);
            }
            output_dir.assign(optarg, utf8_encoding());
            break;

          default:
            print(2, "{0}: unknown argument {1}\n", program_name, argv[optind]);
            usage(program_name);
            break;
        }
    }

    argc -= optind;
    argv += optind;
    if (argc != 0) {
        print(2, "{0}: too many arguments\n", program_name);
        usage(program_name);
    }

    if (!output_dir.empty()) {
        MakeDirs(output_dir, 0755);
    }

    FakeDrawingInit(640, 480);
    ImageDriver::set_driver(new LibpngImageDriver);
    switch (test) {
      case TEST_MAIN_SCREEN:
        SoundDriver::set_driver(new NullSoundDriver);
        VideoDriver::set_driver(new MainScreenVideoDriver(output_dir));
        break;

      case TEST_MISSION_BRIEFING:
        SoundDriver::set_driver(new NullSoundDriver);
        VideoDriver::set_driver(new MissionBriefingVideoDriver(output_dir, level));
        break;

      case TEST_DEMO:
        if (output_dir.empty()) {
            SoundDriver::set_driver(new NullSoundDriver);
        } else {
            String out(output_dir);
            out.append("/sound.log", ascii_encoding());
            SoundDriver::set_driver(new LogSoundDriver(out));
        }
        VideoDriver::set_driver(new DemoVideoDriver(output_dir, level));
        break;

      case TEST_UNKNOWN:
        fail("--test was an unknown value");
    }
    Ledger::set_ledger(new NullLedger);

    CardStack stack(AresInit());
    VideoDriver::driver()->loop(&stack);
}

}  // namespace antares

int main(int argc, char* const* argv) {
    antares::TestMain(argc, argv);
    return 0;
}