// The character record (F:c614 + 0x11 p, docs/formats/characters.md), kept
// as the original's 17 bytes: the fight reads and writes it byte by byte,
// and the reference rig compares it byte for byte. The 'Mech record is
// formats::MechRecord (formats/rules.h).
#pragma once

#include <array>
#include <cstdint>

namespace chr::game {

using CharacterRecord = std::array<std::uint8_t, 0x11>;

namespace character {
inline constexpr int kName = 0x00;        // name number (character.<n>); ff: empty
inline constexpr int kB = 0x01;           // the panel's first number; 10 x it is the hit-point maximum
inline constexpr int kD = 0x02;           // the second: walking MP on foot
inline constexpr int kC = 0x03;           // the third
inline constexpr int kSkills = 0x04;      // seven skills, 0-4: Bow & Blade, Pistol, Rifle, Gunnery, Piloting, Tech, Medical
inline constexpr int kSkillCount = 7;
inline constexpr int kWeapon = 0x0b;      // the personal weapon (weapon table index)
inline constexpr int kMech = 0x0c;        // the 'Mech record it is in, or kOnFoot and above
inline constexpr int kArmourType = 0x0d;
inline constexpr int kArmour = 0x0e;      // armour points left
inline constexpr int kHitPoints = 0x0f;   // 0: killed
inline constexpr int kOnFoot = 8;
inline constexpr std::uint8_t kEmpty = 0xff;
}  // namespace character

// BTECH.EXE 1000:4edf, part of the new-game setup: Jason (name 0) on foot
// with 8, 9, 7 and 80 hit points; records 1-7 empty; the enemy's records
// 8-15 zero (name 0).
inline std::array<CharacterRecord, 16> new_game_characters() {
    std::array<CharacterRecord, 16> c{};
    c[0][character::kB] = 8;
    c[0][character::kD] = 9;
    c[0][character::kC] = 7;
    c[0][character::kMech] = character::kOnFoot;
    c[0][character::kHitPoints] = 80;
    for (int i = 1; i < 8; ++i) c[std::size_t(i)][character::kName] = character::kEmpty;
    return c;
}

}  // namespace chr::game
