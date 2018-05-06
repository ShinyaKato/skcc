#ifndef __UTF8_INCLUDE__
#define __UTF8_INCLUDE__

struct utf8c {
  int bytes;
  unsigned char sequence[5];
};

extern struct utf8c eof;

extern int count_bytes(unsigned char c);
extern int check_sequence(struct utf8c uc);
extern struct utf8c single_byte_char(unsigned char c);
extern struct utf8c code_point(int code);

#endif
