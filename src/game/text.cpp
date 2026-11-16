#include "game/text.h"

#include <algorithm>

namespace chr::game {

namespace {

// The window table, 385b:0000: x, y, width, height, colours. (Layout data,
// kept as a constant: it is where the program puts its boxes, not content.)
//   0 whole screen        3 lower left box (party)  6 big box right of panel
//   1 middle box          4 upper left box          7 message box over the map
//   2 whole screen inset  5 large box               8 left strip, 9/10 dialogs
constexpr std::array<TextWindow, 11> kWindows = {{
    {0, 0, 40, 25, 15, 0, 0, 0},
    {14, 15, 25, 9, 15, 0, 0, 0},
    {1, 1, 38, 23, 15, 0, 0, 0},
    {1, 13, 11, 11, 15, 0, 0, 0},
    {1, 1, 11, 11, 15, 0, 0, 0},
    {10, 1, 29, 16, 15, 0, 0, 0},
    {14, 1, 25, 23, 15, 0, 0, 0},
    {14, 10, 25, 5, 15, 0, 0, 0},
    {0, 0, 16, 25, 15, 0, 0, 0},
    {1, 2, 38, 20, 0, 15, 14, 0},
    {0, 0, 25, 8, 0, 15, 14, 0},
}};

// Frame styles, 385b:0320/0338/0350 (pointers at DGROUP:4fa4): the four
// corners (top-left, top-right, bottom-left, bottom-right), then the top,
// left, right and bottom edges, each a run of pieces ended by 0xff and
// repeated along the edge.
constexpr std::uint8_t kEnd = 0xff;
constexpr std::array<std::uint8_t, 12> kStyleA = {0, 1, 6, 7, 4, kEnd, 5, kEnd, 5, kEnd, 4, kEnd};
constexpr std::array<std::uint8_t, 12> kStyleB = {2, 3, 6, 7, 4, kEnd, 5, kEnd, 5, kEnd, 4, kEnd};
constexpr std::array<std::uint8_t, 12> kStyleC = {0, 1, 2, 3, 4, kEnd, 5, kEnd, 5, kEnd, 4, kEnd};
// Styles 0-2 use A, 3 uses B, 4 uses C (DGROUP:4fa4).
const std::uint8_t* style_table(int style) {
    if (style == 3) return kStyleB.data();
    if (style == 4) return kStyleC.data();
    return kStyleA.data();
}

// BTECH.EXE 2656:01e7 (Ghidra FUN_2656_01e7)
// Lays a run of frame pieces along an edge; returns where the next run starts.
template <class Put>
int edge(const std::uint8_t* table, int start, int col, int row, int count, bool across, Put put) {
    int i = start;
    for (int n = 0; n < count; ++n) {
        put(table[i], col, row);
        if (across) ++col; else ++row;
        ++i;
        if (table[i] == kEnd) i = start;
    }
    while (table[i] != kEnd) ++i;
    return i + 1;
}

}  // namespace

TextScreen::TextScreen() : windows_(kWindows) {}

void TextScreen::select_window(int n) {
    // The first call has nothing to keep (DGROUP:4fa2).
    selected_once_ = true;
    current_ = std::clamp(n, 0, int(windows_.size()) - 1);
}

void TextScreen::clear_window() {
    auto& w = windows_[current_];
    fill(w.x, w.y, w.width, w.height, w.bg);
    w.col = 0;
    w.row = 0;
}

void TextScreen::put_piece(int piece, int col, int row) {
    if (col < 0 || row < 0 || col >= kTextColumns || row >= kTextRows) return;
    cells_[row * kTextColumns + col] = {TextCell::Kind::Frame, std::uint8_t(piece), 15, 0};
}

void TextScreen::frame(int style) {
    const auto& w = windows_[current_];
    const auto* t = style_table(style);
    put_piece(t[0], w.x - 1, w.y - 1);
    put_piece(t[1], w.x + w.width, w.y - 1);
    put_piece(t[2], w.x - 1, w.y + w.height);
    put_piece(t[3], w.x + w.width, w.y + w.height);
    auto put = [this](int p, int c, int r) { put_piece(p, c, r); };
    int next = edge(t, 4, w.x, w.y - 1, w.width, true, put);
    next = edge(t, next, w.x - 1, w.y, w.height, false, put);
    next = edge(t, next, w.x + w.width, w.y, w.height, false, put);
    edge(t, next, w.x, w.y + w.height, w.width, true, put);
}

void TextScreen::put_text(std::string_view text, int col, int row, int fg, int bg) {
    for (const char ch : text) {
        if (col >= kTextColumns) break;  // (the original wraps to the next screen row)
        if (row >= 0 && row < kTextRows && col >= 0)
            cells_[row * kTextColumns + col] = {TextCell::Kind::Glyph,
                                                std::uint8_t(std::uint8_t(ch) & 0x7f),
                                                std::uint8_t(fg), std::uint8_t(bg)};
        ++col;
    }
}

void TextScreen::fill(int col, int row, int width, int height, int colour) {
    for (int r = row; r < row + height; ++r)
        for (int c = col; c < col + width; ++c)
            if (c >= 0 && r >= 0 && c < kTextColumns && r < kTextRows)
                cells_[r * kTextColumns + c] = {TextCell::Kind::Fill, 0, 15, std::uint8_t(colour)};
}

void TextScreen::forget(int col, int row, int width, int height) {
    for (int r = row; r < row + height; ++r)
        for (int c = col; c < col + width; ++c)
            if (c >= 0 && r >= 0 && c < kTextColumns && r < kTextRows)
                cells_[r * kTextColumns + c] = {};
}

void TextScreen::clear_all() { cells_.fill({}); }

// One line into the window at the cursor. Past the bottom the window scrolls
// up a row and the line goes on the bottom row, padded to the width and
// starting at the window's left edge, whatever the cursor column was.
void TextScreen::put_line(std::string& line, bool newline) {
    auto& w = windows_[current_];
    if (w.row < w.height) {
        put_text(line, w.x + w.col, w.y + w.row, w.fg, w.bg);
        if (!newline) {
            w.col += int(line.size());
            if (w.col < w.width) {
                line.clear();
                return;
            }
        }
    } else {
        if (int(line.size()) < w.width) line.resize(std::size_t(w.width), ' ');
        line.resize(std::size_t(w.width));
        for (int r = w.y; r < w.y + w.height - 1; ++r)
            for (int c = w.x; c < w.x + w.width; ++c)
                cells_[r * kTextColumns + c] = cells_[(r + 1) * kTextColumns + c];
        fill(w.x, w.y + w.height - 1, w.width, 1, 0);  // cleared to colour 0, not the window's
        put_text(line, w.x, w.y + w.height - 1, w.fg, w.bg);
        if (!newline) w.row = w.height - 1;
    }
    w.col = 0;
    ++w.row;
    line.clear();
}

void TextScreen::print(std::string_view text) {
    print(std::span(reinterpret_cast<const std::uint8_t*>(text.data()), text.size()));
}

void TextScreen::print(std::span<const std::uint8_t> s) {
    auto& w = windows_[current_];
    std::string word, line;
    int line_col = w.col;  // where `line` starts
    bool last = false;
    std::size_t i = 0;
    auto next_byte = [&]() -> std::uint8_t { return i < s.size() ? s[i++] : 0; };
    // Prints what is pending, the word joining the line if it fits.
    auto flush = [&](bool newline) {
        if (int(word.size()) + line_col <= w.width) {
            line += word;
            put_line(line, newline);
        } else {
            put_line(line, true);
            put_line(word, newline);
        }
        word.clear();
        line_col = w.col;
    };
    while (!last) {
        std::uint8_t c = next_byte();
        if (c == 0) break;
        if (c & 0x80) {  // the last character of a piece of text
            c &= 0x7f;
            last = true;
        }
        if (c == 0x0d) {
            flush(true);
        } else if (c == 0x02) {  // background colour: the next byte
            flush(false);
            w.bg = next_byte();
        } else if (c == 0x06) {  // foreground colour: the next byte
            flush(false);
            w.fg = next_byte();
        } else if (c == 0x09) {  // cursor to the column in the next byte
            flush(false);
            w.col = next_byte();
            if (w.col >= w.width) w.col = 0;
            line_col = w.col;
        } else if (c == 0x13) {  // pad with spaces up to the column in the next byte
            int to = next_byte();
            if (w.col < to) {  // (it counts from the cursor, not from the end of the line)
                for (; w.col < to; --to) word += ' ';
                line += word;
                line_col += int(word.size());
                word.clear();
            }
        } else if (c == ' ') {
            const int at = int(word.size()) + line_col;
            if (at > w.width) {  // the word goes on the next line
                word += ' ';
                put_line(line, true);
                line = word;
                while (i < s.size() && s[i] == ' ') ++i;
                line_col = int(word.size());
            } else {
                if (at != w.width && at != 0) word += ' ';
                line += word;
                line_col += int(word.size());
            }
            word.clear();
        } else if (int(word.size()) < w.width - 1) {
            word += char(c);
        } else {  // a word as wide as the window is broken
            if (!line.empty()) put_line(line, true);
            word += char(c);
            line += word;
            put_line(line, true);
            word.clear();
            line_col = w.col;
        }
    }
    if (!word.empty() || !line.empty()) {
        if (int(word.size()) + line_col <= w.width) {
            line += word;
            put_line(line, false);
        } else {
            put_line(line, true);
            put_line(word, false);
        }
    }
}

std::string TextScreen::row_text(int row) const {
    std::string out;
    for (int c = 0; c < kTextColumns; ++c) {
        const auto& cell = cells_[row * kTextColumns + c];
        out += cell.kind == TextCell::Kind::Glyph && cell.code >= 0x20 ? char(cell.code) : ' ';
    }
    return out;
}

}  // namespace chr::game
