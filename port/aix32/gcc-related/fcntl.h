extern "C" {
#define open  Fx_open
#include_next <fcntl.h>
#undef  open
int open(const char *, int, mode_t = 0666);
}
