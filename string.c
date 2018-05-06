#include <stdlib.h>
#include "string.h"

struct string *allocate_string() {
  const int INIT_SIZE = 64;

  struct string *str = (struct string *) malloc(sizeof(struct string));
  str->head = (unsigned char *) malloc(sizeof(unsigned char) * (INIT_SIZE + 1));
  str->size = 0;
  str->alloc_size = INIT_SIZE;
  str->head[0] = '\0';

  return str;
}

void append_string(struct string *str, unsigned char c) {
  if(str->size >= str->alloc_size) {
    unsigned char *t = (unsigned char *) malloc(sizeof(unsigned char) * (str->alloc_size * 2 + 1));

    for(int i = 0; i < str->size; i++) t[i] = str->head[i];
    free(str->head);

    str->head = t;
    str->alloc_size *= 2;
  }

  str->head[str->size++] = c;
  str->head[str->size] = '\0';
}

void concat_string(struct string *dist, struct string *src) {
  for(int i = 0; i < src->size; i++) {
    append_string(dist, src->head[i]);
  }
}

void free_string(struct string *str) {
  free(str->head);
  free(str);
}
