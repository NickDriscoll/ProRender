#include <chrono>

#define TIMER_IMPLEMENTATION  //TODO: Figure out how to remove this

struct Timer {

#ifndef TIMER_IMPLEMENTATION
    void start();
    double check();

#else

    void start() {
        start_time = std::chrono::steady_clock::now();
    }

    double check() {
        const std::chrono::duration<double, std::ratio<1, 1000>> d = std::chrono::steady_clock::now() - start_time;
        return d.count();
    }

#endif

private:
    std::chrono::time_point<std::chrono::steady_clock> start_time;
};