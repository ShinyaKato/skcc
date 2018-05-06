#ifndef __STRING_INCLUDE__
#define __STRING_INCLUDE__

struct string {
  unsigned char *head;
  int size;
  int alloc_size;
};

extern struct string *allocate_string();
extern void append_string(struct string *str, unsigned char c);
extern void concat_string(struct string *dist, struct string *src);
extern void free_string(struct string *str);

#endif
