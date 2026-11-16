// The default asset provider: decodes the player's original files.
#pragma once

#include <filesystem>
#include <map>
#include <memory>
#include <string>

#include "formats/cmp.h"
#include "formats/exe_data.h"
#include "formats/icn.h"
#include "platform/asset_provider.h"

namespace chr::platform {

class OriginalAssets final : public AssetProvider {
public:
    explicit OriginalAssets(std::filesystem::path game_dir);
    // Also answers {"FONT", c}, the font's character c read from BTECH.EXE
    // (white on transparent, for the renderer to colour), and
    // {"NAME.CMP:8x8", n}, cell n of that picture cut into 8x8 cells.
    std::optional<Image> image(const AssetId& id) override;

    // The checked program, loaded the first time something needs it; null
    // (with last_error() set) if it is missing or not the supported version.
    const formats::OriginalExe* exe();

    // Why the last image() call returned nothing (empty after a success).
    const std::string& last_error() const { return last_error_; }

private:
    const formats::TileSheet* sheet(const std::string& file);

    std::filesystem::path game_dir_;
    std::map<std::string, formats::TileSheet> sheets_;  // decoded once per file
    std::map<std::string, formats::CmpPicture> pictures_;
    std::unique_ptr<formats::OriginalExe> exe_;
    std::string exe_error_;
    std::string last_error_;
};

}  // namespace chr::platform
