#include "breakout.h"

#include "common.h"
#include "windows.h"

// For some reason, someone thought that line
// #define main SDL_main
// was a good idea
#undef main

int main(int, char*[]) {
    try {
        Breakout breakout;
        breakout.run();
    } catch (std::runtime_error e) {
        MessageBoxA(NULL, e.what(), NULL, MB_ICONERROR | MB_OK);
        return -1;
    }

    return 0;
}
