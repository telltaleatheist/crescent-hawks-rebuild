// The original's pace. It never reads a clock: its waits count vertical
// retraces (273d:0006 waits for one through the VGA/EGA status port 3DA),
// so it runs at the display's refresh rate: about 70 per second on a VGA
// card in its 320x200 mode, 60 on an EGA card. The timer chip is set to
// about 291 Hz (284b:0048) only for the music driver. docs/reference.md.
//
// The game logic counts passes and ticks, never seconds. The platform layer
// turns real time into frames with a FrameClock; scripts and tests give the
// frames directly, so a run does not depend on the machine.
#pragma once

namespace chr::game {

// A VGA card's refresh rate in the 320x200 modes (the rebuild's reference,
// and what DOSBox shows). An EGA card refreshes at about 60.
inline constexpr double kVgaRefreshHz = 70.086;

class FrameClock {
public:
    explicit FrameClock(double hz = kVgaRefreshHz) : hz_(hz) {}
    // Adds `seconds` of real time and returns how many whole frames are now
    // due. A long stall (more than a quarter second) is not caught up.
    int advance(double seconds) {
        if (seconds > 0.25) seconds = 0.25;
        pending_ += seconds * hz_;
        const int frames = int(pending_);
        pending_ -= frames;
        return frames;
    }

private:
    double hz_;
    double pending_ = 0;
};

}  // namespace chr::game
