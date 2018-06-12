#ifndef __PREPROCESS_INCLUDE__
#define __PREPROCESS_INCLUDE__

#include <stdio.h>
#include "string.h"
#include "lex.h"
#include "utf8.h"

/* prime number */
#define MACRO_TABLE_SIZE 40961
#define MACRO_TABLE_SLIDE_MOD 1001

#define MACRO_PARAMS_SIZE 128
#define MACRO_PARAMS_LIMIT (MACRO_PARAMS_SIZE - 1)

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
  struct string *parameters[MACRO_PARAMS_SIZE];
  struct pp_list *replacement_list;
  int expanded;
};

struct preprocessor {
  struct pp_token_lexer *lexer;
  struct pp_token *token_queue[1];
  int token_queue_size;
  struct pp_list *list;
};

extern void group(struct preprocessor *pp);
extern void skip_line(struct preprocessor *pp);
extern void skip_group(struct preprocessor *pp);
extern struct pp_list *parse_preprocessing_file(unsigned char *file);
extern struct pp_list *preprocess(unsigned char *file);

#endif
