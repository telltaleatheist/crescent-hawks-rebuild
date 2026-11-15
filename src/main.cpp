// chr: the rebuild's program. For now it opens a window and shows the title
// screen, decoded from the player's own BTTITLE.CMP through the asset provider.
//
//   chr [--game-dir DIR] [--screenshot FILE.bmp]
//
// The game folder is --game-dir, else the CHR_GAME_DIR environment variable,
// else the folder set when the program was built.

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <string>

#include <SDL3/SDL.h>

#include "game/asset_id.h"
#include "platform/original_assets.h"
#include "platform/renderer.h"

namespace {

struct Options {
    std::filesystem::path game_dir;
    std::string screenshot;  // write one frame here and quit
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
        } else if (arg == "--screenshot" && i + 1 < argc) {
            options.screenshot = argv[++i];
        } else {
            std::fprintf(stderr, "usage: chr [--game-dir DIR] [--screenshot FILE.bmp]\n");
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

}  // namespace

int main(int argc, char** argv) {
    Options options;
    if (!parse(argc, argv, options)) return 2;

    if (!SDL_Init(SDL_INIT_VIDEO)) {
        std::fprintf(stderr, "chr: SDL_Init: %s\n", SDL_GetError());
        return 1;
    }
    SDL_Window* window = nullptr;
    SDL_Renderer* sdl_renderer = nullptr;
    if (!SDL_CreateWindowAndRenderer("Crescent Hawks Rebuild", 960, 600, SDL_WINDOW_RESIZABLE,
                                     &window, &sdl_renderer)) {
        std::fprintf(stderr, "chr: can't open a window: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    int status = 0;
    {
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
            if (!renderer.draw(title, 0, 0) && !reported) {
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
