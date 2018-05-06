#ifndef __FILE_INCLUDE__
#define __FILE_INCLUDE__

#include <stdio.h>
#include "error.h"
#include "utf8.h"

struct source {
  FILE *fp;
  const unsigned char *file;
  int row, col;
  int trigraph_queue_size;
  struct utf8c trigraph_queue[3];
  int splice_queue_size;
  struct utf8c splice_queue[3];
};

extern struct source *allocate_source(const unsigned char *file);
extern void free_source(struct source *src);
extern struct utf8c next_source_char(struct source *src);

#endif
