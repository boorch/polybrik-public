// sdltest: display mode, renderer, audio obtained spec, joystick, 20 s event log.
#include <SDL.h>
#include <stdio.h>
#include <stdlib.h>
static void silence(void *u, Uint8 *s, int n) { (void)u; SDL_memset(s, 0, n); }
int main(int argc, char **argv) {
  int secs = argc > 1 ? atoi(argv[1]) : 20;
  SDL_version v; SDL_GetVersion(&v); printf("SDL linked %d.%d.%d\n", v.major, v.minor, v.patch);
  if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_JOYSTICK) < 0) { printf("SDL_Init: %s\n", SDL_GetError()); return 1; }
  printf("video=%s audio=%s\n", SDL_GetCurrentVideoDriver(), SDL_GetCurrentAudioDriver());
  SDL_DisplayMode m;
  if (SDL_GetCurrentDisplayMode(0, &m) == 0) printf("display mode %dx%d @%d Hz fmt=%s\n", m.w, m.h, m.refresh_rate, SDL_GetPixelFormatName(m.format));
  else printf("SDL_GetCurrentDisplayMode: %s\n", SDL_GetError());
  SDL_Window *w = SDL_CreateWindow("sdltest", 0, 0, 1024, 768, SDL_WINDOW_FULLSCREEN | SDL_WINDOW_SHOWN);
  if (!w) printf("SDL_CreateWindow: %s\n", SDL_GetError());
  SDL_Renderer *r = w ? SDL_CreateRenderer(w, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC) : NULL;
  SDL_RendererInfo ri;
  if (r && SDL_GetRendererInfo(r, &ri) == 0) { int ww, wh; SDL_GetWindowSize(w, &ww, &wh);
    printf("renderer %s flags=0x%x max_tex=%dx%d window=%dx%d\n", ri.name, ri.flags, ri.max_texture_width, ri.max_texture_height, ww, wh); }
  else if (w) printf("SDL_CreateRenderer: %s\n", SDL_GetError());
  SDL_AudioSpec want = {0}, got = {0};
  want.freq = 44100; want.format = AUDIO_S16; want.channels = 2; want.samples = 512; want.callback = silence;
  SDL_AudioDeviceID ad = SDL_OpenAudioDevice(NULL, 0, &want, &got, SDL_AUDIO_ALLOW_FREQUENCY_CHANGE);
  if (ad) { printf("audio obtained freq=%d samples=%d format=0x%04x channels=%d\n", got.freq, got.samples, got.format, got.channels); SDL_PauseAudioDevice(ad, 0); }
  else printf("SDL_OpenAudioDevice: %s\n", SDL_GetError());
  int nj = SDL_NumJoysticks(); printf("joysticks=%d\n", nj);
  SDL_Joystick *j = nj > 0 ? SDL_JoystickOpen(0) : NULL;
  if (j) { char g[33]; SDL_JoystickGetGUIDString(SDL_JoystickGetGUID(j), g, sizeof g);
    printf("joystick0 \"%s\" guid=%s buttons=%d axes=%d hats=%d\n", SDL_JoystickName(j), g, SDL_JoystickNumButtons(j), SDL_JoystickNumAxes(j), SDL_JoystickNumHats(j)); }
  fflush(stdout);
  Uint32 end = SDL_GetTicks() + secs * 1000u; int frame = 0;
  while (!SDL_TICKS_PASSED(SDL_GetTicks(), end)) {
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
      Uint32 t = e.common.timestamp;
      if (e.type == SDL_JOYBUTTONDOWN || e.type == SDL_JOYBUTTONUP) printf("%u button %d %s\n", t, e.jbutton.button, e.jbutton.state ? "down" : "up");
      else if (e.type == SDL_JOYAXISMOTION) printf("%u axis %d = %d\n", t, e.jaxis.axis, e.jaxis.value);
      else if (e.type == SDL_JOYHATMOTION) printf("%u hat %d = %d\n", t, e.jhat.hat, e.jhat.value);
      else if (e.type == SDL_KEYDOWN) printf("%u keydown %s\n", t, SDL_GetKeyName(e.key.keysym.sym));
      fflush(stdout);
    }
    if (r) { SDL_SetRenderDrawColor(r, 0, (frame++ & 63) * 2, 64, 255); SDL_RenderClear(r); SDL_RenderPresent(r); }
    else SDL_Delay(16);
  }
  printf("frames=%d\n", frame);
  if (ad) SDL_CloseAudioDevice(ad);
  if (j) SDL_JoystickClose(j);
  SDL_Quit();
  FILE *fb = fopen("/dev/fb0", "wb");  // blank the framebuffer on exit
  if (fb) { static char z[4096]; while (fwrite(z, 1, sizeof z, fb) == sizeof z) {} fclose(fb); }
  printf("quit\n");
  return 0;
}
