#ifndef __PREPROCESS_INCLUDE__
#define __PREPROCESS_INCLUDE__

#include <stdio.h>
#include "string.h"
#include "lex.h"
#include "utf8.h"

/* prime number */
#define MACRO_TABLE_SIZE 40961
#define MACRO_TABLE_SLIDE_MOD 1001

#define MACRO_PARAMS_LIMIT 127

struct pp_list {
  struct pp_node *head;
  struct pp_node **tail;
};

struct pp_node {
  struct pp_token *token;
  struct pp_node *next;
  int skip;
};

enum macro_type { MACRO_OBJECT, MACRO_FUNCTION };

struct macro_entry {
  enum macro_type type;
  const unsigned char *identifier;
  int parameter_size;
  int parameter_ellipsis;
  struct string *parameters[MACRO_PARAMS_LIMIT];
  struct pp_list *replacement_list;
  int expanded;
};

struct preprocessor {
  struct pp_token_lexer *lexer;
  struct pp_token *token_queue[1];
  int token_queue_size;
  struct pp_list *list;
  struct macro_entry *macro_table[MACRO_TABLE_SIZE];
};

extern struct preprocessor *allocate_preprocessor(const unsigned char *file);
extern void free_preprocessor(struct preprocessor *pp);
extern struct pp_list *preprocessing_file(struct preprocessor *pp);

#endif
