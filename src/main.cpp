// chr: the rebuild's program. For now it opens a window and shows the title
// screen, decoded from the player's own BTTITLE.CMP through the asset
// provider, or with --map N map N with the panel, walking with the arrow
// keys and Home/PgUp/End/PgDn.
//
//   chr [--game-dir DIR] [--data-dir DIR] [--import] [--map N] [--script KEYS]
//       [--text FILE] [--screenshot FILE.bmp]
//
// The game's text and font are imported once from the player's BTECH.EXE
// into the data folder (--data-dir, else CHR_DATA_DIR, else the user's
// application-data folder); that happens by itself the first time, or with
// --import (which imports again and quits). --text FILE layers a text file
// of the same form (formats/import.h) over the imported one, e.g. a
// translation.
// --script plays a key sequence on the map first ("up*5 y", "idle:40", see
// game/town.h) and prints what each key did. --screenshot renders one frame
// to the file without opening a window. In a window the town runs at the
// original's pace, one main-loop pass per display frame of a VGA card
// (game/clock.h).
//
// The game folder is --game-dir, else the CHR_GAME_DIR environment variable,
// else the folder set when the program was built.

#include <cstdio>
#include <deque>
#include <memory>
#include <vector>
#include <cstdlib>
#include <filesystem>
#include <string>

#include <SDL3/SDL.h>

#include "formats/game_files.h"
#include "formats/import.h"
#include "formats/mtp.h"
#include "formats/palette.h"
#include "game/asset_id.h"
#include "game/clock.h"
#include "game/map_view.h"
#include "game/text_provider.h"
#include "game/town.h"
#include "platform/map_layer.h"
#include "platform/original_assets.h"
#include "platform/panel_layer.h"
#include "platform/renderer.h"
#include "platform/text_layer.h"

namespace {

struct Options {
    std::filesystem::path game_dir;
    std::string screenshot;  // write one frame here and quit
    std::string script;      // keys to play on the map first
    std::string text_file;   // text overrides
    std::filesystem::path data_dir;  // the imported text and font
    bool import = false;
    int map = 0;             // 0: the title screen
};

bool parse(int argc, char** argv, Options& options) {
    if (const char* env = std::getenv("CHR_GAME_DIR"); env && *env)
        options.game_dir = env;
    else
        options.game_dir = CHR_DEFAULT_GAME_DIR;
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--game-dir" && i + 1 < argc) {
            options.game_dir = argv[++i];
        } else if (arg == "--map" && i + 1 < argc) {
            options.map = std::atoi(argv[++i]);
            if (options.map < 1 || options.map > 15) {
                std::fprintf(stderr, "chr: --map takes 1 to 15\n");
                return false;
            }
        } else if (arg == "--data-dir" && i + 1 < argc) {
            options.data_dir = argv[++i];
        } else if (arg == "--import") {
            options.import = true;
        } else if (arg == "--text" && i + 1 < argc) {
            options.text_file = argv[++i];
        } else if (arg == "--script" && i + 1 < argc) {
            options.script = argv[++i];
        } else if (arg == "--screenshot" && i + 1 < argc) {
            options.screenshot = argv[++i];
        } else {
            std::fprintf(stderr, "usage: chr [--game-dir DIR] [--data-dir DIR] [--import] [--map N] [--script KEYS]\n"
                         "           [--text FILE] [--screenshot FILE.bmp]\n");
            return false;
        }
    }
    return true;
}

bool save_screenshot(SDL_Renderer* renderer, const std::string& path) {
    SDL_Surface* shot = SDL_RenderReadPixels(renderer, nullptr);
    if (!shot) return false;
    const bool ok = SDL_SaveBMP(shot, path.c_str());
    SDL_DestroySurface(shot);
    return ok;
}

// Map `number` and where the party starts on it: the new-game start for
// map 1; for the others (reached from elsewhere in the game, not yet
// rebuilt) the map's middle.
bool load_map(const std::filesystem::path& game_dir, int number, chr::formats::MtpMap& map,
              chr::formats::MapPosition& start) {
    const std::string name = "MAP" + std::to_string(number) + ".MTP";
    try {
        const auto path = chr::formats::find_game_file(game_dir, name);
        if (!path) {
            std::fprintf(stderr, "chr: %s not found in %s\n", name.c_str(), game_dir.string().c_str());
            return false;
        }
        map = chr::formats::decode_mtp(chr::formats::read_file(*path));
        start = chr::game::new_game_position();
        if (number != 1)
            start = {std::uint16_t(map.block_x * 16 + map.width),
                     std::uint16_t(map.block_y * 16 + map.height)};
        return true;
    } catch (const std::exception& e) {
        std::fprintf(stderr, "chr: %s: %s\n", name.c_str(), e.what());
        return false;
    }
}

// The original's codes for the keys the town understands, or 0.
int key_code(SDL_Keycode key) {
    namespace k = chr::game::key;
    switch (key) {
    case SDLK_UP: case SDLK_KP_8: return k::kUp;
    case SDLK_DOWN: case SDLK_KP_2: return k::kDown;
    case SDLK_LEFT: case SDLK_KP_4: return k::kLeft;
    case SDLK_RIGHT: case SDLK_KP_6: return k::kRight;
    case SDLK_HOME: case SDLK_KP_7: return k::kHome;
    case SDLK_PAGEUP: case SDLK_KP_9: return k::kPageUp;
    case SDLK_END: case SDLK_KP_1: return k::kEnd;
    case SDLK_PAGEDOWN: case SDLK_KP_3: return k::kPageDown;
    case SDLK_RETURN: case SDLK_KP_ENTER: return k::kEnter;
    case SDLK_SPACE: return k::kSpace;
    default: break;
    }
    if (key >= SDLK_A && key <= SDLK_Z) return int('a' + (key - SDLK_A));
    return 0;
}

}  // namespace

int main(int argc, char** argv) {
    Options options;
    if (!parse(argc, argv, options)) return 2;

    if (options.data_dir.empty()) {
        if (const char* env = std::getenv("CHR_DATA_DIR"); env && *env) {
            options.data_dir = env;
        } else if (char* pref = SDL_GetPrefPath("", "crescent-hawks-rebuild")) {
            options.data_dir = pref;
            SDL_free(pref);
        } else {
            std::fprintf(stderr, "chr: no data folder (%s); use --data-dir\n", SDL_GetError());
            return 1;
        }
    }
    try {
        if (options.import) {
            chr::formats::import_original(options.game_dir, options.data_dir);
            std::printf("imported the text and font into %s\n", options.data_dir.string().c_str());
            return 0;
        }
        if (chr::formats::ensure_imported(options.game_dir, options.data_dir))
            std::fprintf(stderr, "chr: imported the text and font into %s\n", options.data_dir.string().c_str());
    } catch (const std::exception& e) {
        std::fprintf(stderr, "chr: %s\n", e.what());
        return 1;
    }

    // A screenshot run never shows a window: it renders with SDL's dummy
    // video driver and the software renderer (unless SDL_VIDEO_DRIVER or
    // SDL_RENDER_DRIVER names others) into a hidden window, so it is safe to
    // run while the screen is being shared.
    const bool headless = !options.screenshot.empty();
    if (headless && !std::getenv("SDL_VIDEO_DRIVER"))
        SDL_SetHint(SDL_HINT_VIDEO_DRIVER, "dummy");
    if (headless && !std::getenv("SDL_RENDER_DRIVER"))
        SDL_SetHint(SDL_HINT_RENDER_DRIVER, "software");
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        std::fprintf(stderr, "chr: SDL_Init: %s\n", SDL_GetError());
        return 1;
    }
    SDL_Window* window = nullptr;
    SDL_Renderer* sdl_renderer = nullptr;
    const SDL_WindowFlags window_flags = headless ? SDL_WINDOW_HIDDEN : SDL_WINDOW_RESIZABLE;
    if (!SDL_CreateWindowAndRenderer("Crescent Hawks Rebuild", 960, 600, window_flags,
                                     &window, &sdl_renderer)) {
        std::fprintf(stderr, "chr: can't open a window: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    int status = 0;
    chr::formats::MtpMap map;
    chr::formats::MapPosition start;
    std::deque<chr::game::ScriptItem> keys;  // every key and idle stretch so far, the script's first
    if (options.map != 0 && !load_map(options.game_dir, options.map, map, start)) status = 1;
    chr::game::TextTable overrides;
    if (status == 0 && !options.text_file.empty()) {
        try {
            overrides = chr::game::TextTable::load(options.text_file);
        } catch (const std::exception& e) {
            std::fprintf(stderr, "chr: --text %s: %s\n", options.text_file.c_str(), e.what());
            status = 2;
        }
    }
    if (status == 0) {
        try {
            keys = chr::game::parse_key_script(options.script);
        } catch (const std::exception& e) {
            std::fprintf(stderr, "chr: --script: %s\n", e.what());
            status = 2;
        }
    }
    if (status == 0) {
        chr::platform::OriginalAssets assets(options.game_dir, options.data_dir);
        chr::platform::Renderer renderer(sdl_renderer, assets);
        const auto palette = chr::formats::standard_palette_ega();
        const auto& title = chr::assets::kTitleScreen;

        // The town takes keys and idle passes as they come. A question asks
        // for its answer in the middle of a turn; when it is left waiting,
        // the town is played again from the start with the answer (replay),
        // which keeps the logic a plain sequence.
        std::unique_ptr<chr::game::Town> town;
        chr::game::TextTable imported;
        try {
            imported = chr::game::TextTable::load(chr::formats::imported_text_path(options.data_dir));
        } catch (const std::exception& e) {
            std::fprintf(stderr, "chr: %s\n", e.what());
            status = 1;
        }
        const chr::game::MapText map_text(map, options.map);
        const chr::game::LayeredText texts({&overrides, &imported, &map_text});
        auto replay = [&]() -> bool {
            try {
                town = std::make_unique<chr::game::Town>(map, options.map, start, texts);
                town->draw_panel();
                chr::game::ScriptKeys source(keys);
                town->run(source);
                return true;
            } catch (const std::exception& e) {
                std::fprintf(stderr, "chr: %s\n", e.what());
                town.reset();
                return false;
            }
        };
        if (options.map != 0 && status == 0) {
            if (!replay()) {
                status = 1;
            } else if (!options.script.empty()) {
                for (const auto& line : town->log()) std::printf("%s\n", line.c_str());
                if (town->question_open()) std::printf("(waiting for an answer)\n");
            }
        }

        bool running = status == 0;
        bool reported = false;
        chr::game::FrameClock clock;
        Uint64 last = SDL_GetTicksNS();
        while (running) {
            SDL_Event event;
            while (SDL_PollEvent(&event)) {
                if (event.type == SDL_EVENT_QUIT) running = false;
                if (event.type != SDL_EVENT_KEY_DOWN) continue;
                if (event.key.key == SDLK_ESCAPE) running = false;
                if (const int k = key_code(event.key.key); k != 0 && town) {
                    keys.push_back({k, 0});
                    if (town->question_open()) {
                        replay();
                    } else {
                        chr::game::ScriptKeys none({});
                        town->key(k, none);
                    }
                }
            }
            const Uint64 now = SDL_GetTicksNS();
            const int frames = clock.advance(double(now - last) / 1e9);
            last = now;
            if (town && frames > 0 && !town->question_open() && options.screenshot.empty()) {
                for (int i = 0; i < frames; ++i) town->idle_pass();
                if (!keys.empty() && keys.back().idle > 0) keys.back().idle += frames;
                else keys.push_back({0, frames});
            }
            renderer.begin_frame();
            bool drawn = true;
            if (town) {
                const auto view = town->view();
                drawn = chr::platform::draw_map(renderer, view);
                drawn = chr::platform::draw_figures(renderer, view, town->figures()) && drawn;
                drawn = chr::platform::draw_text(renderer, town->text(), palette) && drawn;
                chr::platform::draw_bars(renderer, town->bars(), palette);
            } else {
                drawn = renderer.draw(title, 0, 0);
            }
            if (!drawn && !reported) {
                std::fprintf(stderr, "chr: %s\n", assets.last_error().c_str());
                reported = true;
                status = 1;
                if (!options.screenshot.empty()) running = false;
            }
            if (!options.screenshot.empty() && running) {
                if (!save_screenshot(sdl_renderer, options.screenshot)) {
                    std::fprintf(stderr, "chr: screenshot: %s\n", SDL_GetError());
                    status = 1;
                }
                running = false;
            }
            renderer.end_frame();
            if (running) SDL_Delay(16);
        }
    }

    SDL_DestroyRenderer(sdl_renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return status;
}
