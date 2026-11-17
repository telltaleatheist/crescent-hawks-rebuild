// The text seam, the same idea as asset IDs for pictures: the game logic
// names every piece of text it shows by a text ID ("walk.too_deep") and asks
// a provider for it, never carrying the words itself. The default texts are
// the ones the importer wrote from the player's BTECH.EXE
// (text/english.json, formats/import.h) and the names in the map files
// (MapText). An override file of the same form, layered on top, replaces any
// ID, for a translation or a rewrite. Under them all is the repo's default
// interface text (kInterfaceTextFile): the menus, prompts, labels and rule
// messages only, never story, dialogue, names or descriptions (docs/text.md,
// "Interface text and imported text").
//
// A text is the program's own bytes: it may hold the printer's control codes
// (docs/text.md), so an override can use them too, written as \u00NN.
#pragma once

#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "formats/mtp.h"

namespace chr::game {

class TextProvider {
public:
    virtual ~TextProvider() = default;
    // The text for `id`, or nothing if this provider doesn't have it.
    virtual std::optional<std::string> find(std::string_view id) const = 0;
};

// Texts held in memory, e.g. read from an override file.
class TextTable final : public TextProvider {
public:
    std::optional<std::string> find(std::string_view id) const override;
    void set(std::string id, std::string text) { texts_[std::move(id)] = std::move(text); }
    std::size_t size() const { return texts_.size(); }

    // A text file: {"texts": [{"id": "...", "text": "..."}, ...]} (other
    // members, like the imported "address", are ignored). Throws
    // formats::JsonError naming the problem.
    static TextTable parse(std::string_view json);
    static TextTable load(const std::filesystem::path& path);

private:
    std::map<std::string, std::string, std::less<>> texts_;
};

// The names in a map file: "mapN.place.E" (the place behind entrance E) and
// "mapN.person.P".
class MapText final : public TextProvider {
public:
    MapText() = default;
    MapText(const formats::MtpMap& map, int map_number);
    // Every map's names can be held at once, since a map loads mid-game.
    void add(const formats::MtpMap& map, int map_number);
    std::optional<std::string> find(std::string_view id) const override;

private:
    TextTable table_;
};

// Asks each provider in turn, the first (overrides) first.
class LayeredText final : public TextProvider {
public:
    explicit LayeredText(std::vector<const TextProvider*> layers) : layers_(std::move(layers)) {}
    std::optional<std::string> find(std::string_view id) const override;

private:
    std::vector<const TextProvider*> layers_;
};

// The text for `id`; throws std::out_of_range naming the ID if no provider has it.
std::string lookup(const TextProvider& provider, std::string_view id);

std::string place_text_id(int map_number, int place);

// The repo's default interface texts, relative to the program's folder (the
// build copies data/ there).
inline constexpr const char* kInterfaceTextFile = "data/text/interface.json";

}  // namespace chr::game
