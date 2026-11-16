#include "platform/renderer.h"

namespace chr::platform {

Renderer::Renderer(SDL_Renderer* sdl, AssetProvider& assets) : sdl_(sdl), assets_(assets) {
    SDL_SetRenderLogicalPresentation(sdl_, kScreenWidth, kScreenHeight,
                                     SDL_LOGICAL_PRESENTATION_INTEGER_SCALE);
    SDL_SetDefaultTextureScaleMode(sdl_, SDL_SCALEMODE_NEAREST);
}

Renderer::~Renderer() {
    for (auto& [id, entry] : cache_)
        if (entry.texture) SDL_DestroyTexture(entry.texture);
}

void Renderer::begin_frame() {
    SDL_SetRenderDrawColor(sdl_, 0, 0, 0, 255);
    SDL_RenderClear(sdl_);
}

SDL_Texture* Renderer::texture_for(const AssetId& id) {
    if (auto it = cache_.find(id); it != cache_.end()) return it->second.texture;
    Entry entry;
    if (auto image = assets_.image(id)) {
        entry.texture = SDL_CreateTexture(sdl_, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STATIC,
                                          image->width, image->height);
        if (entry.texture) {
            SDL_UpdateTexture(entry.texture, nullptr, image->rgba.data(), image->width * 4);
            SDL_SetTextureScaleMode(entry.texture, SDL_SCALEMODE_NEAREST);
            SDL_SetTextureBlendMode(entry.texture, SDL_BLENDMODE_BLEND);
            entry.logical_width = float(image->logical_width);
            entry.logical_height = float(image->logical_height);
        }
    }
    return cache_.emplace(id, entry).first->second.texture;
}

bool Renderer::draw(const AssetId& id, int x, int y) {
    SDL_Texture* texture = texture_for(id);
    if (!texture) return false;
    const Entry& entry = cache_.at(id);
    const SDL_FRect dest{float(x), float(y), entry.logical_width, entry.logical_height};
    return SDL_RenderTexture(sdl_, texture, nullptr, &dest);
}

bool Renderer::draw_top(const AssetId& id, int x, int y, int visible_height) {
    SDL_Texture* texture = texture_for(id);
    if (!texture) return false;
    const Entry& entry = cache_.at(id);
    if (visible_height >= entry.logical_height) return draw(id, x, y);
    if (visible_height <= 0) return true;
    float tw = 0, th = 0;
    SDL_GetTextureSize(texture, &tw, &th);
    const SDL_FRect src{0, 0, tw, th * float(visible_height) / entry.logical_height};
    const SDL_FRect dest{float(x), float(y), entry.logical_width, float(visible_height)};
    return SDL_RenderTexture(sdl_, texture, &src, &dest);
}

bool Renderer::draw_tinted(const AssetId& id, int x, int y, std::uint8_t r, std::uint8_t g,
                           std::uint8_t b) {
    SDL_Texture* texture = texture_for(id);
    if (!texture) return false;
    SDL_SetTextureColorMod(texture, r, g, b);
    const bool ok = draw(id, x, y);
    SDL_SetTextureColorMod(texture, 255, 255, 255);
    return ok;
}

void Renderer::fill(int x, int y, int width, int height, std::uint8_t r, std::uint8_t g,
                    std::uint8_t b) {
    SDL_SetRenderDrawColor(sdl_, r, g, b, 255);
    const SDL_FRect rect{float(x), float(y), float(width), float(height)};
    SDL_RenderFillRect(sdl_, &rect);
}

void Renderer::clip(int x, int y, int width, int height) {
    const SDL_Rect rect{x, y, width, height};
    SDL_SetRenderClipRect(sdl_, &rect);
}

void Renderer::clear_clip() { SDL_SetRenderClipRect(sdl_, nullptr); }

std::optional<std::pair<int, int>> Renderer::logical_size(const AssetId& id) {
    if (!texture_for(id)) return std::nullopt;
    const Entry& entry = cache_.at(id);
    return std::pair{int(entry.logical_width), int(entry.logical_height)};
}

void Renderer::end_frame() { SDL_RenderPresent(sdl_); }

}  // namespace chr::platform
