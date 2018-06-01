#include "file.h"

struct source *allocate_source(const unsigned char *file) {
  struct source *src = (struct source *) malloc(sizeof(struct source));
  if(src == NULL) {
    perror("malloc");
    exit(1);
  }

  src->fp = fopen(file, "r");
  if(src->fp == NULL) {
    perror("fopen");
    exit(1);
  }

  src->file = file;
  src->row = 1;
  src->col = 1;
  src->trigraph_queue_size = 0;
  src->splice_queue_size = 0;
}

void free_source(struct source *src) {
  fclose(src->fp);
  free(src);
}

struct utf8c fget_utf8c(struct source *src) {
  struct utf8c uc;
  uc.sequence[0] = fgetc(src->fp);
  uc.bytes = count_bytes(uc.sequence[0]);

  if(uc.bytes == -1) {
    error("source file \"%s\" contains invalid sequence.\n", src->file);
  }

  if(uc.bytes == 0) {
    return ueof;
  }

  for(int i = 1; i < uc.bytes; i++) {
    uc.sequence[i] = fgetc(src->fp);
  }

  if(!check_sequence(uc)) {
    error("source file \"%s\" contains invalid sequence.\n", src->file);
  }

  uc.sequence[uc.bytes] = '\0';

  return uc;
}

struct utf8c replace_trigraph(struct source *src) {
  unsigned char trigraph_symbol[9] = { '=', '(', '/', ')', '\'', '<', '!', '>', '-' };
  unsigned char trigraph_char[9] = { '#', '[', '\\', ']', '^', '{', '|', '}', '~' };

  for(; src->trigraph_queue_size < 3; src->trigraph_queue_size++) {
    src->trigraph_queue[src->trigraph_queue_size] = fget_utf8c(src);
    if(src->trigraph_queue[src->trigraph_queue_size].bytes == 0) {
      break;
    }
  }

  if(src->trigraph_queue_size == 0) {
    return ueof;
  }

  if(src->trigraph_queue_size == 3) {
    if(src->trigraph_queue[0].sequence[0] == '?') {
      if(src->trigraph_queue[1].sequence[0] == '?') {
        for(int i = 0; i < 9; i++) {
          if(src->trigraph_queue[2].sequence[0] == trigraph_symbol[i]) {
            struct utf8c uc = single_byte_char(trigraph_char[i]);
            src->trigraph_queue_size = 0;
            return uc;
          }
        }
      }
    }
  }

  struct utf8c uc = src->trigraph_queue[0];
  src->trigraph_queue_size--;
  for(int i = 0; i < src->trigraph_queue_size; i++) {
    src->trigraph_queue[i] = src->trigraph_queue[i + 1];
  }
  return uc;
}

struct utf8c splice_line(struct source *src) {
  for(; src->splice_queue_size < 3; src->splice_queue_size++) {
    src->splice_queue[src->splice_queue_size] = replace_trigraph(src);
    if(src->splice_queue[src->splice_queue_size].bytes == 0) {
      break;
    }
  }

  if(src->splice_queue_size == 0) {
    return ueof;
  }

  if(src->splice_queue_size >= 2) {
    if(src->splice_queue[0].sequence[0] == '\\') {
      if(src->splice_queue[1].sequence[0] == '\n') {
        if(src->splice_queue_size == 2) {
          error("end of the source file requires new line indicator.\n");
        }
        src->splice_queue_size = 0;
        return src->splice_queue[2];
      }
    }
  }

  struct utf8c uc = src->splice_queue[0];
  src->splice_queue_size--;
  for(int i = 0; i < src->splice_queue_size; i++) {
    src->splice_queue[i] = src->splice_queue[i + 1];
  }
  return uc;
}

struct utf8c next_source_char(struct source *src) {
  return splice_line(src);
}
