#include <NDL.h>
#include <SDL.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

static void sdl_audio_unsupported(const char *name) {
  fprintf(stderr, "[miniSDL] Unsupported audio API called: %s\n", name);
  assert(0);
}

int SDL_OpenAudio(SDL_AudioSpec *desired, SDL_AudioSpec *obtained) {
  if (desired) {
    NDL_OpenAudio(desired->freq, desired->channels, desired->samples);
    if (obtained) {
      *obtained = *desired;
    }
    return 0;
  }
  if (obtained) {
    obtained->freq = 0;
    obtained->format = 0;
    obtained->channels = 0;
    obtained->samples = 0;
    obtained->size = 0;
    obtained->callback = NULL;
    obtained->userdata = NULL;
  }
  return 0;
}

void SDL_CloseAudio() {
  NDL_CloseAudio();
}

void SDL_PauseAudio(int pause_on) {
  (void)pause_on;
  // 当前 NDL 音频接口本身就是空实现，这里不额外做事
}

void SDL_MixAudio(uint8_t *dst, uint8_t *src, uint32_t len, int volume) {
  assert(dst && src);
  if (volume <= 0) return;
  if (volume > SDL_MIX_MAXVOLUME) volume = SDL_MIX_MAXVOLUME;

  for (uint32_t i = 0; i < len; i++) {
    int sample = dst[i] + ((int)src[i] * volume) / SDL_MIX_MAXVOLUME;
    if (sample > 255) sample = 255;
    dst[i] = (uint8_t)sample;
  }
}

SDL_AudioSpec *SDL_LoadWAV(const char *file, SDL_AudioSpec *spec, uint8_t **audio_buf, uint32_t *audio_len) {
  (void)file;
  (void)spec;
  (void)audio_buf;
  (void)audio_len;
  sdl_audio_unsupported("SDL_LoadWAV");
  return NULL;
}

void SDL_FreeWAV(uint8_t *audio_buf) {
  free(audio_buf);
}

void SDL_LockAudio() {
  // 单线程环境下不需要
}

void SDL_UnlockAudio() {
  // 单线程环境下不需要
}