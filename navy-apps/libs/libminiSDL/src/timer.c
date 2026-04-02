#include <NDL.h>
#include <sdl-timer.h>
#include <stdio.h>
#include <assert.h>

static void sdl_timer_unsupported(const char *name) {
  fprintf(stderr, "[miniSDL] Unsupported timer API called: %s\n", name);
  assert(0);
}

SDL_TimerID SDL_AddTimer(uint32_t interval, SDL_NewTimerCallback callback, void *param) {
  (void)interval;
  (void)callback;
  (void)param;
  sdl_timer_unsupported("SDL_AddTimer");
  return NULL;
}

int SDL_RemoveTimer(SDL_TimerID id) {
  (void)id;
  sdl_timer_unsupported("SDL_RemoveTimer");
  return 0;
}

void SDL_AudioUpdate(void);

uint32_t SDL_GetTicks() {
  SDL_AudioUpdate();
  return NDL_GetTicks();
}

void SDL_Delay(uint32_t ms) {
  uint32_t start = NDL_GetTicks();
  while (NDL_GetTicks() - start < ms) {
    SDL_AudioUpdate();
  }
}