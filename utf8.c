#include "utf8.h"

struct utf8c eof = { 0, { 0xFF } };

int count_bytes(unsigned char c) {
  int bytes;

  if(0x00 <= c && c <= 0x7F) { // 1-byte character
    bytes = 1;
  } else if(0x80 <= c && c <= 0xC1) { // invalid sequence (not first byte)
    bytes = -1;
  } else if(0xC2 <= c && c <= 0xDF) { // 2-bytes character
    bytes = 2;
  } else if(0xE0 <= c && c <= 0xEF) { // 3-bytes character
    bytes = 3;
  } else if(0xF0 <= c && c <= 0xF7) { // 4-bytes character
    bytes = 4;
  } else if(0xF8 <= c && c <= 0xFB) { // 5-bytes character
    bytes = -1;
  } else if(0xFC <= c && c <= 0xFD) { // 6-bytes character
    bytes = -1;
  } else if(c == 0xFE) { // invalid sequence
    bytes = -1;
  } else if(c == 0xFF) { // EOF
    bytes = 0;
  }

  return bytes;
}

int check_sequence(struct utf8c uc) {
  if(0x00 <= uc.sequence[0] && uc.sequence[0] <= 0x7F) { // 1-byte character
    if(uc.bytes != 1) return 0;
  } else if(0x80 <= uc.sequence[0] && uc.sequence[0] <= 0xC1) { // invalid sequence (not first byte)
    return 0;
  } else if(0xC2 <= uc.sequence[0] && uc.sequence[0] <= 0xDF) { // 2-bytes character
    if(uc.bytes != 2) return 0;
    if(!(0x80 <= uc.sequence[1] && uc.sequence[1] <= 0xC1)) return 0;
    if(uc.sequence[0] & 0x1E) return 0;
  } else if(0xE0 <= uc.sequence[0] && uc.sequence[0] <= 0xEF) { // 3-bytes character
    if(uc.bytes != 3) return 0;
    if(!(0x80 <= uc.sequence[1] && uc.sequence[1] <= 0xC1)) return 0;
    if(!(0x80 <= uc.sequence[2] && uc.sequence[2] <= 0xC1)) return 0;
    if((uc.sequence[0] & 0x0F) && (uc.sequence[1] & 0x20)) return 0;
  } else if(0xF0 <= uc.sequence[0] && uc.sequence[0] <= 0xF7) { // 4-bytes character
    if(uc.bytes != 4) return 0;
    if(!(0x80 <= uc.sequence[1] && uc.sequence[1] <= 0xC1)) return 0;
    if(!(0x80 <= uc.sequence[2] && uc.sequence[2] <= 0xC1)) return 0;
    if(!(0x80 <= uc.sequence[3] && uc.sequence[3] <= 0xC1)) return 0;
    if((uc.sequence[0] & 0x08) && (uc.sequence[1] & 0x30)) return 0;
  } else if(0xF8 <= uc.sequence[0] && uc.sequence[0] <= 0xFB) { // 5-bytes character
    return 0;
  } else if(0xFC <= uc.sequence[0] && uc.sequence[0] <= 0xFD) { // 6-bytes character
    return 0;
  } else if(uc.sequence[0] == 0xFE) { // invalid sequence
    return 0;
  } else if(uc.sequence[0] == 0xFF) { // EOF
    if(uc.bytes != 0) return 0;
    if(uc.sequence[0] != 0xFF) return 0;
  }
  return 1;
}

struct utf8c single_byte_char(unsigned char c) {
  struct utf8c uc;
  uc.bytes = 1;
  uc.sequence[0] = c;
  uc.sequence[1] = '\0';
  return uc;
}

struct utf8c code_point(int code) {
  int bit = 0;
  for(int i = code; i > 0; i /= 2) bit++;

  struct utf8c uc;
  if(0 <= bit && bit <= 7) {
    uc.bytes = 1;
    uc.sequence[0] = code;
  } else if(8 <= bit && bit <= 11) {
    uc.bytes = 2;
    uc.sequence[0] = ((code >> 6) & 0x1F) | 0xC0;
    uc.sequence[1] = (code & 0x3F) | 0x80;
  } else if(12 <= bit && bit <= 16) {
    uc.bytes = 3;
    uc.sequence[0] = ((code >> 12) & 0xF) | 0xE0;
    uc.sequence[1] = ((code >> 6) & 0x3F) | 0x80;
    uc.sequence[2] = (code & 0x3F) | 0x80;
  } else if(17 <= bit && bit <= 21) {
    uc.bytes = 4;
    uc.sequence[0] = ((code >> 18) & 0x7) | 0xF0;
    uc.sequence[1] = ((code >> 12) & 0x3F) | 0x80;
    uc.sequence[2] = ((code >> 6) & 0x3F) | 0x80;
    uc.sequence[3] = (code & 0x3F) | 0x80;
  } else {
    uc = eof;
  }
  return uc;
}
