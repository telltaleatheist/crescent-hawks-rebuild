# The start-up

From running BTECH to the town, in a new game. Rebuilt in `src/game/startup.cpp`, and compared
with the original by the rig's `start_*` cases (docs/reference.md): all match pixel for pixel.

## Order

1. **Hardware questions, text mode.** The graphics adapter, the disk drives, then a note about
   saved games. These choose the original's drivers and disks. The rebuild has neither, so they
   are not rebuilt.
2. **The Infocom/Westwood screen** (in `1527:0044`, at 1527:0131..0216): INFOCOM.CMP over the whole
   screen. The keyboard is emptied, then the screen stays up for at most 700 display frames,
   one `273d:0006` wait each, or until a key (`273d:002f`, which only looks).
3. **The title** (`1000:46a7`): BTTITLE.CMP with its palette.
4. **`1000:50c8`**, the rest of the start-up:
   - 256 random numbers fill a table at P:09fb (counted in the town's random state,
     docs/townspeople.md);
   - `1000:320b` loads ANIMATE.ICN and BTTLTECH.ICN for later (nothing shows);
   - `1000:476d` plays the title's tune, WWOODBT.SIF, until a key or the tune's end.
   - **A key:** the keyboard is emptied, `1000:4dc7` starts a new game (the party, map 1, the
     panel), and window 6 gets its frame and the notice: text IDs `intro.notice.0`–`3`, then
     `intro.first_time` and the Yes/No question (`1000:1a13`, Yes first).
     - Yes: the town (`1000:0000(0)`, the main loop, docs/town_loop.md).
     - No: `intro.load_old_game` and Yes/No again. Yes loads a saved game (`1000:32b3`), No
       goes to the town as after Yes.
   - **No key by the tune's end:** the demo. The keyboard is replaced by DEMOFILE's recorded
     keys, the random numbers are set to 25 13 90, and the main loop runs with them
     (`1000:0000(1)`). Afterwards the title is shown again and the loop repeats.

## In the rebuild

`Town::start_up()` begins a Town with the start-up. `chr` does it unless `--map` is given.
The full-screen pictures come through the asset provider (`assets::kInfocomLogo`,
`assets::kTitleScreen`), and the texts through their imported IDs.

Not rebuilt yet, and not reachable without them:
- the title's tune (sound is not rebuilt), so the title waits for a key however long;
- so the demo never plays;
- loading a saved game: Yes there logs it and a new game starts.
