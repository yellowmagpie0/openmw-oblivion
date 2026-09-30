// Test-only SDL input replay for displays that cannot expose an X11 socket.
// Compile: c++ -std=c++17 -shared -fPIC scripts/sdl_offscreen_input.cpp
//          $(pkg-config --cflags --libs sdl2) -ldl -o replay.so
// Opt in with LD_PRELOAD and OPENMW_SDL_INPUT pointing to an append-only file.
// Lines: <sequence starting at 1> <down|up> <SDL scancode>, or <sequence> quit.
// Complete lines are consumed only after SDL's existing event queue is empty.
// The helper supplies input events; it has no engine or game-state interface.

#include <SDL.h>
#include <dlfcn.h>

#include <array>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <string>

namespace
{
    struct Replay
    {
        const char* path = std::getenv("OPENMW_SDL_INPUT");
        std::streamoff offset = 0;
        unsigned long long sequence = 1;
        bool failed = false;
        std::array<Uint8, SDL_NUM_SCANCODES> held{};

        bool next(SDL_Event& event)
        {
            if (!path || failed)
                return false;
            std::ifstream input(path);
            if (!input)
                return fail(event, "cannot open input file");
            input.seekg(offset);
            std::string line;
            if (!std::getline(input, line) || input.eof())
                return false; // An append may not have completed the final newline yet.
            const char* driver = SDL_GetCurrentVideoDriver();
            if (!driver || std::string(driver) != "offscreen")
                return fail(event, "replay requires the SDL offscreen driver");
            const auto end = input.tellg();
            std::istringstream fields(line);
            unsigned long long ordinal = 0;
            std::string operation, extra;
            int code = 0;
            if (!(fields >> ordinal >> operation) || ordinal != sequence)
                return fail(event, "invalid input sequence");
            event = {};
            if (operation == "quit")
            {
                if (fields >> extra)
                    return fail(event, "unexpected quit arguments");
                event.type = SDL_QUIT;
            }
            else
            {
                if ((operation != "down" && operation != "up") || !(fields >> code)
                    || code <= SDL_SCANCODE_UNKNOWN || code >= SDL_NUM_SCANCODES || (fields >> extra))
                    return fail(event, "invalid keyboard input");
                event.type = operation == "down" ? SDL_KEYDOWN : SDL_KEYUP;
                event.key.timestamp = SDL_GetTicks();
                event.key.windowID = 1; // OpenMW's sole SDL window.
                event.key.state = operation == "down" ? SDL_PRESSED : SDL_RELEASED;
                event.key.repeat = 0;
                event.key.keysym.scancode = static_cast<SDL_Scancode>(code);
                event.key.keysym.sym = SDL_GetKeyFromScancode(event.key.keysym.scancode);
                held[code] = event.key.state;
                SDL_Keymod mods = KMOD_NONE;
                for (const auto& [scan, flag] : std::array<std::pair<int, SDL_Keymod>, 8>{ {
                         { SDL_SCANCODE_LSHIFT, KMOD_LSHIFT }, { SDL_SCANCODE_RSHIFT, KMOD_RSHIFT },
                         { SDL_SCANCODE_LCTRL, KMOD_LCTRL }, { SDL_SCANCODE_RCTRL, KMOD_RCTRL },
                         { SDL_SCANCODE_LALT, KMOD_LALT }, { SDL_SCANCODE_RALT, KMOD_RALT },
                         { SDL_SCANCODE_LGUI, KMOD_LGUI }, { SDL_SCANCODE_RGUI, KMOD_RGUI } } })
                    if (held[scan])
                        mods = static_cast<SDL_Keymod>(mods | flag);
                SDL_SetModState(mods);
                event.key.keysym.mod = mods;
            }
            std::ofstream receipt(std::string(path) + ".delivered", std::ios::app);
            receipt << line << '\n';
            receipt.flush();
            if (!receipt)
                return fail(event, "cannot record delivered input");
            offset = end;
            ++sequence;
            return true;
        }

        bool fail(SDL_Event& event, const char* message)
        {
            std::fprintf(stderr, "SDL offscreen replay error: %s\n", message);
            failed = true;
            event = {};
            event.type = SDL_QUIT;
            return true;
        }
    };

    Replay& replay()
    {
        static Replay value;
        return value;
    }
}

extern "C" int SDL_PollEvent(SDL_Event* event)
{
    static auto original = reinterpret_cast<decltype(&SDL_PollEvent)>(dlsym(RTLD_NEXT, "SDL_PollEvent"));
    const int result = original(event);
    if (result || !event)
        return result;
    return replay().next(*event) ? 1 : 0;
}

extern "C" const Uint8* SDL_GetKeyboardState(int* count)
{
    static auto original
        = reinterpret_cast<decltype(&SDL_GetKeyboardState)>(dlsym(RTLD_NEXT, "SDL_GetKeyboardState"));
    if (!replay().path)
        return original(count);
    if (count)
        *count = SDL_NUM_SCANCODES;
    return replay().held.data();
}
