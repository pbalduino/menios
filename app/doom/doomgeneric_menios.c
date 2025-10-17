#include <stdint.h>
#include <time.h>

#include "doomgeneric.h"

void DG_Init(void) {
  // TODO: Wire DG_Init to the meniOS framebuffer subsystem when it is available.
}

void DG_DrawFrame(void) {
  // TODO: Push DG_ScreenBuffer to the meniOS display driver once it exists.
}

void DG_SleepMs(uint32_t ms) {
  struct timespec req = {
      .tv_sec = (time_t)(ms / 1000U),
      .tv_nsec = (long)((ms % 1000U) * 1000000UL),
  };

  nanosleep(&req, NULL);
}

uint32_t DG_GetTicksMs(void) {
  struct timespec ts;
  if(clock_gettime(CLOCK_MONOTONIC, &ts) != 0) {
    return 0;
  }

  uint64_t total_ms = (uint64_t)ts.tv_sec * 1000ULL + (uint64_t)ts.tv_nsec / 1000000ULL;
  return (uint32_t)(total_ms & UINT32_MAX);
}

int DG_GetKey(int* pressed, unsigned char* key) {
  (void)pressed;
  (void)key;
  // TODO: Integrate DG_GetKey with the meniOS input subsystem when keyboard events are exposed.
  return 0;
}

void DG_SetWindowTitle(const char* title) {
  (void)title;
  // TODO: Propagate the window title to the meniOS window manager once it is implemented.
}

int main(int argc, char** argv) {
  doomgeneric_Create(argc, argv);

  for(;;) {
    doomgeneric_Tick();
  }

  return 0;
}
