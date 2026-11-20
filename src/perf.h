#pragma once

// The game's own load, for the debug overlay (debug mode, bottom left).
namespace Perf {
// Processor time used by the game since the last call, as a percent of one core (0 the first
// time). Call it now and then -- every half second -- not every frame.
float cpuPercent();
// Memory the game holds, in megabytes: on macOS as Activity Monitor counts it (its footprint), on
// Linux its resident set; 0 where neither is known.
float memoryMB();
}
