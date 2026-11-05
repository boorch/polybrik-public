// fbinfo: print FBIOGET_VSCREENINFO / FBIOGET_FSCREENINFO of /dev/fb0
#include <fcntl.h>
#include <linux/fb.h>
#include <stdio.h>
#include <sys/ioctl.h>
int main(void) {
  int fd = open("/dev/fb0", O_RDONLY);
  struct fb_var_screeninfo v; struct fb_fix_screeninfo f;
  if (fd < 0 || ioctl(fd, FBIOGET_VSCREENINFO, &v) || ioctl(fd, FBIOGET_FSCREENINFO, &f)) { perror("fb0"); return 1; }
  printf("id=%s smem_len=%u line_length=%u type=%u visual=%u\n", f.id, f.smem_len, f.line_length, f.type, f.visual);
  printf("xres=%u yres=%u xres_virtual=%u yres_virtual=%u xoffset=%u yoffset=%u bpp=%u\n",
         v.xres, v.yres, v.xres_virtual, v.yres_virtual, v.xoffset, v.yoffset, v.bits_per_pixel);
  printf("red=%u/%u green=%u/%u blue=%u/%u transp=%u/%u (offset/length)\n", v.red.offset, v.red.length,
         v.green.offset, v.green.length, v.blue.offset, v.blue.length, v.transp.offset, v.transp.length);
  printf("pixclock=%u margins l%u r%u u%u d%u hsync=%u vsync=%u\n", v.pixclock, v.left_margin, v.right_margin,
         v.upper_margin, v.lower_margin, v.hsync_len, v.vsync_len);
  return 0;
}
