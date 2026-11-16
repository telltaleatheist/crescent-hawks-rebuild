// The importer: reads the player's own BTECH.EXE once (checked as DOS 1.03)
// and writes the data the game needs from it as plain files in the player's
// data folder, never in the repo:
//
//   import.json          what was imported, by which import version
//   text/english.json    every text: id, its address in the program (or a
//                        .BLD file), text
//   font/font.txt        the font (formats/font_file.h)
//   rules/*.json         the rule tables (formats/rules.h)
//
// From then on the game reads only those files; the program is needed only
// to import. Overrides (a translation, a mod) are more text files of the same
// form, layered on top (game/text_provider.h).
#pragma once

#include <filesystem>
#include <span>
#include <string>

#include "formats/exe_data.h"

namespace chr::formats {

// Bump when the files change shape or the import table gains pieces: a data
// folder from an older import is imported again.
// (A change to the import table alone needs no bump: import_table_hash.)
inline constexpr int kImportVersion = 9;  // 2: the direction names; 3: "C-Bills:"; 4: the .BLD texts;
                                        // 5: the class and talk texts; 6: the start-up's;
                                        // 7: the rule tables, 'Mech and weapon names, combat texts;
                                        // 8: the table hash in import.json; 9: rules/quiz.json

std::filesystem::path imported_text_path(const std::filesystem::path& data_dir);
std::filesystem::path imported_font_path(const std::filesystem::path& data_dir);

// A hash of the import table (OriginalExe::table()): every piece's kind,
// ID, address, size and expected hash. import.json records it, and a data
// folder imported with another table is imported again, so a piece added
// to the table can't be missed by an older folder.
std::string import_table_hash(std::span<const ExeSpan> table);
std::string import_table_hash();

enum class ImportState { Current, Missing, Outdated };
ImportState import_state(const std::filesystem::path& data_dir);

// Imports from `game_dir` into `data_dir`. Throws FormatError (a missing or
// unknown BTECH.EXE) or std::filesystem errors.
void import_original(const std::filesystem::path& game_dir, const std::filesystem::path& data_dir);

// Imports if the data folder has no import or an older one; true if it did.
// If that is needed and impossible, throws FormatError saying how to import.
bool ensure_imported(const std::filesystem::path& game_dir, const std::filesystem::path& data_dir);

}  // namespace chr::formats
