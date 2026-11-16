// The original's text screen: 40x25 character cells of 8x8 pixels, written
// through numbered text windows (docs/text.md). The logic only says which
// glyph, frame piece or fill colour goes in which cell; the renderer decides
// how a cell looks, so a makeover can change the font and frames.
#pragma once

#include <array>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>

#include "game/asset_id.h"

namespace chr::game {

// The grid is the original's layout in character cells; how big a cell is
// on screen is the renderer's business (it measures the font).
inline constexpr int kTextColumns = 40;
inline constexpr int kTextRows = 25;

// Glyph ids: the font is read from the player's BTECH.EXE (formats/exe_data.h).
inline AssetId font_glyph(std::uint8_t ch) { return {"FONT", std::uint16_t(ch & 0x7f)}; }
// Frame pieces: the 8x8 cells the program cuts from the top of BTBORDER.CMP
// (1527:0044, with 2656:0ae5).
inline AssetId frame_piece(int n) { return {"BTBORDER.CMP:8x8", std::uint16_t(n)}; }

struct TextCell {
    enum class Kind : std::uint8_t { Empty, Fill, Glyph, Frame };
    Kind kind = Kind::Empty;  // Empty: whatever is drawn underneath shows
    std::uint8_t code = 0;    // character (Glyph) or frame piece (Frame)
    std::uint8_t fg = 15;     // colour numbers 0..15
    std::uint8_t bg = 0;
    AssetId asset() const { return kind == Kind::Frame ? frame_piece(code) : font_glyph(code); }
};

// One text window, as the program keeps it (a 16-byte record per window).
struct TextWindow {
    int x = 0, y = 0;           // top-left inside cell
    int width = 0, height = 0;  // in cells
    int fg = 15, bg = 0;
    int col = 0, row = 0;       // cursor
};

class TextScreen {
public:
    TextScreen();

    const TextCell& cell(int col, int row) const { return cells_[row * kTextColumns + col]; }
    const TextWindow& window() const { return windows_[current_]; }
    int window_number() const { return current_; }

    // BTECH.EXE 2656:0281 (Ghidra FUN_2656_0281)
    // Makes window n current, keeping the old window's colours and cursor.
    void select_window(int n);
    // BTECH.EXE 2656:0388 (Ghidra FUN_2656_0388)
    // Fills the current window with its background colour; cursor to its corner.
    void clear_window();
    // BTECH.EXE 2656:0004 (Ghidra FUN_2656_0004)
    // Draws frame style `style` around the current window, one cell outside it.
    void frame(int style);
    // BTECH.EXE 2656:03f5 (Ghidra FUN_2656_03f5)
    // Prints the program's coded text into the current window, word-wrapped.
    void print(std::span<const std::uint8_t> text);
    void print(std::string_view text);
    // BTECH.EXE 273d:00d5 (Ghidra FUN_273d_00d5)
    // Writes characters straight into cells from (col, row), no wrapping.
    void put_text(std::string_view text, int col, int row, int fg, int bg);
    // Forgets everything (the whole screen shows what is underneath).
    void clear_all();
    // Empties a rectangle of cells, for when the map is drawn over them.
    void forget(int col, int row, int width, int height);
    // Fills a rectangle of cells with one colour.
    void fill(int col, int row, int width, int height, int colour);

    // The window's current colours: the yes/no question sets and restores them.
    void set_fg(int fg) { windows_[current_].fg = fg; }
    // Puts the current window's cursor at (col, row) inside it
    // (DGROUP:3748/374e for the current window).
    void set_cursor(int col, int row) {
        windows_[current_].col = col;
        windows_[current_].row = row;
    }
    // Moves the cursor up a row (the yes/no question rewrites its line).
    void cursor_up() { --windows_[current_].row; }

    // Every cell, for tests and the renderer.
    std::span<const TextCell> cells() const { return cells_; }
    // The characters of one row, '\0' cells as spaces, for tests.
    std::string row_text(int row) const;

private:
    // BTECH.EXE 2656:07cb (Ghidra FUN_2656_07cb)
    void put_line(std::string& line, bool newline);
    void put_piece(int piece, int col, int row);

    std::array<TextCell, kTextColumns * kTextRows> cells_{};
    std::array<TextWindow, 11> windows_;
    int current_ = 0;
    bool selected_once_ = false;
};

}  // namespace chr::game
