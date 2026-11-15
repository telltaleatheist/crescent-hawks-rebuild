// chr: the rebuild's program. For now it opens a window and shows the title
// screen, decoded from the player's own BTTITLE.CMP through the asset
// provider, or with --map N the part of map N the player first sees.
//
//   chr [--game-dir DIR] [--map N] [--screenshot FILE.bmp]
//
// --screenshot renders one frame to the file without opening a window.
//
// The game folder is --game-dir, else the CHR_GAME_DIR environment variable,
// else the folder set when the program was built.

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <string>

#include <SDL3/SDL.h>

#include "formats/game_files.h"
#include "formats/mtp.h"
#include "game/asset_id.h"
#include "game/map_view.h"
#include "platform/map_layer.h"
#include "platform/original_assets.h"
#include "platform/renderer.h"

namespace {

struct Options {
    std::filesystem::path game_dir;
    std::string screenshot;  // write one frame here and quit
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
        } else if (arg == "--screenshot" && i + 1 < argc) {
            options.screenshot = argv[++i];
        } else {
            std::fprintf(stderr, "usage: chr [--game-dir DIR] [--map N] [--screenshot FILE.bmp]\n");
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

// The view of map `number` at its starting position: the new-game start
// for map 1; for the others (reached from elsewhere in the game, not yet
// rebuilt) the map's middle.
bool load_map_view(const std::filesystem::path& game_dir, int number, chr::game::MapView& view) {
    const std::string name = "MAP" + std::to_string(number) + ".MTP";
    try {
        const auto path = chr::formats::find_game_file(game_dir, name);
        if (!path) {
            std::fprintf(stderr, "chr: %s not found in %s\n", name.c_str(), game_dir.string().c_str());
            return false;
        }
        const auto map = chr::formats::decode_mtp(chr::formats::read_file(*path));
        chr::formats::MapPosition position = chr::game::new_game_position();
        if (number != 1)
            position = {std::uint16_t(map.block_x * 16 + map.width),
                        std::uint16_t(map.block_y * 16 + map.height)};
        view = chr::game::view_of(map, chr::game::tile_sheet_for_map(number), position);
        return true;
    } catch (const std::exception& e) {
        std::fprintf(stderr, "chr: %s: %s\n", name.c_str(), e.what());
        return false;
    }
}

}  // namespace

int main(int argc, char** argv) {
    Options options;
    if (!parse(argc, argv, options)) return 2;

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
    chr::game::MapView map_view;
    if (options.map != 0 && !load_map_view(options.game_dir, options.map, map_view)) status = 1;
    if (status == 0) {
        chr::platform::OriginalAssets assets(options.game_dir);
        chr::platform::Renderer renderer(sdl_renderer, assets);
        const auto& title = chr::assets::kTitleScreen;

        bool running = true;
        bool reported = false;
        while (running) {
            SDL_Event event;
            while (SDL_PollEvent(&event)) {
                if (event.type == SDL_EVENT_QUIT) running = false;
                if (event.type == SDL_EVENT_KEY_DOWN && event.key.key == SDLK_ESCAPE)
                    running = false;
            }
            renderer.begin_frame();
            bool drawn = true;
            if (options.map != 0)
                drawn = chr::platform::draw_map(renderer, map_view);
            else
                drawn = renderer.draw(title, 0, 0);
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
