#include "platform/renderer.h"

#include <algorithm>
#include <vector>

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

bool Renderer::xor_colours(int x, int y, int width, int height, int colour, const formats::Palette16& palette) {
    // The output may be scaled: read the rectangle at the output's size and
    // take one pixel per screen unit.
    SDL_FRect shown{};
    if (!SDL_GetRenderLogicalPresentationRect(sdl_, &shown) || shown.w <= 0) return false;
    const float scale = shown.w / float(kScreenWidth);
    SDL_SetRenderLogicalPresentation(sdl_, 0, 0, SDL_LOGICAL_PRESENTATION_DISABLED);
    const SDL_Rect area{int(shown.x + x * scale), int(shown.y + y * scale), int(width * scale), int(height * scale)};
    SDL_Surface* read = SDL_RenderReadPixels(sdl_, &area);
    SDL_SetRenderLogicalPresentation(sdl_, kScreenWidth, kScreenHeight, SDL_LOGICAL_PRESENTATION_INTEGER_SCALE);
    if (!read) return false;
    SDL_Surface* rgba = SDL_ConvertSurface(read, SDL_PIXELFORMAT_RGBA32);
    SDL_DestroySurface(read);
    if (!rgba) return false;
    std::vector<std::uint8_t> out(std::size_t(width * height * 4));
    for (int py = 0; py < height; ++py)
        for (int px = 0; px < width; ++px) {
            const int sx = std::min(rgba->w - 1, int(px * scale)), sy = std::min(rgba->h - 1, int(py * scale));
            const auto* p = static_cast<const std::uint8_t*>(rgba->pixels) + sy * rgba->pitch + sx * 4;
            formats::Rgb c{p[0], p[1], p[2]};
            for (int i = 0; i < 16; ++i)
                if (palette[std::size_t(i)] == c) {
                    c = palette[std::size_t(i ^ (colour & 15))];
                    break;
                }
            auto* q = &out[std::size_t((py * width + px) * 4)];
            q[0] = c.r, q[1] = c.g, q[2] = c.b, q[3] = 255;
        }
    SDL_DestroySurface(rgba);
    SDL_Texture* t = SDL_CreateTexture(sdl_, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STATIC, width, height);
    if (!t) return false;
    SDL_SetTextureScaleMode(t, SDL_SCALEMODE_NEAREST);
    SDL_UpdateTexture(t, nullptr, out.data(), width * 4);
    const SDL_FRect dst{float(x), float(y), float(width), float(height)};
    SDL_RenderTexture(sdl_, t, nullptr, &dst);
    SDL_DestroyTexture(t);
    return true;
}

const Image* Renderer::source(const AssetId& id) {
    if (auto it = sources_.find(id); it != sources_.end()) return it->second ? &*it->second : nullptr;
    auto& slot = sources_[id];
    slot = assets_.image(id);
    return slot ? &*slot : nullptr;
}

void Renderer::draw_rgba(int x, int y, int width, int height, const std::vector<std::uint8_t>& rgba) {
    SDL_Texture* t = SDL_CreateTexture(sdl_, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STATIC, width, height);
    if (!t) return;
    SDL_SetTextureScaleMode(t, SDL_SCALEMODE_NEAREST);
    SDL_UpdateTexture(t, nullptr, rgba.data(), width * 4);
    const SDL_FRect dst{float(x), float(y), float(width), float(height)};
    SDL_RenderTexture(sdl_, t, nullptr, &dst);
    SDL_DestroyTexture(t);
}

void Renderer::end_frame() { SDL_RenderPresent(sdl_); }

}  // namespace chr::platform
