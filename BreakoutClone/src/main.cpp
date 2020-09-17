#include "breakout.h"

#include "common.h"

#include <stdexcept>

// For some reason, someone thought that line
// #define main SDL_main
// was a good idea
#undef main

int main(int, char*[]) {
    try {
        Breakout breakout;
        breakout.run();
    } catch (std::runtime_error e) {
        printf("%s\n", e.what());
        return -1;
    }

    return 0;
}
