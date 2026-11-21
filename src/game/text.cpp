#include "game/text.h"

#include <algorithm>

namespace chr::game {

namespace {

// The frame piece table for `style` (DGROUP:4fa4), from the imported tables.
const int* style_table(const formats::Tables& tables, int style) {
    style = std::clamp(style, 0, formats::kFrameStyleCount - 1);
    return tables.frame_pieces[std::size_t(tables.frame_style_pieces[std::size_t(style)])].data();
}

// BTECH.EXE 2656:01e7 (Ghidra FUN_2656_01e7)
// Lays a run of frame pieces along an edge; returns where the next run starts.
template <class Put>
int edge(const int* table, int start, int col, int row, int count, bool across, Put put) {
    int i = start;
    for (int n = 0; n < count; ++n) {
        put(table[i], col, row);
        if (across) ++col; else ++row;
        ++i;
        if (table[i] == formats::kFramePieceEnd) i = start;
    }
    while (table[i] != formats::kFramePieceEnd) ++i;
    return i + 1;
}

}  // namespace

// The window table was part of BTECH.EXE (385b:0000); it is now imported
// into tables.json (formats/tables.h).
TextScreen::TextScreen(const formats::Tables& tables) : tables_(&tables) {
    for (std::size_t n = 0; n < windows_.size(); ++n) {
        const auto& r = tables.windows[n];
        windows_[n] = TextWindow{r.x, r.y, r.width, r.height, TextColour::data(r.fg), TextColour::data(r.bg), r.col, r.row};
    }
}

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
    cells_[row * kTextColumns + col] = {TextCell::Kind::Frame, std::uint8_t(piece)};
    painted_[std::size_t(row * kTextColumns + col)] = paint_clock_;
}

void TextScreen::frame(int style) {
    const auto& w = windows_[current_];
    const auto* t = style_table(*tables_, style);
    ++paint_clock_;
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

void TextScreen::put_text(std::string_view text, int col, int row, TextColour fg, TextColour bg) {
    ++paint_clock_;
    for (const char ch : text) {
        if (col >= kTextColumns) break;  // (the original wraps to the next screen row)
        if (row >= 0 && row < kTextRows && col >= 0) {
            cells_[row * kTextColumns + col] = {TextCell::Kind::Glyph,
                                                std::uint8_t(std::uint8_t(ch) & 0x7f),
                                                fg, bg};
            painted_[std::size_t(row * kTextColumns + col)] = paint_clock_;
        }
        ++col;
    }
}

void TextScreen::fill(int col, int row, int width, int height, TextColour colour) {
    ++paint_clock_;
    for (int r = row; r < row + height; ++r)
        for (int c = col; c < col + width; ++c)
            if (c >= 0 && r >= 0 && c < kTextColumns && r < kTextRows) {
                cells_[r * kTextColumns + c] = {TextCell::Kind::Fill, 0, TextRole::Text, colour};
                painted_[std::size_t(r * kTextColumns + c)] = paint_clock_;
            }
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
        const int length = int(line.size());
        if (int(line.size()) < w.width) line.resize(std::size_t(w.width), ' ');
        line.resize(std::size_t(w.width));
        ++paint_clock_;
        for (int r = w.y; r < w.y + w.height - 1; ++r)
            for (int c = w.x; c < w.x + w.width; ++c) {
                cells_[r * kTextColumns + c] = cells_[(r + 1) * kTextColumns + c];
                painted_[std::size_t(r * kTextColumns + c)] = paint_clock_;
            }
        fill(w.x, w.y + w.height - 1, w.width, 1, TextRole::Background);  // not the window's colour
        put_text(line, w.x, w.y + w.height - 1, w.fg, w.bg);
        if (!newline) {
            // Without a line break the cursor stays after the text on the
            // bottom row, as above.
            w.row = w.height - 1;
            w.col = length;
            if (w.col < w.width) {
                line.clear();
                return;
            }
        }
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
            w.bg = TextColour::data(next_byte());
        } else if (c == 0x06) {  // foreground colour: the next byte
            flush(false);
            w.fg = TextColour::data(next_byte());
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

void TextScreen::menu_bar(int col, int row, int width, int menu) {
    if (row < 0 || row >= kTextRows) return;
    const auto mark = std::uint8_t(menu + 1);
    for (int c = col; c < col + width && c < kTextColumns; ++c) {
        if (c < 0) continue;
        TextCell& cell = cells_[row * kTextColumns + c];
        if (cell.kind == TextCell::Kind::Empty) {  // as in xor_bar
            cell.kind = TextCell::Kind::Fill;
            cell.bg = TextRole::Background;
        }
        // Twice on a cell is off; two menus' bars on one cell both show.
        if (cell.bars[0] == mark) cell.bars[0] = 0;
        else if (cell.bars[1] == mark) cell.bars[1] = 0;
        else if (cell.bars[0] == 0) cell.bars[0] = mark;
        else cell.bars[1] = mark;
    }
}

void TextScreen::xor_bar(int col, int row, int width, int colour) {
    if (row < 0 || row >= kTextRows) return;
    for (int c = col; c < col + width && c < kTextColumns; ++c) {
        if (c < 0) continue;
        TextCell& cell = cells_[row * kTextColumns + c];
        // An empty cell shows what is underneath (the map); the program
        // never puts a bar there. As black under the bar, it would turn
        // the colour itself.
        if (cell.kind == TextCell::Kind::Empty) {
            cell.kind = TextCell::Kind::Fill;
            cell.bg = TextRole::Background;
        }
        cell.xor_mask = static_cast<std::uint8_t>((cell.xor_mask ^ colour) & 0x0f);
    }
}

}  // namespace chr::game
