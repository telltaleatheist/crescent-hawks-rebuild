#include "game/asset_id.h"

namespace chr {

std::string to_string(const AssetId& id) {
    return id.file + "#" + std::to_string(id.index);
}

}  // namespace chr
