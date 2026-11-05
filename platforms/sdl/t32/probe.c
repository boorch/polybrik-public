/* T32 dependency probe (safe while NextUI runs: no video, no audio device opened).
 * Static SDL2: joystick enumeration and per-device button/axis/hat counts, to compare with the
 * device's own SDL (16 buttons, 6 axes, 1 hat). Static alsa-lib: loads the
 * device's alsa.conf and resolves pcm.hw without opening a PCM.
 * Build: see platforms/sdl/t32/deps.sh output (prefix/). */
#include <SDL.h>
#include <alsa/asoundlib.h>
#include <stdio.h>

int main(void)
{
    snd_config_t* n = NULL;
    int e = snd_config_update();
    printf("alsa: lib %s, snd_config_update %d (%s), pcm.hw %s\n", snd_asoundlib_version(), e,
           e < 0 ? snd_strerror(e) : "ok", e >= 0 && snd_config_search(snd_config, "pcm.hw", &n) == 0 ? "found" : "MISSING");
    if (SDL_Init(SDL_INIT_JOYSTICK) != 0) { printf("SDL_Init: %s\n", SDL_GetError()); return 1; }
    printf("sdl: %d joysticks\n", SDL_NumJoysticks());
    for (int i = 0; i < SDL_NumJoysticks(); i++)
    {
        SDL_Joystick* j = SDL_JoystickOpen(i);
        char guid[64];
        SDL_JoystickGetGUIDString(SDL_JoystickGetGUID(j), guid, sizeof guid);
        printf("joystick%d \"%s\" guid=%s buttons=%d axes=%d hats=%d\n", i, SDL_JoystickName(j), guid,
               SDL_JoystickNumButtons(j), SDL_JoystickNumAxes(j), SDL_JoystickNumHats(j));
    }
    SDL_Quit();
    return 0;
}
