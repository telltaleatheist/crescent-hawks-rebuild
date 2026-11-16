#include "game/text_provider.h"

#include <stdexcept>

#include "formats/game_files.h"
#include "formats/json.h"

namespace chr::game {

std::optional<std::string> TextTable::find(std::string_view id) const {
    if (const auto it = texts_.find(id); it != texts_.end()) return it->second;
    return std::nullopt;
}

TextTable TextTable::parse(std::string_view json) {
    TextTable table;
    const auto doc = formats::parse_json(json);
    const auto* texts = doc.find("texts");
    if (!texts) throw formats::JsonError("text file: no \"texts\" list");
    for (const auto& entry : texts->array()) {
        const auto* id = entry.find("id");
        const auto* text = entry.find("text");
        if (!id || !text) throw formats::JsonError("text file: an entry without \"id\" and \"text\"");
        table.set(id->string(), text->string());
    }
    return table;
}

TextTable TextTable::load(const std::filesystem::path& path) {
    const auto bytes = formats::read_file(path);
    try {
        return parse(std::string(bytes.begin(), bytes.end()));
    } catch (const formats::JsonError& e) {
        throw formats::JsonError(path.string() + ": " + e.what());
    }
}

std::string place_text_id(int map_number, int place) {
    return "map" + std::to_string(map_number) + ".place." + std::to_string(place);
}

MapText::MapText(const formats::MtpMap& map, int map_number) {
    for (std::size_t e = 0; e < map.places.size(); ++e)
        table_.set(place_text_id(map_number, int(e)), map.places[e]);
    for (std::size_t p = 0; p < map.people.size(); ++p)
        table_.set("map" + std::to_string(map_number) + ".person." + std::to_string(p), map.people[p]);
}

std::optional<std::string> MapText::find(std::string_view id) const { return table_.find(id); }

std::optional<std::string> LayeredText::find(std::string_view id) const {
    for (const auto* layer : layers_)
        if (auto t = layer->find(id)) return t;
    return std::nullopt;
}

std::string lookup(const TextProvider& provider, std::string_view id) {
    if (auto t = provider.find(id)) return *t;
    throw std::out_of_range("no text for ID " + std::string(id));
}

}  // namespace chr::game
