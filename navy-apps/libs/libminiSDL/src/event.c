#include <NDL.h>
#include <SDL.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>

#define keyname(k) #k,

static const char *keyname[] = {
  "NONE",
  _KEYS(keyname)
};

static uint8_t key_state[sizeof(keyname) / sizeof(keyname[0])] = {0};

static int lookup_keycode(const char *name) {
  int n = sizeof(keyname) / sizeof(keyname[0]);
  for (int i = 0; i < n; i++) {
    if (strcmp(name, keyname[i]) == 0) {
      return i;
    }
  }
  return 0;
}

static void sdl_event_unsupported(const char *name) {
  fprintf(stderr, "[miniSDL] Unsupported event API called: %s\n", name);
  assert(0);
}

int SDL_PushEvent(SDL_Event *ev) {
  (void)ev;
  sdl_event_unsupported("SDL_PushEvent");
  return -1;
}

int SDL_PollEvent(SDL_Event *ev) {
  char buf[64];
  int n = NDL_PollEvent(buf, sizeof(buf) - 1);
  if (n <= 0) return 0;

  buf[n] = '\0';

  char type[8] = {0};
  char name[32] = {0};
  if (sscanf(buf, "%7s %31s", type, name) != 2) {
    return 0;
  }

  int code = lookup_keycode(name);
  if (code < 0 || code >= (int)(sizeof(key_state) / sizeof(key_state[0]))) {
    code = 0;
  }

  int is_keydown = (strcmp(type, "kd") == 0);
  key_state[code] = is_keydown ? 1 : 0;

  if (ev) {
    memset(ev, 0, sizeof(*ev));
    ev->type = is_keydown ? SDL_KEYDOWN : SDL_KEYUP;
    ev->key.type = ev->type;
    ev->key.keysym.sym = code;
  }

  return 1;
}

int SDL_WaitEvent(SDL_Event *event) {
  while (1) {
    if (SDL_PollEvent(event)) return 1;
  }
}

int SDL_PeepEvents(SDL_Event *ev, int numevents, int action, uint32_t mask) {
  (void)ev;
  (void)numevents;
  (void)action;
  (void)mask;
  sdl_event_unsupported("SDL_PeepEvents");
  return -1;
}

uint8_t* SDL_GetKeyState(int *numkeys) {
  if (numkeys) {
    *numkeys = sizeof(key_state) / sizeof(key_state[0]);
  }
  return key_state;
}