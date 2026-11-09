#ifndef AUDIO_H
#define AUDIO_H

// false in the headless simulator, so thousands of matches stay silent
extern bool g_audio_enabled;

void play_sound_kick();
void play_sound_goal();
void play_sound_whistle();
void play_sound_gameover();

#endif
