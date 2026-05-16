#include "audio.h"
#include <thread>
#include <cstdlib>
#include <string>

#include <chrono>

static void play_sound_async(const std::string& command) {
    std::thread([command]() {
        std::system((command + " > /dev/null 2>&1").c_str());
    }).detach();
}

static auto last_pop_time = std::chrono::steady_clock::now();

void play_sound_kick() {
    auto now = std::chrono::steady_clock::now();
    if (std::chrono::duration_cast<std::chrono::milliseconds>(now - last_pop_time).count() > 100) {
        last_pop_time = now;
        play_sound_async("afplay /System/Library/Sounds/Pop.aiff");
    }
}

void play_sound_goal() {
    play_sound_async("afplay /System/Library/Sounds/Glass.aiff");
}

void play_sound_whistle() {
    play_sound_async("afplay /System/Library/Sounds/Basso.aiff");
}

void play_sound_gameover() {
    play_sound_async("afplay /System/Library/Sounds/Hero.aiff");
}
