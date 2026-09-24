/// remote control pipe for the client commands are queued and drained on the game thread since it is not safe

#pragma once

namespace pilot {

struct Config {
    bool enabled = false;
    bool d3d = true;
};

void configure(const Config& cfg);

// spawns the pipe thread and subclasses the game window once it appears
bool install();

void shutdown();

}  // namespace pilot
