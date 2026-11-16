// Draws asset IDs on the original's 320x200 screen, scaled up to the window
// by whole numbers with nearest-neighbour sampling. It gets every picture
// through an AssetProvider and keeps a texture per ID.
#pragma once

#include <cstdint>
#include <map>
#include <optional>
#include <utility>
#include <vector>

#include <SDL3/SDL.h>

#include "formats/palette.h"
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
    // Only the top `visible_height` screen units of it (a figure standing
    // in water shows less of itself).
    bool draw_top(const AssetId& id, int x, int y, int visible_height);
    // The same, with every pixel's colour multiplied by (r, g, b): a white
    // glyph drawn this way comes out in that colour.
    bool draw_tinted(const AssetId& id, int x, int y, std::uint8_t r, std::uint8_t g, std::uint8_t b);
    // XORs `colour` into the palette index of every pixel drawn so far in a
    // rectangle (320x200 screen units), going by what each pixel shows: the
    // fallback for new art (map_layer's draw_xor_box works on the original's
    // colour numbers). A colour shown by two numbers takes the first, and a
    // colour not in `palette` is left as it is.
    bool xor_colours(int x, int y, int width, int height, int colour, const formats::Palette16& palette);
    // The provider's picture for `id` (kept after the first call), or null.
    const Image* source(const AssetId& id);
    // Draws `rgba` (width x height pixels) over a rectangle of the screen.
    void draw_rgba(int x, int y, int width, int height, const std::vector<std::uint8_t>& rgba);
    // A solid rectangle, in 320x200 screen units.
    void fill(int x, int y, int width, int height, std::uint8_t r, std::uint8_t g, std::uint8_t b);
    // Limits drawing to a rectangle of the 320x200 screen; clear_clip() lifts it.
    void clip(int x, int y, int width, int height);
    void clear_clip();
    void end_frame();
    // The size `id` covers on the 320x200 screen, or nothing if no provider has it.
    std::optional<std::pair<int, int>> logical_size(const AssetId& id);

private:
    SDL_Texture* texture_for(const AssetId& id);

    SDL_Renderer* sdl_;
    AssetProvider& assets_;
    struct Entry {
        SDL_Texture* texture = nullptr;  // null: the provider had nothing
        float logical_width = 0, logical_height = 0;
    };
    std::map<AssetId, Entry> cache_;
    std::map<AssetId, std::optional<Image>> sources_;
};

}  // namespace chr::platform
