// The default asset provider: decodes the player's original files.
#pragma once

#include <filesystem>
#include <map>
#include <string>

#include "formats/icn.h"
#include "platform/asset_provider.h"

namespace chr::platform {

class OriginalAssets final : public AssetProvider {
public:
    explicit OriginalAssets(std::filesystem::path game_dir);
    std::optional<Image> image(const AssetId& id) override;

    // Why the last image() call returned nothing (empty after a success).
    const std::string& last_error() const { return last_error_; }

private:
    const formats::TileSheet* sheet(const std::string& file);

    std::filesystem::path game_dir_;
    std::map<std::string, formats::TileSheet> sheets_;  // decoded once per file
    std::string last_error_;
};

}  // namespace chr::platform
