// Minimal Linux ARM EABI runtime for a newlib (arm-none-eabi) static binary: _start and the
// newlib system hooks as raw syscalls (svc 0, number in r7). Everything newlib needs and this
// file does not provide comes from libnosys (returns ENOSYS).
// Built with -fomit-frame-pointer: in Thumb code r7 is otherwise the frame pointer.

#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>

#undef errno
extern int errno;

enum
{
    NR_exit = 1, NR_read = 3, NR_write = 4, NR_open = 5, NR_close = 6, NR_lseek = 19,
    NR_getpid = 20, NR_kill = 37, NR_brk = 45, NR_sched_setaffinity = 241, NR_exit_group = 248, NR_clock_gettime = 263
};

static long sys3(long n, long a, long b, long c)
{
    register long r7 __asm__("r7") = n;
    register long r0 __asm__("r0") = a;
    register long r1 __asm__("r1") = b;
    register long r2 __asm__("r2") = c;
    __asm__ volatile("svc 0" : "+r"(r0) : "r"(r7), "r"(r1), "r"(r2) : "memory");
    return r0;
}

static int ret(long r)
{
    if (r < 0 && r > -4096)
    {
        errno = (int)-r;
        return -1;
    }
    return (int)r;
}

int _read(int fd, char* buf, int len) { return ret(sys3(NR_read, fd, (long)buf, len)); }
int _write(int fd, const char* buf, int len) { return ret(sys3(NR_write, fd, (long)buf, len)); }
int _close(int fd) { return ret(sys3(NR_close, fd, 0, 0)); }
int _lseek(int fd, int off, int whence) { return ret(sys3(NR_lseek, fd, off, whence)); }
int _getpid(void) { return (int)sys3(NR_getpid, 0, 0, 0); }
int _kill(int pid, int sig) { return ret(sys3(NR_kill, pid, sig, 0)); }
void _exit(int code)
{
    for (;;)
        sys3(NR_exit_group, code, 0, 0);
}

// newlib's O_* bits differ from Linux ARM's.
int _open(const char* path, int flags, int mode)
{
    int lf = flags & 3;
    if (flags & O_CREAT) lf |= 0100;
    if (flags & O_EXCL) lf |= 0200;
    if (flags & O_TRUNC) lf |= 01000;
    if (flags & O_APPEND) lf |= 02000;
    return ret(sys3(NR_open, (long)path, lf, mode));
}

// stdio only asks fstat/isatty to choose buffering: the first three descriptors are terminals.
int _isatty(int fd) { return fd <= 2; }
int _fstat(int fd, struct stat* st)
{
    *st = (struct stat){ 0 };
    st->st_mode = fd <= 2 ? S_IFCHR : S_IFREG;
    return 0;
}

void* _sbrk(ptrdiff_t incr)
{
    static uintptr_t cur;
    if (!cur)
        cur = (uintptr_t)sys3(NR_brk, 0, 0, 0);
    uintptr_t old = cur, want = cur + incr;
    if ((uintptr_t)sys3(NR_brk, want, 0, 0) < want)
    {
        errno = ENOMEM;
        return (void*)-1;
    }
    cur = want;
    return (void*)old;
}

// Kernel 4.9 32-bit clock_gettime takes a 32-bit timespec; newlib's time_t is 64-bit.
static int clock32(int clk, int32_t ts[2]) { return ret(sys3(NR_clock_gettime, clk, (long)ts, 0)); }

double helper_now_ms(void)
{
    int32_t ts[2];
    clock32(1 /* CLOCK_MONOTONIC */, ts);
    return ts[0] * 1e3 + ts[1] / 1e6;
}

int helper_pin_cpu(int cpu)
{
    unsigned long mask = 1ul << cpu;
    return ret(sys3(NR_sched_setaffinity, 0, sizeof mask, (long)&mask));
}

// Thread CPU time: separates the synth's own cost from preemption in the wall-clock numbers.
double helper_cpu_ms(void)
{
    int32_t ts[2];
    clock32(3 /* CLOCK_THREAD_CPUTIME_ID */, ts);
    return ts[0] * 1e3 + ts[1] / 1e6;
}

int _gettimeofday(struct timeval* tv, void* tz)
{
    int32_t ts[2];
    if (clock32(0 /* CLOCK_REALTIME */, ts) < 0)
        return -1;
    tv->tv_sec = ts[0];
    tv->tv_usec = ts[1] / 1000;
    return 0;
}

// -nostartfiles drops crti/crtbegin/crtn; init_array/fini_array come from the default linker
// script. __dso_handle (normally crtbegin) is the atexit key static destructors register with.
void _init(void) {}
void _fini(void) {}
void* __dso_handle = 0;

extern void __libc_init_array(void);
extern void __libc_fini_array(void);
extern int main(int, char**, char**);

__attribute__((noreturn, used)) static void start_c(long* sp)
{
    int argc = (int)sp[0];
    char** argv = (char**)(sp + 1);
    char** envp = argv + argc + 1;
    extern char** environ;
    environ = envp;
    atexit(__libc_fini_array);
    __libc_init_array();
    exit(main(argc, argv, envp));
}

// Kernel entry: sp points at argc. The ELF entry has bit 0 set, so the kernel starts in Thumb.
__attribute__((naked, noreturn)) void _start(void)
{
    __asm__ volatile("mov r0, sp\n"
                     "bic r1, r0, #7\n"
                     "mov sp, r1\n"
                     "b start_c\n");
}
