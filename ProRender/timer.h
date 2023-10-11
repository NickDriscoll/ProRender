#include <chrono>

#define TIMER_IMPLEMENTATION  //TODO: Figure out how to remove this

struct Timer {

#ifndef TIMER_IMPLEMENTATION
    void start();
    double check();
    void print(const char* title);

    Timer();
    Timer(const char* name);

#else

    void start() {
        start_time = std::chrono::steady_clock::now();
    }

    double check() {
        const std::chrono::duration<double, std::ratio<1, 1000>> d = std::chrono::steady_clock::now() - start_time;
        return d.count();
    }

    void print(const char* title) {
        printf("%s: %s took %.4fms.\n", name, title, check());
    }

    Timer() {
        *this = Timer("");
    }

    Timer(const char* name) {
        this->name = name;
        this->start_time = std::chrono::steady_clock::now();
    }

#endif

private:
    const char* name;
    std::chrono::time_point<std::chrono::steady_clock> start_time;
};