// The program's menu table (docs/scripts.md, "Menus"). A place's script asks
// for a menu by number; the record says where its first item is and how many
// items it has. The items' words are the script's own text, printed before.
#pragma once

#include <array>

namespace chr::formats {

struct MenuRecord {
    int first_row;  // the first item's row in the current window
    int width;      // the highlight bar's width, in cells
    int items;      // how many items (the script's jump table has one word each)
    int colour;     // +8: kept with the record; not read by the menu itself
    int highlight;  // the highlight bar's colour (the bar is XORed in, docs/scripts.md)
};

// BTECH.EXE 385b:0092, 41 records of 16 bytes: first row, width, items, the
// last item chosen (+6, state, starts at 0 and is not kept here), colour,
// highlight, two unused words.
inline constexpr std::array<MenuRecord, 41> kMenus = {{
    {2, 38, 20, 15, 14},  {0, 25, 8, 15, 14},  {0, 0, 0, 0, 0},     {0, 11, 11, 15, 14},
    {1, 25, 5, 0, 14},    {1, 10, 1, 15, 14},  {2, 25, 2, 15, 14},  {1, 25, 3, 15, 14},
    {7, 25, 5, 15, 14},   {4, 25, 4, 15, 14},  {19, 25, 3, 15, 14}, {16, 25, 3, 15, 14},
    {10, 25, 4, 15, 14},  {9, 25, 6, 15, 14},  {9, 25, 5, 15, 14},  {5, 25, 4, 15, 14},
    {11, 25, 7, 15, 14},  {10, 25, 4, 15, 14}, {9, 25, 3, 15, 14},  {6, 25, 3, 15, 14},
    {1, 25, 3, 15, 14},   {5, 25, 6, 15, 14},  {6, 25, 2, 15, 14},  {1, 25, 7, 15, 14},
    {8, 25, 2, 15, 14},   {14, 25, 5, 15, 14}, {11, 25, 8, 15, 14}, {14, 25, 6, 15, 14},
    {14, 25, 7, 15, 14},  {14, 25, 2, 15, 14}, {10, 25, 3, 15, 14}, {8, 25, 2, 15, 14},
    {14, 25, 3, 15, 14},  {0, 25, 6, 15, 14},  {0, 25, 6, 15, 14},  {7, 25, 2, 15, 14},
    {13, 25, 2, 15, 14},  {4, 16, 20, 15, 14}, {1, 25, 3, 15, 14},  {0, 25, 8, 15, 14},
    {1, 11, 7, 15, 14},
}};

}  // namespace chr::formats
