# Crescent Hawk's Rebuild

A ground-up rebuild of **BattleTech: The Crescent Hawk's Inception** (Infocom,
1988, written by Westwood Associates) in modern C++, so the game runs on
today's Windows, macOS and Linux.

This is **not** an emulator, a static recompilation or a patched copy of the
original. Every line is new. We study the original DOS program to learn
exactly what it does, then write our own code that does the same thing. The
aim is for the rebuild to play as the original did, and for its source and
notes to document how the original worked, for preservation and research.

**Status:** just started. Nothing is playable yet.

## You need your own copy of the game

The rebuild contains none of the original's files. It reads the graphics,
maps, animations and text from your own copy of the DOS version, so you need
a copy of the game to play it. The reference version is DOS 1.03.

## The rules

This project is unofficial and non-commercial. It distributes none of the
original's files and none of its code. It is not affiliated with or endorsed
by the holders of any rights in Infocom, Westwood or BattleTech. The full
rules are in [docs/rules.md](docs/rules.md).

## Layout

- `docs/` — how the original works, in our own words, and the project rules.
- `tools/` — the tools we use to study the original. They work on your own
  copy, and they never publish it.
