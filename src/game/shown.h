// What the screen shows while the logic runs on without the player: the
// fight's shots with combat graphics, and the pauses after its messages
// (docs/combat.md 3.14, docs/resuming.md). The logic records each moment
// as an event and runs on; the platform plays them at its own pace when
// the logic next stops for input.
#pragma once

#include <optional>
#include <vector>

#include "game/asset_id.h"
#include "game/figures.h"
#include "game/map_view.h"
#include "game/text.h"

namespace chr::game {

// A place's picture in the upper left box (1000:48b7): O<number>.ANM.
struct PlacePicture {
    int number = 0;
    bool operator==(const PlacePicture&) const = default;
};

// A sprite drawn over the view at a point of the 320x200 screen (its
// top-left corner): a missile or an explosion (1e31:1f73).
struct ShownSprite {
    AssetId sprite;
    int x = 0, y = 0;
};

// A pixel of a line shot, in a colour number of the palette, plotted
// straight onto the screen (273d:031c).
struct ShownPixel {
    int x = 0, y = 0;
    int colour = 0;
};

// The screen at one moment: the view and its figures as last shown, the
// sprites drawn over them, the line pixels plotted on top, the text, and
// the place picture.
struct Shown {
    MapView view;
    std::vector<Figure> figures;
    std::vector<ShownSprite> sprites;
    std::vector<ShownPixel> pixels;
    TextScreen text;
    std::optional<PlacePicture> picture;
};

// One event: a sound started (1000:19bf, its number), or the screen shown
// and held for `frames` display frames (273d:0006 counts them).
struct ShownEvent {
    std::optional<Shown> screen;
    int frames = 0;
    int sound = -1;
};

}  // namespace chr::game
