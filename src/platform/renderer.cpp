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

void Renderer::end_frame() { SDL_RenderPresent(sdl_); }

}  // namespace chr::platform
