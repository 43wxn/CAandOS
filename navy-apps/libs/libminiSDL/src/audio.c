#include <NDL.h>
#include <SDL.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static SDL_AudioSpec g_spec;
static int g_audio_opened = 0;
static int g_audio_paused = 1;
static uint32_t g_last_tick = 0;
static uint32_t g_interval_ms = 0;
static uint8_t *g_stream_buf = NULL;
static int g_stream_buf_size = 0;

static int bytes_per_sample(uint16_t format) {
  switch (format) {
    case AUDIO_U8:  return 1;
    case AUDIO_S16: return 2;
    default:
      assert(0);
      return 1;
  }
}

static void CallbackHelper() {
  static int dbg_cnt = 0;

  if (!g_audio_opened) {
    if (dbg_cnt < 5) {
      printf("[audio] callback skip: not opened\n");
      dbg_cnt++;
    }
    return;
  }

  if (g_audio_paused) {
    if (dbg_cnt < 5) {
      printf("[audio] callback skip: paused\n");
      dbg_cnt++;
    }
    return;
  }

  if (g_spec.callback == NULL) {
    if (dbg_cnt < 5) {
      printf("[audio] callback skip: callback is NULL\n");
      dbg_cnt++;
    }
    return;
  }

  uint32_t now = NDL_GetTicks();
  if (now - g_last_tick < g_interval_ms) return;
  g_last_tick = now;

  int bps = bytes_per_sample(g_spec.format);
  int want_len = g_spec.samples * g_spec.channels * bps;
  if (want_len <= 0) return;

  int free_bytes = NDL_QueryAudio();
  if (dbg_cnt < 20) {
    printf("[audio] callback run: free_bytes=%d want_len=%d\n", free_bytes, want_len);
    dbg_cnt++;
  }

  if (free_bytes <= 0) return;

  int len = want_len;
  if (len > free_bytes) len = free_bytes;

  int frame_size = g_spec.channels * bps;
  if (frame_size > 0) {
    len = (len / frame_size) * frame_size;
  }
  if (len <= 0) return;

  if (len > g_stream_buf_size) {
    free(g_stream_buf);
    g_stream_buf = (uint8_t *)malloc(len);
    assert(g_stream_buf != NULL);
    g_stream_buf_size = len;
  }

  memset(g_stream_buf, 0, len);
  g_spec.callback(g_spec.userdata, g_stream_buf, len);

  if (dbg_cnt < 30) {
    printf("[audio] play %d bytes\n", len);
    dbg_cnt++;
  }

  NDL_PlayAudio(g_stream_buf, len);
}
int SDL_OpenAudio(SDL_AudioSpec *desired, SDL_AudioSpec *obtained) {
  assert(desired != NULL);

  g_spec = *desired;
  g_audio_opened = 1;
  g_audio_paused = 0;   // 先强行直接播放，排除 SDL_PauseAudio(0) 没被调用的问题
  g_last_tick = NDL_GetTicks();

  g_interval_ms = desired->samples * 1000 / desired->freq;
  if (g_interval_ms == 0) g_interval_ms = 1;

  printf("[audio] SDL_OpenAudio: freq=%d channels=%d samples=%d format=%d interval=%u ms\n",
      desired->freq, desired->channels, desired->samples, desired->format, g_interval_ms);

  NDL_OpenAudio(desired->freq, desired->channels, desired->samples);

  if (obtained) {
    *obtained = *desired;
  }
  return 0;
}

void SDL_CloseAudio() {
  g_audio_opened = 0;
  g_audio_paused = 1;

  if (g_stream_buf) {
    free(g_stream_buf);
    g_stream_buf = NULL;
    g_stream_buf_size = 0;
  }

  NDL_CloseAudio();
}

void SDL_PauseAudio(int pause_on) {
  g_audio_paused = pause_on;
  if (!pause_on) {
    g_last_tick = NDL_GetTicks();
  }
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

SDL_AudioSpec *SDL_LoadWAV(const char *file,
                          SDL_AudioSpec *spec,
                          uint8_t **audio_buf,
                          uint32_t *audio_len) {
  assert(file && spec && audio_buf && audio_len);

  FILE *fp = fopen(file, "rb");
  if (!fp) return NULL;

  uint8_t header[44];
  if (fread(header, 1, 44, fp) != 44) {
    fclose(fp);
    return NULL;
  }

  if (memcmp(header, "RIFF", 4) != 0 ||
      memcmp(header + 8, "WAVE", 4) != 0) {
    fclose(fp);
    return NULL;
  }

  uint16_t channels = *(uint16_t *)(header + 22);
  uint32_t sample_rate = *(uint32_t *)(header + 24);
  uint16_t bits_per_sample = *(uint16_t *)(header + 34);
  uint32_t data_size = *(uint32_t *)(header + 40);

  assert(bits_per_sample == 8 || bits_per_sample == 16);

  uint8_t *buf = (uint8_t *)malloc(data_size);
  assert(buf != NULL);

  if (fread(buf, 1, data_size, fp) != data_size) {
    free(buf);
    fclose(fp);
    return NULL;
  }
  fclose(fp);

  spec->freq = sample_rate;
  spec->channels = channels;
  spec->samples = 1024;
  spec->callback = NULL;
  spec->userdata = NULL;
  spec->format = (bits_per_sample == 8) ? AUDIO_U8 : AUDIO_S16;
  spec->size = data_size;

  *audio_buf = buf;
  *audio_len = data_size;
  return spec;
}

void SDL_FreeWAV(uint8_t *audio_buf) {
  free(audio_buf);
}

void SDL_LockAudio() {
}

void SDL_UnlockAudio() {
}

void SDL_AudioUpdate() {
  CallbackHelper();
}