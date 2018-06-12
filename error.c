#include "error.h"

void print_diagnose(char *type, char *file, int line, char *format, va_list args) {
  fprintf(stderr, "[%s] %s:%d\n", type, file, line);
  vfprintf(stderr, format, args);
  fprintf(stderr, "\n");
  fprintf(stderr, "\n");
}

void print_error(char *file, int line, char *format, ...) {
  va_list args;
  va_start(args, format);
  print_diagnose("error", file, line, format, args);
  va_end(args);
  exit(1);
}

void print_warning(char *file, int line, char *format, ...) {
  va_list args;
  va_start(args, format);
  print_diagnose("warning", file, line, format, args);
  va_end(args);
}

void print_debug(char *file, int line, char *format, ...) {
  va_list args;
  va_start(args, format);
  print_diagnose("debug", file, line, format, args);
  va_end(args);
}
