// probe32.c  (compile: arm-none-linux-gnueabihf-gcc -static -mthumb -mfloat-abi=hard -mfpu=fp-armv8 -O1 probe32.c -o probe32)
#include <stdio.h>
#include <sys/utsname.h>
int main(void){ struct utsname u; uname(&u); printf("machine=%s ptr=%zu\n", u.machine, sizeof(void*));
  unsigned a = 0x00017FFF, b = 0x00010001, r; __asm__ volatile("qadd16 %0,%1,%2" : "=r"(r) : "r"(a), "r"(b)); printf("qadd16=%08x (expect 00027fff)\n", r);
  int acc = 0; __asm__ volatile("smlad %0,%1,%2,%3" : "=r"(acc) : "r"(0x00030002), "r"(0x00050004), "r"(acc)); printf("smlad=%d (expect 23)\n", acc);
  volatile float x = 1.5f, y = 2.5f; printf("vfp=%f (expect 3.75)\n", x * y); return 0; }
