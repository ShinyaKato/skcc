#include "preprocess.h"

struct macro_entry *macro_table[MACRO_TABLE_SIZE];

struct pp_list *object_macro_invocation(struct preprocessor *pp, struct macro_entry *macro);
struct pp_list *function_macro_invocation(struct preprocessor *pp, struct macro_entry *macro, struct pp_list **args, int args_size);
int conditional_expression(struct pp_node **node);
void group(struct preprocessor *pp);
void skip_line(struct preprocessor *pp);
void skip_group(struct preprocessor *pp);
struct pp_list *parse_preprocessing_file(unsigned char *file);

// pp_list
struct pp_list *allocate_pp_list() {
  struct pp_list *list = (struct pp_list *) malloc(sizeof(struct pp_list));
  if(list == NULL) {
    perror("malloc");
    exit(1);
  }
  list->head = NULL;
  list->tail = &(list->head);
  return list;
}

void free_pp_list(struct pp_list *list) {
  struct pp_node *node = list->head;
  while(node != NULL) {
    struct pp_node *next = node->next;
    free(node);
    node = next;
  }
  free(list);
}

void append_pp_list(struct pp_list *list, struct pp_token *token) {
  struct pp_node *node = (struct pp_node *) malloc(sizeof(struct pp_node));
  node->token = token;
  node->next = NULL;
  node->skip = 0;
  *(list->tail) = node;
  list->tail = &(node->next);
}

void concat_pp_list(struct pp_list *list1, struct pp_list *list2) {
  if(list2->head != NULL) {
    *(list1->tail) = list2->head;
    list1->tail = list2->tail;
  }
}

// macro_entry
struct macro_entry *allocate_macro_entry() {
  struct macro_entry *macro = (struct macro_entry *) malloc(sizeof(struct macro_entry));
  if(macro == NULL) {
    perror("malloc");
    exit(1);
  }
  macro->replacement_list = allocate_pp_list();
  macro->expanded = 0;
  return macro;
}

void free_macro_entry(struct macro_entry *macro) {
  free_pp_list(macro->replacement_list);
  free(macro);
}

int ident_hash(const unsigned char *ident) {
  const int BASE = 257;
  int h = 0;
  for(int i = 0; ident[i] != '\0'; i++) {
    h = (h * BASE + ident[i]) % MACRO_TABLE_SIZE;
  }
  return h;
}

int compare_macro(const struct macro_entry *macro1, const struct macro_entry *macro2) {
  if(macro1->type != macro2->type) {
    return 0;
  }

  if(macro1->type == MACRO_FUNCTION) {
    if(macro1->parameter_ellipsis != macro2->parameter_ellipsis) return 0;
    if(macro1->parameter_size != macro2->parameter_size) return 0;
    for(int i = 0; i < macro1->parameter_size; i++) {
      if(strcmp(macro1->parameters[i]->head, macro2->parameters[i]->head) != 0) return 0;
    }
  }

  struct pp_node *node1 = macro1->replacement_list->head;
  struct pp_node *node2 = macro2->replacement_list->head;
  while(1) {
    if(node1 == NULL || node2 == NULL) {
      if(node1 == NULL && node2 == NULL) break;
      return 0;
    }
    if(node1->token->type != node2->token->type) return 0;
    if(strcmp(node1->token->text->head, node2->token->text->head) != 0) return 0;
    node1 = node1->next;
    node2 = node2->next;
  }

  return 1;
}

void insert_macro_table(struct macro_entry *macro) {
  int h1 = ident_hash(macro->identifier);
  for(int i = 0, h = h1; i < MACRO_TABLE_SIZE; i++, h = (h + 1) % MACRO_TABLE_SIZE) {
    if(macro_table[h] == NULL) {
      macro_table[h] = macro;
      break;
    } else {
      if(strcmp(macro_table[h]->identifier, macro->identifier) == 0) {
        if(compare_macro(macro_table[h], macro)) {
          break;
        } else {
          error("duplicated macro definition: %s\n", macro->identifier);
        }
      }
    }
  }
}

void delete_macro_table(const unsigned char *identifier) {
  int h1 = ident_hash(identifier);
  for(int i = 0, h = h1; i < MACRO_TABLE_SIZE; i++, h = (h + 1) % MACRO_TABLE_SIZE) {
    if(macro_table[h] == NULL) break;
    if(strcmp(macro_table[h]->identifier, identifier) == 0) {
      macro_table[h] = NULL;
      h = (h + 1) % MACRO_TABLE_SIZE;
      for(int j = i; j < MACRO_TABLE_SIZE; j++, h = (h + 1) % MACRO_TABLE_SIZE) {
        if(macro_table[h] == NULL) break;
        struct macro_entry *t = macro_table[h];
        macro_table[h] = NULL;
        insert_macro_table(t);
      }
      break;
    }
  }
}

struct macro_entry *search_macro_table(const unsigned char *identifier) {
  int h1 = ident_hash(identifier);
  for(int i = 0, h = h1; i < MACRO_TABLE_SIZE; i++, h = (h + 1) % MACRO_TABLE_SIZE) {
    if(macro_table[h] == NULL) break;
    if(strcmp(macro_table[h]->identifier, identifier) == 0) {
      return macro_table[h];
    }
  }
  return NULL;
}

// macro replacement
int check_object_macro_invocation(struct preprocessor *pp, struct pp_node *node) {
  if(node->token->type != PP_IDENT) return 0;
  if(node->skip) return 0;

  struct macro_entry *macro = search_macro_table(node->token->text->head);
  return macro != NULL && !macro->expanded && macro->type == MACRO_OBJECT;
}

int check_function_macro_invocation(struct preprocessor *pp, struct pp_node *node) {
  if(node->token->type != PP_IDENT) return 0;
  if(node->skip) return 0;

  struct macro_entry *macro = search_macro_table(node->token->text->head);
  if(macro == NULL || macro->expanded || macro->type != MACRO_FUNCTION) {
    return 0;
  }

  struct pp_node *current = node->next;
  if(current == NULL) {
    return 0;
  }
  if(current->token->type == PP_NEW_LINE || current->token->type == PP_SPACE) {
    current = current->next;
    if(current == NULL) return 0;
  }
  if(current->token->type != PP_LPAREN) return 0;

  return 1;
}

struct pp_list *scan_macro(struct preprocessor *pp, struct pp_list *list) {
  struct pp_list *result = allocate_pp_list();

  for(struct pp_node *node = list->head; node != NULL; node = node->next) {
    struct macro_entry *macro = search_macro_table(node->token->text->head);

    // object-like macro invocation
    if(check_object_macro_invocation(pp, node)) {
      struct macro_entry *macro = search_macro_table(node->token->text->head);
      struct pp_list *list = object_macro_invocation(pp, macro);
      concat_pp_list(result, list);
    }

    // function-like macro invocation
    else if(check_function_macro_invocation(pp, node)) {
      struct macro_entry *macro = search_macro_table(node->token->text->head);
      struct pp_list *args[MACRO_PARAMS_LIMIT];
      int args_count = 0;
      int level = 0;
      int valid = 0;

      node = node->next;
      if(node->token->type == PP_NEW_LINE || node->token->type == PP_SPACE) {
        node = node->next;
      }
      node = node->next;

      if(node != NULL && (node->token->type == PP_NEW_LINE || node->token->type == PP_SPACE)) {
        node = node->next;
      }

      // parse arguments
      if(node != NULL && node->token->type == PP_RPAREN) {
        valid = 1;
      } else if(node != NULL) {
        args[args_count] = allocate_pp_list();
        if(macro->parameter_size > 0) {
          for(; node != NULL; node = node->next) {
            struct pp_token *token = node->token;
            if(token->type == PP_LPAREN) {
              append_pp_list(args[args_count], token);
              level++;
            } else if(token->type == PP_RPAREN) {
              if(level == 0) {
                args_count++;
                valid = 1;
                break;
              }
              append_pp_list(args[args_count], token);
              level--;
            } else if(level == 0 && token->type == PP_COMMA) {
              args_count++;
              args[args_count] = allocate_pp_list();
              if(args_count == macro->parameter_size) {
                if(macro->parameter_ellipsis && !valid) {
                  node = node->next;
                  break;
                } else {
                  error("too many arguments.\n");
                }
                break;
              }
            } else {
              append_pp_list(args[args_count], node->token);
            }
          }
        }
        if(macro->parameter_ellipsis && !valid) {
          for(; node != NULL; node = node->next) {
            if(node->token->type == PP_RPAREN) {
              break;
            }
            append_pp_list(args[args_count], node->token);
          }
          args_count++;
          valid = 1;
        }
      }

      if(!valid) {
        error("macro arguments list is not terminated.\n");
      }

      for(int i = 0; i < args_count; i++) {
        if(args[i]->head == NULL) {
          continue;
        }
        if(args[i]->head->token->type == PP_NEW_LINE || args[i]->head->token->type == PP_SPACE) {
          args[i]->head = args[i]->head->next;
        }
        if(args[i]->head != NULL) {
          struct pp_node **arg_node = &(args[i]->head);
          for(; (*arg_node)->next != NULL; arg_node = &((*arg_node)->next));
          if((*arg_node)->token->type == PP_NEW_LINE || (*arg_node)->token->type == PP_SPACE) {
            *arg_node = NULL;
          }
        }
      }

      struct pp_list *list = function_macro_invocation(pp, macro, args, args_count);
      concat_pp_list(result, list);
    }

    // not replaced
    else {
      append_pp_list(result, node->token);
    }
  }

  return result;
}

struct pp_list *concat_macro_token(struct pp_list *list) {
  struct pp_list *result = allocate_pp_list();

  for(struct pp_node *node = list->head; node != NULL; node = node->next) {
    struct pp_node *left = node;
    struct pp_node *middle = left->next;
    if(middle != NULL && middle->token->type == PP_SPACE) {
      middle = middle->next;
    }
    struct pp_node *right = middle != NULL ? middle->next : NULL;
    if(right != NULL && right->token->type == PP_SPACE) {
      right = right->next;
    }

    if(middle != NULL && right != NULL && middle->token->type == PP_CONCAT && !middle->token->concat) {
      struct pp_token *l = left->token;
      struct pp_token *r = right->token;

      struct string *str = allocate_string();
      concat_string(str, l->text);
      concat_string(str, r->text);

      struct pp_token_lexer lexer;
      lexer.queue = (struct utf8c *) malloc(sizeof(struct utf8c) * (str->size + 1));
      lexer.queue_head = 0;
      lexer.queue_size = 0;
      lexer.queue_allocate_size = str->size + 1;
      lexer.context = CTX_NORMAL;

      for(int i = 0; i < str->size;) {
        struct utf8c uc;
        uc.bytes = count_bytes(str->head[i]);
        for(int j = 0; j < uc.bytes; j++) {
          uc.sequence[j] = str->head[i++];
        }
        uc.sequence[uc.bytes] = '\0';
        lexer.queue[lexer.queue_size++] = uc;
      }
      lexer.queue[lexer.queue_size++] = ueof;

      struct pp_token *new_token = next_pp_token(&lexer);
      new_token->concat = 1;
      if(lexer.queue_size > 1) {
        error("invalid token concatnation: %s ## %s\n", l->text->head, r->text->head);
      }
      append_pp_list(result, new_token);

      node = right;
    } else {
      append_pp_list(result, node->token);
    }
  }

  // remove place marker
  for(struct pp_node **node = &(result->head); *node != NULL;) {
    if((*node)->token->type == PP_PLACE_MARKER) {
      *node = (*node)->next;
    } else {
      node = &((*node)->next);
    }
  }

  return result;
}

struct pp_list *object_macro_invocation(struct preprocessor *pp, struct macro_entry *macro) {
  macro->expanded = 1;

  struct pp_list *list = allocate_pp_list();
  for(struct pp_node *node = macro->replacement_list->head; node != NULL; node = node->next) {
    append_pp_list(list, node->token);
  }

  struct pp_list *concat_list = concat_macro_token(list);
  struct pp_list *result = scan_macro(pp, concat_list);

  macro->expanded = 0;
  return result;
}

struct pp_list *function_macro_invocation(struct preprocessor *pp, struct macro_entry *macro, struct pp_list **args, int args_size) {
  macro->expanded = 1;

  struct pp_list *replaced_args[MACRO_PARAMS_LIMIT];
  for(int i = 0; i < args_size; i++) {
    if(args[i]->head != NULL) {
      replaced_args[i] = scan_macro(pp, args[i]);
    } else {
      struct pp_token *place_marker = allocate_pp_token();
      place_marker->type = PP_PLACE_MARKER;
      place_marker->name = pp_token_name[PP_PLACE_MARKER];

      replaced_args[i] = allocate_pp_list();
      append_pp_list(replaced_args[i], place_marker);

      append_pp_list(args[i], place_marker);
    }
  }

  struct pp_list *list = allocate_pp_list();
  for(struct pp_node *node = macro->replacement_list->head; node != NULL; node = node->next) {
    if(node->token->type == PP_SHARP) {
      node = node->next;
      if(node->token->type == PP_SPACE) {
        node = node->next;
      }

      int matched;
      for(int i = 0; i < args_size; i++) {
        if(node->token->type == PP_IDENT && strcmp(node->token->text->head, macro->parameters[i]->head) == 0) {
          matched = i;
        }
      }

      struct pp_token *new_token = allocate_pp_token();
      new_token->type = PP_STR;
      new_token->name = pp_token_name[PP_STR];
      append_string(new_token->text, '"');
      for(struct pp_node *arg_node = args[matched]->head; arg_node != NULL; arg_node = arg_node->next) {
        for(int i = 0; i < arg_node->token->text->size; i++) {
          unsigned char c = arg_node->token->text->head[i];
          if(c == '\\' || c == '"') {
            append_string(new_token->text, '\\');
          }
          append_string(new_token->text, c);
        }
      }
      append_string(new_token->text, '"');
      append_pp_list(list, new_token);

      continue;
    }

    struct pp_node *left = node;
    struct pp_node *middle = left->next;
    if(middle != NULL && middle->token->type == PP_SPACE) {
      middle = middle->next;
    }
    struct pp_node *right = middle != NULL ? middle->next : NULL;
    if(right != NULL && right->token->type == PP_SPACE) {
      right = right->next;
    }
    if(middle != NULL && middle->token->type == PP_CONCAT && !middle->token->concat) {
      int left_replaced = 0;
      for(int i = 0; i < args_size; i++) {
        if(left->token->type == PP_IDENT && strcmp(left->token->text->head, macro->parameters[i]->head) == 0) {
          for(struct pp_node *arg_node = args[i]->head; arg_node != NULL; arg_node = arg_node->next) {
            append_pp_list(list, arg_node->token);
          }
          left_replaced = 1;
          break;
        }
      }
      if(!left_replaced) {
        append_pp_list(list, left->token);
      }
      if(left->next != middle) {
        append_pp_list(list, left->next->token);
      }
      append_pp_list(list, middle->token);
      if(middle->next != right) {
        append_pp_list(list, middle->next->token);
      }
      int right_replaced = 0;
      for(int i = 0; i < args_size; i++) {
        if(right->token->type == PP_IDENT && strcmp(right->token->text->head, macro->parameters[i]->head) == 0) {
          for(struct pp_node *arg_node = args[i]->head; arg_node != NULL; arg_node = arg_node->next) {
            append_pp_list(list, arg_node->token);
          }
          right_replaced = 1;
          break;
        }
      }
      if(!right_replaced) {
        append_pp_list(list, right->token);
      }
      node = right;
      continue;
    }

    int replaced = 0;
    for(int i = 0; i < args_size; i++) {
      if(node->token->type == PP_IDENT && strcmp(node->token->text->head, macro->parameters[i]->head) == 0) {
        for(struct pp_node *arg_node = replaced_args[i]->head; arg_node != NULL; arg_node = arg_node->next) {
          append_pp_list(list, arg_node->token);
        }
        replaced = 1;
        break;
      }
    }

    if(!replaced) {
      append_pp_list(list, node->token);
    }
  }

  struct pp_list *concat_list = concat_macro_token(list);
  struct pp_list *result = scan_macro(pp, concat_list);

  macro->expanded = 0;
  return result;
}

// preprocessor
struct pp_token *peek_pp_token(struct preprocessor *pp) {
  if(pp->token_queue_size == 0) {
    pp->token_queue[pp->token_queue_size++] = next_pp_token(pp->lexer);
  }
  return pp->token_queue[0];
}

struct pp_token *read_pp_token(struct preprocessor *pp) {
  if(pp->token_queue_size == 0) {
    pp->token_queue[pp->token_queue_size++] = next_pp_token(pp->lexer);
  }
  return pp->token_queue[--pp->token_queue_size];
}

int check_pp_token(struct preprocessor *pp, enum pp_token_type type) {
  return peek_pp_token(pp)->type == type;
}

int check_keyword(struct preprocessor *pp, char *text) {
  struct pp_token *token = peek_pp_token(pp);
  return token->type == PP_IDENT && strcmp(token->text->head, text) == 0;
}

void skip_pp_token(struct preprocessor *pp) {
  struct pp_token *token = read_pp_token(pp);
  free_pp_token(token);
}

void remove_white_space(struct preprocessor *pp) {
  if(check_pp_token(pp, PP_SPACE)) skip_pp_token(pp);
}

struct pp_token *expect_pp_token(struct preprocessor *pp, enum pp_token_type type) {
  struct pp_token *token = read_pp_token(pp);
  if(token->type != type) error("%s is expected.\n", pp_token_name[type]);
  return token;
}

void discard_new_line(struct preprocessor *pp) {
  struct pp_token *token = expect_pp_token(pp, PP_NEW_LINE);
  free_pp_token(token);
}

struct pp_token *read_pp_token_with_space(struct preprocessor *pp) {
  struct pp_token *token = read_pp_token(pp);
  remove_white_space(pp);
  return token;
}

void skip_pp_token_with_space(struct preprocessor *pp) {
  struct pp_token *token = read_pp_token(pp);
  free_pp_token(token);
  remove_white_space(pp);
}

#define unexpected_pp_token(pp) { \
  struct pp_token *token = read_pp_token(pp); \
  error("unexpected preprocessing token: %s.", pp_token_name[token->type]); \
}

// if_directive
int primary_expression(struct pp_node **node) {
  if((*node)->token->type == PP_NUM) {
    int value = 0;
    int base = 10;
    unsigned char *s = (*node)->token->text->head;
    for(int i = 0; i < (*node)->token->text->size; i++) {
      char c = s[i];
      if(i == 0 && c == '0') {
        base = 8;
      }
      if(i == 1 && (c == 'x' || c == 'X') && base == 8) {
        base = 16;
        break;
      }
      if(c == 'l' || c == 'L') break;
      if(base == 10) {
        if('0' <= c && c <= '9') {
          value = value * 10 + (c - '0');
        } else {
          error("invalid integer constant: \"%s\"\n", (*node)->token->text->head);
        }
      } else if(base == 8) {
        if('0' <= c && c <= '7') {
          value = value * 8 + (c - '0');
        } else {
          error("invalid integer constant: \"%s\"\n", (*node)->token->text->head);
        }
      } else if(base == 16) {
        if('0' <= c && c <= '9') {
          value = value * 16 + (c - '0');
        } else if('a' <= c && c <= 'f') {
          value = value * 16 + (c - 'a' + 10);
        } else if('A' <= c && c <= 'F') {
          value = value * 16 + (c - 'A' + 10);
        } else {
          error("invalid integer constant: \"%s\"\n", (*node)->token->text->head);
        }
      }
    }
    *node = (*node)->next;
    return value;
  } else if((*node)->token->type == PP_CHAR) {
    int value = (*node)->token->text->head[0];
    *node = (*node)->next;
    return value;
  } else if((*node)->token->type == PP_LPAREN) {
    *node = (*node)->next;
    int value = conditional_expression(node);
    *node = (*node)->next;
    return value;
  }
  error("invalid integer constant expression: %s\n", (*node)->token->text->head);
}

int unary_expression(struct pp_node **node) {
  if((*node)->token->type == PP_PLUS) {
    *node = (*node)->next;
    return +primary_expression(node);
  } else if((*node)->token->type == PP_MINUS) {
    *node = (*node)->next;
    return -primary_expression(node);
  } else if((*node)->token->type == PP_TILDE) {
    *node = (*node)->next;
    return ~primary_expression(node);
  } else if((*node)->token->type == PP_NOT) {
    *node = (*node)->next;
    return !primary_expression(node);
  }
  return primary_expression(node);
}

int multiplicative_expression(struct pp_node **node) {
  int result = unary_expression(node);
  while((*node) && ((*node)->token->type == PP_MUL || (*node)->token->type == PP_DIV || (*node)->token->type == PP_MOD)) {
    enum pp_token_type type = (*node)->token->type;
    *node = (*node)->next;
    int value = unary_expression(node);
    if(type == PP_MUL) {
      result = result * value;
    } else if(type == PP_DIV) {
      result = result / value;
    } else if(type == PP_MOD) {
      result = result % value;
    }
  }
  return result;
}

int additive_expression(struct pp_node **node) {
  int result = multiplicative_expression(node);
  while((*node) && ((*node)->token->type == PP_PLUS || (*node)->token->type == PP_MINUS)) {
    enum pp_token_type type = (*node)->token->type;
    *node = (*node)->next;
    int value = multiplicative_expression(node);
    if(type == PP_PLUS) {
      result = result + value;
    } else if(value == PP_MINUS) {
      result = result - value;
    }
  }
  return result;
}

int shift_expression(struct pp_node **node) {
  int result = additive_expression(node);
  while((*node) && ((*node)->token->type == PP_LSHIFT || (*node)->token->type == PP_RSHIFT)) {
    enum pp_token_type type = (*node)->token->type;
    *node = (*node)->next;
    int value = additive_expression(node);
    if(type == PP_LSHIFT) {
      result = result << value;
    } else if(type == PP_RSHIFT) {
      result = result >> value;
    }
  }
  return result;
}

int relational_expression(struct pp_node **node) {
  int result = shift_expression(node);
  while((*node) && ((*node)->token->type == PP_LT || (*node)->token->type == PP_GT || (*node)->token->type == PP_LTE || (*node)->token->type == PP_GTE)) {
    enum pp_token_type type = (*node)->token->type;
    *node = (*node)->next;
    int value = shift_expression(node);
    if(type == PP_LT) {
      result = result < value;
    } else if(type == PP_GT) {
      result = result > value;
    } else if(type == PP_LTE) {
      result = result <= value;
    } else if(type == PP_GTE) {
      result = result >= value;
    }
  }
  return result;
}

int equality_expression(struct pp_node **node) {
  int result = relational_expression(node);
  while((*node) && ((*node)->token->type == PP_EQ || (*node)->token->type == PP_NEQ)) {
    enum pp_token_type type = (*node)->token->type;
    *node = (*node)->next;
    int value = relational_expression(node);
    if(type == PP_EQ) {
      result = result == value;
    } else if(type == PP_NEQ) {
      result = result == value;
    }
  }
  return result;
}

int and_expression(struct pp_node **node) {
  int result = equality_expression(node);
  while((*node) && (*node)->token->type == PP_AND) {
    *node = (*node)->next;
    int value = equality_expression(node);
    result = result & value;
  }
  return result;
}

int exclusive_or_expression(struct pp_node **node) {
  int result = and_expression(node);
  while((*node) && (*node)->token->type == PP_XOR) {
    *node = (*node)->next;
    int value = and_expression(node);
    result = result ^ value;
  }
  return result;
}

int inclusive_or_expression(struct pp_node **node) {
  int result = exclusive_or_expression(node);
  while((*node) && (*node)->token->type == PP_OR) {
    *node = (*node)->next;
    int value = exclusive_or_expression(node);
    result = result | value;
  }
  return result;
}

int logical_and_expression(struct pp_node **node) {
  int result = inclusive_or_expression(node);
  while((*node) && (*node)->token->type == PP_LAND) {
    *node = (*node)->next;
    int value = inclusive_or_expression(node);
    result = result && value;
  }
  return result;
}

int logical_or_expression(struct pp_node **node) {
  int result = logical_and_expression(node);
  while((*node) && (*node)->token->type == PP_LOR) {
    *node = (*node)->next;
    int value = logical_and_expression(node);
    result = result || value;
  }
  return result;
}

int conditional_expression(struct pp_node **node) {
  int result = logical_or_expression(node);
  if((*node) && (*node)->token->type == PP_QUESTION) {
    *node = (*node)->next;
    int left = conditional_expression(node);
    *node = (*node)->next;
    int right = conditional_expression(node);
    result = result ? left : right;
  }
  return result;
}

struct pp_node *check_defined_operator(struct pp_node **node) {
  struct pp_node *ident;

  *node = (*node)->next;
  if((*node)->token->type == PP_SPACE) {
    *node = (*node)->next;
  }

  if((*node)->token->type == PP_IDENT) {
    ident = *node;
  } else if((*node)->token->type == PP_LPAREN) {
    *node = (*node)->next;
    if((*node)->token->type == PP_SPACE) {
      *node = (*node)->next;
    }

    if((*node)->token->type == PP_IDENT) {
      ident = *node;
      *node = (*node)->next;

      if((*node)->token->type != PP_RPAREN) {
        error("%s is expected.\n", pp_token_name[PP_RPAREN]);
      }
    } else {
      error("%s is expected.\n", pp_token_name[PP_IDENT]);
    }
  } else {
    error("%s or %s is expected.\n", pp_token_name[PP_IDENT], pp_token_name[PP_RPAREN]);
  }

  return ident;
}

int if_control(struct preprocessor *pp) {
  struct pp_list *list = allocate_pp_list();
  while(!check_pp_token(pp, PP_NEW_LINE)) {
    struct pp_token *token = read_pp_token(pp);
    append_pp_list(list, token);
  }
  discard_new_line(pp);

  for(struct pp_node *node = list->head; node != NULL; node = node->next) {
    if(node->token->type == PP_IDENT && strcmp(node->token->text->head, "defined") == 0) {
      struct pp_node *ident = check_defined_operator(&node);
      ident->skip = 1;
    }
  }

  list = scan_macro(pp, list);

  struct pp_token *zero = allocate_pp_token();
  zero->type = PP_NUM;
  zero->name = pp_token_name[PP_NUM];
  append_string(zero->text, '0');

  struct pp_token *one = allocate_pp_token();
  one->type = PP_NUM;
  one->name = pp_token_name[PP_NUM];
  append_string(one->text, '1');

  struct pp_list *replaced = allocate_pp_list();
  for(struct pp_node *node = list->head; node != NULL; node = node->next) {
    if(node->token->type == PP_IDENT && strcmp(node->token->text->head, "defined") == 0) {
      struct pp_node *ident = check_defined_operator(&node);
      struct macro_entry *macro = search_macro_table(ident->token->text->head);
      append_pp_list(replaced, macro == NULL ? zero : one);
    } else {
      append_pp_list(replaced, node->token->type != PP_IDENT ? node->token : zero);
    }
  }

  for(struct pp_node **node = &(replaced->head); *node != NULL;) {
    if((*node)->token->type == PP_SPACE || (*node)->token->type == PP_SPACE) {
      *node = (*node)->next;
    } else {
       node = &((*node)->next);
    }
  }

  return conditional_expression(&(replaced->head));
}

void conditional_include(struct preprocessor *pp, int condition) {
  if(condition) group(pp);
  else skip_group(pp);
}

int if_directive(struct preprocessor *pp) {
  skip_pp_token_with_space(pp);

  int control = if_control(pp);
  conditional_include(pp, control);

  return control;
}

int ifdef_directive(struct preprocessor *pp) {
  skip_pp_token_with_space(pp);

  struct pp_token *ident = expect_pp_token(pp, PP_IDENT);
  discard_new_line(pp);

  struct macro_entry *macro = search_macro_table(ident->text->head);
  int control = macro != NULL;
  conditional_include(pp, control);

  return control;
}

int ifndef_directive(struct preprocessor *pp) {
  skip_pp_token_with_space(pp);

  struct pp_token *ident = expect_pp_token(pp, PP_IDENT);
  discard_new_line(pp);

  struct macro_entry *macro = search_macro_table(ident->text->head);
  int control = macro == NULL;
  conditional_include(pp, control);

  return control;
}

int elif_directive(struct preprocessor *pp, int skip) {
  skip_pp_token_with_space(pp);

  int control = if_control(pp);
  conditional_include(pp, !skip && control);

  return skip || control;
}

void else_directive(struct preprocessor *pp, int skip) {
  skip_pp_token_with_space(pp);
  discard_new_line(pp);

  conditional_include(pp, !skip);
}

void endif_directive(struct preprocessor *pp) {
  skip_pp_token_with_space(pp);
  discard_new_line(pp);
}

void if_section(struct preprocessor *pp) {
  int skip;

  if(check_keyword(pp, "if")) {
    skip = if_directive(pp);
  } else if(check_keyword(pp, "ifdef")) {
    skip = ifdef_directive(pp);
  } else if(check_keyword(pp, "ifndef")) {
    skip = ifndef_directive(pp);
  }

  while(check_keyword(pp, "elif")) {
    skip = elif_directive(pp, skip);
  }

  if(check_keyword(pp, "else")) {
    else_directive(pp, skip);
  }

  if(check_keyword(pp, "endif")) {
    endif_directive(pp);
  } else {
    error("#endif directive is missing.\n");
  }
}

// include directive
int try_fopen(char *file) {
  FILE *fp = fopen(file, "r");

  if(fp == NULL) return 0;

  fclose(fp);
  return 1;
}

struct string *search_header_file(struct string *text) {
  char location[4][64] = {
    "/usr/lib/gcc/x86_64-linux-gnu/7/include/",
    "/usr/include/",
    "/usr/include/linux/",
    "/usr/include/x86_64-linux-gnu/"
  };

  for(int k = 0; k < 4; k++) {
    struct string *path = allocate_string();
    write_string(path, location[k]);
    for(int i = 1; i < text->size - 1; i++) {
      append_string(path, text->head[i]);
    }

    if(try_fopen(path->head)) return path;

    free_string(path);
  }

  // failed to search header file
  return NULL;
}

struct string *search_named_source_file(struct string *text, const char *current_file) {
  int last_slash = 0;
  for(int i = 0; current_file[i]; i++) {
    if(current_file[i] == '/') {
      last_slash = i;
    }
  }

  struct string *dir = allocate_string();
  if(last_slash > 0) {
    for(int i = 0; i < last_slash; i++) {
      append_string(dir, current_file[i]);
    }
    append_string(dir, '/');
  }

  struct string *path = allocate_string();
  concat_string(path, dir);
  for(int i = 1; i < text->size - 1; i++) {
    append_string(path, text->head[i]);
  }

  if(try_fopen(path->head)) {
    return path;
  }

  free_string(path);

  // reprocess as if header file was read
  return search_header_file(text);
}

void include_directive(struct preprocessor *pp) {
  struct pp_token *header;
  if(peek_pp_token(pp)->type == PP_H_NAME) {
    header = read_pp_token(pp);
  } else {
    error("macro-replaced include directive is not implemented yet.\n");
  }
  discard_new_line(pp);

  struct string *path;
  if(header->type == PP_H_NAME && header->text->head[0] == '<') {
    path = search_header_file(header->text);
  } else if(header->type == PP_H_NAME && header->text->head[0] == '"') {
    path = search_named_source_file(header->text, pp->lexer->src->file);
  }

  if(path == NULL) {
    error("failed to search include file\n");
  }

  struct pp_list *list = parse_preprocessing_file(path->head);
  concat_pp_list(pp->list, list);
}

// define, undef directive
int check_parameter(struct macro_entry *macro, struct pp_token *token) {
  int parameter_size = macro->parameter_size;
  if(macro->parameter_ellipsis) parameter_size++;

  if(token->type == PP_IDENT) {
    for(int i = 0; i < parameter_size; i++) {
      if(strcmp(token->text->head, macro->parameters[i]->head) == 0) {
        return 1;
      }
    }
  }

  return 0;
}

int check_stringify_operator(struct macro_entry *macro) {
  struct pp_list *list = macro->replacement_list;

  for(struct pp_node *node = list->head; node != NULL; node = node->next) {
    if(node->token->type == PP_SHARP) {
      struct pp_node *target = node->next;

      if(target == NULL) return 0;
      if(target->token->type == PP_SPACE) target = target->next;
      if(!check_parameter(macro, target->token)) return 0;

      node = target;
    }
  }

  return 1;
}

int check_concat_operator(struct macro_entry *macro) {
  struct pp_list *list = macro->replacement_list;

  for(struct pp_node *node = list->head; node != NULL; node = node->next) {
    if(node->token->type == PP_CONCAT) {
      if(node == list->head) return 0;
      if(node->next == NULL) return 0;
    }
  }

  return 1;
}

void define_directive(struct preprocessor *pp) {
  struct macro_entry *macro = allocate_macro_entry();

  macro->identifier = expect_pp_token(pp, PP_IDENT)->text->head;

  if(check_pp_token(pp, PP_SPACE) || check_pp_token(pp, PP_NEW_LINE)) {
    macro->type = MACRO_OBJECT;
    remove_white_space(pp);
  } else if(check_pp_token(pp, PP_LPAREN)) {
    macro->type = MACRO_FUNCTION;
    macro->parameter_size = 0;

    skip_pp_token_with_space(pp);

    if(check_pp_token(pp, PP_RPAREN)) {
      macro->parameter_ellipsis = 0;
      skip_pp_token_with_space(pp);
    } else {
      while(1) {
        if(check_pp_token(pp, PP_IDENT)) {
          if(macro->parameter_size == MACRO_PARAMS_LIMIT) {
            error("too many macro parameters.\n");
          }

          struct pp_token *token = read_pp_token_with_space(pp);
          macro->parameters[macro->parameter_size++] = token->text;

          if(check_pp_token(pp, PP_COMMA)) {
            skip_pp_token_with_space(pp);
          } else if(check_pp_token(pp, PP_RPAREN)) {
            macro->parameter_ellipsis = 0;
            skip_pp_token_with_space(pp);
            break;
          } else {
            unexpected_pp_token(pp);
          }
        } else if(check_pp_token(pp, PP_ELLIPSIS)) {
          skip_pp_token_with_space(pp);

          struct string *va_args = allocate_string();
          write_string(va_args, "__VA_ARGS__");
          macro->parameters[macro->parameter_size] = va_args;
          macro->parameter_ellipsis = 1;

          if(check_pp_token(pp, PP_RPAREN)) {
            skip_pp_token_with_space(pp);
            break;
          } else {
            unexpected_pp_token(pp);
          }
        }
        else {
          unexpected_pp_token(pp);
        }
      }
    }
  } else {
    unexpected_pp_token(pp);
  }

  while(!check_pp_token(pp, PP_NEW_LINE)) {
    struct pp_token *token = read_pp_token(pp);
    append_pp_list(macro->replacement_list, token);
  }

  discard_new_line(pp);

  if(macro->type == MACRO_FUNCTION && !check_stringify_operator(macro)) {
    error("invalid # operator.\n");
  }
  if(!check_concat_operator(macro)) {
    error("invalid ## operator.\n");
  }

  insert_macro_table(macro);
}

void undef_directive(struct preprocessor *pp) {
  struct pp_token *ident = expect_pp_token(pp, PP_IDENT);
  discard_new_line(pp);
  delete_macro_table(ident->text->head);
}

// text line
void parse_text_line(struct preprocessor *pp) {
  struct pp_list *text = allocate_pp_list();

  while(1) {
    struct pp_token *token = read_pp_token(pp);
    append_pp_list(text, token);
    if(token->type == PP_NEW_LINE) {
      struct pp_token *next = peek_pp_token(pp);
      if(next->type == PP_SHARP || next->type == PP_NONE) {
        break;
      }
    }
  }

  struct pp_list *replaced = scan_macro(pp, text);
  concat_pp_list(pp->list, replaced);
}

// group
int check_if_section(struct preprocessor *pp) {
  if(check_keyword(pp, "if")) return 1;
  if(check_keyword(pp, "ifdef")) return 1;
  if(check_keyword(pp, "ifndef")) return 1;
  return 0;
}

int check_group_end(struct preprocessor *pp) {
  if(check_keyword(pp, "elif")) return 1;
  if(check_keyword(pp, "else")) return 1;
  if(check_keyword(pp, "endif")) return 1;
  return 0;
}

void group(struct preprocessor *pp) {
  while(!check_pp_token(pp, PP_NONE)) {
    if(check_pp_token(pp, PP_SHARP)) {
      skip_pp_token_with_space(pp);

      if(check_if_section(pp)) {
        if_section(pp);
      } else if(check_group_end(pp)) {
        break;
      } else if(check_keyword(pp, "include")) {
        skip_pp_token_with_space(pp);
        include_directive(pp);
      } else if(check_keyword(pp, "define")) {
        skip_pp_token_with_space(pp);
        define_directive(pp);
      } else if(check_keyword(pp, "undef")) {
        skip_pp_token_with_space(pp);
        undef_directive(pp);
      } else if(check_keyword(pp, "line")) {
        warning("#line directive is not implemented yet.\n");
        skip_line(pp);
      } else if(check_keyword(pp, "error")) {
        warning("#error directive is not implemented yet.\n");
        skip_line(pp);
      } else if(check_keyword(pp, "pragma")) {
        warning("#pragma directive is not implemented yet.\n");
        skip_line(pp);
      } else if(check_pp_token(pp, PP_SPACE)) {
        skip_line(pp);
      } else {
        struct pp_token *directive = read_pp_token(pp);
        error("unknown directive: \"#%s\".\n", directive->text->head);
      }
    } else {
      parse_text_line(pp);
    }
  }
}

void skip_line(struct preprocessor *pp) {
  while(1) {
    struct pp_token *token = read_pp_token(pp);
    if(token->type == PP_NEW_LINE) break;
  }
}

void skip_group(struct preprocessor *pp) {
  while(peek_pp_token(pp)->type != PP_NONE) {
    if(peek_pp_token(pp)->type == PP_SHARP) {
      read_pp_token_with_space(pp);

      if(check_if_section(pp)) {
        skip_line(pp);
        skip_group(pp);
        while(check_keyword(pp, "elif")) {
          skip_line(pp);
          skip_group(pp);
        }
        if(check_keyword(pp, "else")) {
          skip_line(pp);
          skip_group(pp);
        }
        if(check_keyword(pp, "endif")) {
          skip_line(pp);
        }
      } else if(check_group_end(pp)) {
        break;
      } else {
        skip_line(pp);
      }
    } else {
      skip_line(pp);
    }
  }
}

struct pp_list *parse_preprocessing_file(unsigned char *file) {
  struct preprocessor pp;
  pp.lexer = allocate_pp_token_lexer(file);
  pp.token_queue_size = 0;
  pp.list = allocate_pp_list();

  group(&pp);

  if(!check_pp_token(&pp, PP_NONE)) {
    if(check_keyword(&pp, "elif")) {
      error("invalid #elif directive appeared.\n");
    } else if(check_keyword(&pp, "else")) {
      error("invalid #else directive appeared.\n");
    } else if(check_keyword(&pp, "endif")) {
      error("invalid #endif directive appeared.\n");
    }
  }

  return pp.list;
}

struct pp_list *preprocess(unsigned char *file) {
  struct macro_entry *arch = allocate_macro_entry();
  arch->identifier = "__x86_64__";
  insert_macro_table(arch);

  struct pp_list *list = parse_preprocessing_file(file);

  for(int i = 0; i < MACRO_TABLE_SIZE; i++) {
    if(macro_table[i] != NULL) {
      free_macro_entry(macro_table[i]);
    }
  }

  return list;
}
