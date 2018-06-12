#ifndef __ERROR__
#define __ERROR__

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

#define error(...) print_error(__FILE__, __LINE__, __VA_ARGS__)
#define warning(...) print_warning(__FILE__, __LINE__, __VA_ARGS__)
#define debug(...) print_debug(__FILE__, __LINE__, __VA_ARGS__)

extern void print_error(char *file, int line, char *format, ...);
extern void print_warning(char *file, int line, char *format, ...);
extern void print_debug(char *file, int line, char *format, ...);

#endif
