// Draws asset IDs on the original's 320x200 screen, scaled up to the window
// by whole numbers with nearest-neighbour sampling. It gets every picture
// through an AssetProvider and keeps a texture per ID.
#pragma once

#include <map>

#include <SDL3/SDL.h>

#include "game/asset_id.h"
#include "platform/asset_provider.h"

namespace chr::platform {

inline constexpr int kScreenWidth = 320;
inline constexpr int kScreenHeight = 200;

class Renderer {
public:
    Renderer(SDL_Renderer* sdl, AssetProvider& assets);
    ~Renderer();
    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;

    void begin_frame();
    // Draws the picture for `id` with its top-left corner at (x, y) in
    // 320x200 screen units. Returns false if no provider has it.
    bool draw(const AssetId& id, int x, int y);
    void end_frame();

private:
    SDL_Texture* texture_for(const AssetId& id);

    SDL_Renderer* sdl_;
    AssetProvider& assets_;
    struct Entry {
        SDL_Texture* texture = nullptr;  // null: the provider had nothing
        float logical_width = 0, logical_height = 0;
    };
    std::map<AssetId, Entry> cache_;
};

}  // namespace chr::platform
