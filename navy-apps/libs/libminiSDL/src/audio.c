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

SDL_AudioSpec *SDL_LoadWAV(const char *file,
                          SDL_AudioSpec *spec,
                          uint8_t **audio_buf,
                          uint32_t *audio_len) {
  assert(file && spec && audio_buf && audio_len);

  FILE *fp = fopen(file, "rb");
  if (!fp) return NULL;

  // 读取 WAV header（最小实现）
  uint8_t header[44];
  fread(header, 1, 44, fp);

  // 检查 "RIFF" 和 "WAVE"
  if (memcmp(header, "RIFF", 4) != 0 ||
      memcmp(header + 8, "WAVE", 4) != 0) {
    fclose(fp);
    return NULL;
  }

  // 解析关键字段
  uint16_t channels = *(uint16_t *)(header + 22);
  uint32_t sample_rate = *(uint32_t *)(header + 24);
  uint16_t bits_per_sample = *(uint16_t *)(header + 34);
  uint32_t data_size = *(uint32_t *)(header + 40);

  // 目前只支持 8/16bit PCM
  assert(bits_per_sample == 8 || bits_per_sample == 16);

  // 分配音频数据
  uint8_t *buf = (uint8_t *)malloc(data_size);
  assert(buf);

  fread(buf, 1, data_size, fp);
  fclose(fp);

  // 填 spec
  spec->freq = sample_rate;
  spec->channels = channels;
  spec->samples = 1024;
  spec->callback = NULL;
  spec->userdata = NULL;

  // SDL 格式（简单处理）
  if (bits_per_sample == 8) {
    spec->format = AUDIO_U8;
  } else {
    spec->format = AUDIO_S16SYS;
  }

  spec->size = data_size;

  *audio_buf = buf;
  *audio_len = data_size;

  return spec;
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