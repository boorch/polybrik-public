// evdump: print every input_event from /dev/input/event* with code names.
// usage: evdump [seconds=30] [device...]  (default: all /dev/input/event0..9)
#include <fcntl.h>
#include <linux/input.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

static volatile sig_atomic_t stop;
static void on_sig(int s) { (void)s; stop = 1; }

static const char *key_name(int c) {
  switch (c) {
  case KEY_POWER: return "KEY_POWER"; case KEY_VOLUMEDOWN: return "KEY_VOLUMEDOWN";
  case KEY_VOLUMEUP: return "KEY_VOLUMEUP"; case KEY_F1: return "KEY_F1"; case KEY_F2: return "KEY_F2";
  case KEY_HOMEPAGE: return "KEY_HOMEPAGE"; case KEY_MENU: return "KEY_MENU";
  case BTN_SOUTH: return "BTN_SOUTH/A"; case BTN_EAST: return "BTN_EAST/B"; case BTN_C: return "BTN_C";
  case BTN_NORTH: return "BTN_NORTH/X"; case BTN_WEST: return "BTN_WEST/Y"; case BTN_Z: return "BTN_Z";
  case BTN_TL: return "BTN_TL"; case BTN_TR: return "BTN_TR"; case BTN_TL2: return "BTN_TL2"; case BTN_TR2: return "BTN_TR2";
  case BTN_SELECT: return "BTN_SELECT"; case BTN_START: return "BTN_START"; case BTN_MODE: return "BTN_MODE";
  case BTN_THUMBL: return "BTN_THUMBL"; case BTN_THUMBR: return "BTN_THUMBR";
  case BTN_DPAD_UP: return "BTN_DPAD_UP"; case BTN_DPAD_DOWN: return "BTN_DPAD_DOWN";
  case BTN_DPAD_LEFT: return "BTN_DPAD_LEFT"; case BTN_DPAD_RIGHT: return "BTN_DPAD_RIGHT";
  case KEY_UP: return "KEY_UP"; case KEY_DOWN: return "KEY_DOWN"; case KEY_LEFT: return "KEY_LEFT"; case KEY_RIGHT: return "KEY_RIGHT";
  default: return "?";
  }
}
static const char *abs_name(int c) {
  static const char *n[] = {"ABS_X","ABS_Y","ABS_Z","ABS_RX","ABS_RY","ABS_RZ"};
  if (c >= 0 && c < 6) return n[c];
  if (c == ABS_HAT0X) return "ABS_HAT0X"; if (c == ABS_HAT0Y) return "ABS_HAT0Y";
  return "?";
}

int main(int argc, char **argv) {
  int secs = argc > 1 ? atoi(argv[1]) : 30;
  struct pollfd fds[16]; const char *names[16]; int n = 0;
  char buf[32];
  if (argc > 2) {
    for (int i = 2; i < argc && n < 16; i++) if ((fds[n].fd = open(argv[i], O_RDONLY)) >= 0) names[n++] = argv[i];
  } else {
    static char paths[10][32];
    for (int i = 0; i < 10; i++) {
      snprintf(paths[i], sizeof paths[i], "/dev/input/event%d", i);
      if ((fds[n].fd = open(paths[i], O_RDONLY)) >= 0) names[n++] = paths[i];
    }
  }
  for (int i = 0; i < n; i++) {
    fds[i].events = POLLIN;
    if (ioctl(fds[i].fd, EVIOCGNAME(sizeof buf), buf) < 0) buf[0] = 0;
    printf("open %s \"%s\"\n", names[i], buf);
  }
  if (!n) { fprintf(stderr, "no input devices\n"); return 1; }
  signal(SIGINT, on_sig); signal(SIGTERM, on_sig);
  time_t end = time(NULL) + secs;
  printf("logging for %d s\n", secs); fflush(stdout);
  while (!stop && time(NULL) < end) {
    if (poll(fds, n, 200) <= 0) continue;
    for (int i = 0; i < n; i++) {
      if (!(fds[i].revents & POLLIN)) continue;
      struct input_event ev;
      if (read(fds[i].fd, &ev, sizeof ev) != sizeof ev) continue;
      if (ev.type == EV_SYN) continue;
      const char *t = ev.type == EV_KEY ? "EV_KEY" : ev.type == EV_ABS ? "EV_ABS" : ev.type == EV_SW ? "EV_SW" : ev.type == EV_MSC ? "EV_MSC" : "EV_?";
      const char *c = ev.type == EV_KEY ? key_name(ev.code) : ev.type == EV_ABS ? abs_name(ev.code) : "";
      printf("%ld.%06ld %s %s code=%d(0x%03x) %s value=%d\n", (long)ev.time.tv_sec, (long)ev.time.tv_usec,
             names[i], t, ev.code, ev.code, c, ev.value);
      fflush(stdout);
    }
  }
  printf("done\n");
  return 0;
}
