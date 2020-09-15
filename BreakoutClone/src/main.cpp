#include "breakout.h"

#include "common.h"

#include <stdexcept>

// For some reason, someone thought that line
// #define main SDL_main
// was a good idea
#undef main

int main(int, char*[]) {
    Breakout breakout;
    try {
        breakout.run();
    } catch (std::runtime_error e) {
        printf("%s\n", e.what());
        return -1;
    }

    return 0;
}
