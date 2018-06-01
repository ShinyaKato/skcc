#include "preprocess.h"

struct pp_list *object_macro_invocation(struct preprocessor *pp, struct macro_entry *macro);
struct pp_list *function_macro_invocation(struct preprocessor *pp, struct macro_entry *macro, struct pp_list **args, int args_size);

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

int ident_hash_slide(const unsigned char *ident) {
  const int BASE = 263;
  int h = 0;
  for(int i = 0; ident[i] != '\0'; i++) {
    h = (h * BASE + ident[i]) % MACRO_TABLE_SLIDE_MOD;
  }
  return MACRO_TABLE_SLIDE_MOD - h + 1;
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

void insert_macro_table(struct preprocessor *pp, struct macro_entry *macro) {
  int h1 = ident_hash(macro->identifier);
  int h2 = ident_hash_slide(macro->identifier);
  for(int i = 0, h = h1; i < MACRO_TABLE_SIZE; i++, h = (h + h2) % MACRO_TABLE_SIZE) {
    if(pp->macro_table[h] == NULL) {
      pp->macro_table[h] = macro;
      break;
    } else {
      if(strcmp(pp->macro_table[h]->identifier, macro->identifier) == 0) {
        if(compare_macro(pp->macro_table[h], macro)) {
          break;
        } else {
          error("duplicated macro definition.\n");
        }
      }
    }
  }
}

void delete_macro_table(struct preprocessor *pp, const unsigned char *identifier) {
  int h1 = ident_hash(identifier);
  int h2 = ident_hash_slide(identifier);
  for(int i = 0, h = h1; i < MACRO_TABLE_SIZE; i++, h = (h + h2) % MACRO_TABLE_SIZE) {
    if(pp->macro_table[h] == NULL) break;
    if(strcmp(pp->macro_table[h]->identifier, identifier) == 0) {
      pp->macro_table[h] = NULL;
      for(h += h2; i < MACRO_TABLE_SIZE; i++, h += h2) {
        if(pp->macro_table[h] == NULL) break;
        struct macro_entry *t = pp->macro_table[h];
        pp->macro_table[h] = NULL;
        insert_macro_table(pp, t);
      }
      break;
    }
  }
}

struct macro_entry *search_macro_table(struct preprocessor *pp, const unsigned char *identifier) {
  int h1 = ident_hash(identifier);
  int h2 = ident_hash_slide(identifier);
  for(int i = 0, h = h1; i < MACRO_TABLE_SIZE; i++, h = (h + h2) % MACRO_TABLE_SIZE) {
    if(pp->macro_table[h] == NULL) break;
    if(strcmp(pp->macro_table[h]->identifier, identifier) == 0) {
      return pp->macro_table[h];
    }
  }
  return NULL;
}

// preprocessor
struct preprocessor *allocate_preprocessor(const unsigned char *file) {
  struct preprocessor *pp = (struct preprocessor *) malloc(sizeof(struct preprocessor));
  if(pp == NULL) {
    perror("malloc");
    exit(1);
  }
  pp->lexer = allocate_pp_token_lexer(file);
  pp->token_queue_size = 0;
  pp->list = allocate_pp_list();
  for(int i = 0; i < MACRO_TABLE_SIZE; i++) {
    pp->macro_table[i] = NULL;
  }
  return pp;
}

void free_preprocessor(struct preprocessor *pp) {
  free_pp_token_lexer(pp->lexer);
  free_pp_list(pp->list);
  free(pp);
}

struct pp_token *peek_pp_token(struct preprocessor *pp) {
  if(pp->token_queue_size == 0) {
    pp->token_queue[pp->token_queue_size++] = next_pp_token(pp->lexer);
  }
  return pp->token_queue[0];
}

struct pp_token *pop_pp_token(struct preprocessor *pp) {
  if(pp->token_queue_size == 0) {
    pp->token_queue[pp->token_queue_size++] = next_pp_token(pp->lexer);
  }
  return pp->token_queue[--pp->token_queue_size];
}

struct pp_token *pop_pp_token_with_space(struct preprocessor *pp) {
  struct pp_token *token = pop_pp_token(pp);
  if(peek_pp_token(pp)->type == PP_SPACE) {
    pop_pp_token(pp);
  }
  return token;
}

// macro replacement
int check_object_macro_invocation(struct preprocessor *pp, struct pp_node *node) {
  if(node->token->type != PP_IDENT) return 0;
  if(node->skip) return 0;

  struct macro_entry *macro = search_macro_table(pp, node->token->text->head);
  return macro != NULL && !macro->expanded && macro->type == MACRO_OBJECT;
}

int check_function_macro_invocation(struct preprocessor *pp, struct pp_node *node) {
  if(node->token->type != PP_IDENT) return 0;
  if(node->skip) return 0;

  struct macro_entry *macro = search_macro_table(pp, node->token->text->head);
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
    struct macro_entry *macro = search_macro_table(pp, node->token->text->head);

    // object-like macro invocation
    if(check_object_macro_invocation(pp, node)) {
      struct macro_entry *macro = search_macro_table(pp, node->token->text->head);
      struct pp_list *list = object_macro_invocation(pp, macro);
      concat_pp_list(result, list);
    }

    // function-like macro invocation
    else if(check_function_macro_invocation(pp, node)) {
      struct macro_entry *macro = search_macro_table(pp, node->token->text->head);
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
        error("invalid token concatnation.\n");
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
      for(int i = 0; i < args_size; i++) {
        if(left->token->type == PP_IDENT && strcmp(left->token->text->head, macro->parameters[i]->head) == 0) {
          for(struct pp_node *arg_node = args[i]->head; arg_node != NULL; arg_node = arg_node->next) {
            append_pp_list(list, arg_node->token);
          }
          break;
        }
      }
      if(left->next != middle) {
        append_pp_list(list, left->next->token);
      }
      append_pp_list(list, middle->token);
      if(middle->next != right) {
        append_pp_list(list, middle->next->token);
      }
      for(int i = 0; i < args_size; i++) {
        if(right->token->type == PP_IDENT && strcmp(right->token->text->head, macro->parameters[i]->head) == 0) {
          for(struct pp_node *arg_node = args[i]->head; arg_node != NULL; arg_node = arg_node->next) {
            append_pp_list(list, arg_node->token);
          }
          break;
        }
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

// directives
void skip_line(struct preprocessor *pp) {
  while(1) {
    struct pp_token *token = pop_pp_token(pp);
    if(token->type == PP_NEW_LINE) break;
  }
}

void skip_group(struct preprocessor *pp) {
  while(peek_pp_token(pp)->type != PP_NONE) {
    if(peek_pp_token(pp)->type == PP_SHARP) {
      pop_pp_token_with_space(pp);

      struct pp_token *directive = peek_pp_token(pp);
      if(directive->type == PP_IDENT && strcmp(directive->text->head, "elif") == 0) {
        break;
      } else if(directive->type == PP_IDENT && strcmp(directive->text->head, "else") == 0) {
        break;
      } else if(directive->type == PP_IDENT && strcmp(directive->text->head, "endif") == 0) {
        break;
      }

      skip_line(pp);
    } else {
      skip_line(pp);
    }
  }
}

int conditional_expression(struct pp_node **node);
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

int if_condition(struct preprocessor *pp) {
  struct pp_list *list = allocate_pp_list();
  while(1) {
    struct pp_token *token = pop_pp_token(pp);
    if(token->type == PP_NEW_LINE) break;
    append_pp_list(list, token);
  }

  printf("  condition:\n    ");
  for(struct pp_node *node = list->head; node != NULL; node = node->next) {
    printf("%s", node->token->text->head);
  }
  printf("\n");

  for(struct pp_node *node = list->head; node != NULL; node = node->next) {
    if(node->token->type == PP_IDENT && strcmp(node->token->text->head, "defined") == 0) {
      if(node->next->token->type == PP_SPACE) node = node->next;
      struct pp_node *next = node->next;
      if(next->token->type == PP_IDENT) {
        next->skip = 1;
        node = next;
      } else if(next->token->type == PP_LPAREN) {
        if(next->next->token->type == PP_IDENT) {
          if(next->next->next->token->type != PP_RPAREN) {
            error("')' is expected.\n");
          }
          next->next->skip = 1;
          node = next->next->next;
        } else {
          error("identifier is expected.\n");
        }
      } else {
        error("identifier or '(' is expected.\n");
      }
    }
  }

  list = scan_macro(pp, list);

  printf("    ");
  for(struct pp_node *node = list->head; node != NULL; node = node->next) {
    printf("%s", node->token->text->head);
  }
  printf("\n");

  struct pp_list *replaced = allocate_pp_list();
  for(struct pp_node *node = list->head; node != NULL; node = node->next) {
    if(node->token->type == PP_IDENT && strcmp(node->token->text->head, "defined") == 0) {
      if(node->next->token->type == PP_SPACE) node = node->next;
      struct pp_node *next = node->next;
      if(next->token->type == PP_IDENT) {
        struct macro_entry *macro = search_macro_table(pp, next->token->text->head);
        if(macro) {
          struct pp_token *token = allocate_pp_token();
          token->type = PP_NUM;
          token->name = pp_token_name[token->type];
          append_string(token->text, '1');
          append_pp_list(replaced, token);
        } else {
          struct pp_token *token = allocate_pp_token();
          token->type = PP_NUM;
          token->name = pp_token_name[token->type];
          append_string(token->text, '0');
          append_pp_list(replaced, token);
        }
        node = next;
      } else if(next->token->type == PP_LPAREN) {
        if(next->next->token->type == PP_IDENT) {
          struct macro_entry *macro = search_macro_table(pp, next->next->token->text->head);
          if(macro) {
            struct pp_token *token = allocate_pp_token();
            token->type = PP_NUM;
            token->name = pp_token_name[token->type];
            append_string(token->text, '1');
            append_pp_list(replaced, token);
          } else {
            struct pp_token *token = allocate_pp_token();
            token->type = PP_NUM;
            token->name = pp_token_name[token->type];
            append_string(token->text, '0');
            append_pp_list(replaced, token);
          }
          if(next->next->next->token->type != PP_RPAREN) {
            error("')' is expected.\n");
          }
          node = next->next->next;
        } else {
          error("identifier is expected.\n");
        }
      } else {
        error("identifier or '(' is expected.\n");
      }
    } else {
      if(node->token->type == PP_IDENT) {
        struct pp_token *token = allocate_pp_token();
        token->type = PP_NUM;
        token->name = pp_token_name[token->type];
        append_string(token->text, '0');
        append_pp_list(replaced, token);
      } else {
        append_pp_list(replaced, node->token);
      }
    }
  }

  printf("    ");
  for(struct pp_node *node = replaced->head; node != NULL; node = node->next) {
    printf("%s", node->token->text->head);
  }
  printf("\n");

  for(struct pp_node **node = &(replaced->head); *node != NULL;) {
    if((*node)->token->type == PP_SPACE || (*node)->token->type == PP_SPACE) {
      *node = (*node)->next;
    } else {
       node = &((*node)->next);
    }
  }

  int condition = conditional_expression(&(replaced->head));
  printf("  result: %d\n", condition);
  return condition;
}

int ifdef_condition(struct preprocessor *pp) {
  struct pp_token *ident = pop_pp_token(pp);
  if(ident->type != PP_IDENT) {
    error("identifier is expected.\n");
  }
  if(pop_pp_token(pp)->type != PP_NEW_LINE) {
    error("invalid ifdef directive\n");
  }

  struct macro_entry *macro = search_macro_table(pp, ident->text->head);
  return macro != NULL;
}

int ifndef_condition(struct preprocessor *pp) {
  return !ifdef_condition(pp);
}

void if_directive(struct preprocessor *pp) {
  int condition = 0;

  printf("#if\n");

  if(if_condition(pp)) {
    group(pp);
    condition = 1;
  } else {
    skip_group(pp);
  }

  while(1) {
    struct pp_token *token = pop_pp_token(pp);
    if(strcmp(token->text->head, "elif") == 0) {
      printf("  #elif\n");
      pop_pp_token_with_space(pp);
      int control = if_condition(pp);
      if(!condition && control) {
        group(pp);
        condition = 1;
      } else {
        skip_group(pp);
      }
    } else if(strcmp(token->text->head, "else") == 0) {
      printf("  #else\n");
      pop_pp_token_with_space(pp);
      if(!condition) {
        group(pp);
      } else {
        skip_group(pp);
      }
    } else if(strcmp(token->text->head, "endif") == 0) {
      printf("  #endif\n");
      pop_pp_token_with_space(pp);
      break;
    }
  }
}

void ifdef_directive(struct preprocessor *pp) {
  int condition = 0;

  printf("#ifdef\n");

  if(ifdef_condition(pp)) {
    group(pp);
    condition = 1;
  } else {
    skip_group(pp);
  }

  while(1) {
    struct pp_token *token = pop_pp_token(pp);
    if(strcmp(token->text->head, "elif") == 0) {
      printf("  #elif\n");
      pop_pp_token_with_space(pp);
      int control = if_condition(pp);
      if(!condition && control) {
        group(pp);
        condition = 1;
      } else {
        skip_group(pp);
      }
    } else if(strcmp(token->text->head, "else") == 0) {
      printf("  #else\n");
      pop_pp_token_with_space(pp);
      if(!condition) {
        group(pp);
      } else {
        skip_group(pp);
      }
    } else if(strcmp(token->text->head, "endif") == 0) {
      printf("  #endif\n");
      pop_pp_token_with_space(pp);
      break;
    }
  }
}

void ifndef_directive(struct preprocessor *pp) {
  int condition = 0;

  printf("#ifndef\n");

  if(ifndef_condition(pp)) {
    group(pp);
    condition = 1;
  } else {
    skip_group(pp);
  }

  while(1) {
    struct pp_token *token = pop_pp_token(pp);
    if(strcmp(token->text->head, "elif") == 0) {
      printf("  #elif\n");
      pop_pp_token_with_space(pp);
      int control = if_condition(pp);
      if(!condition && control) {
        group(pp);
        condition = 1;
      } else {
        skip_group(pp);
      }
    } else if(strcmp(token->text->head, "else") == 0) {
      printf("  #else\n");
      pop_pp_token_with_space(pp);
      if(!condition) {
        group(pp);
      } else {
        skip_group(pp);
      }
    } else if(strcmp(token->text->head, "endif") == 0) {
      printf("  #endif\n");
      pop_pp_token_with_space(pp);
      break;
    }
  }
}

void include_directive(struct preprocessor *pp) {
  struct pp_token *header = pop_pp_token(pp);
  struct string *path = allocate_string();

  if(header->type == PP_H_NAME && header->text->head[0] == '<') {
    for(int i = 0; i < 13; i++) {
      append_string(path, "/usr/include/"[i]);
    }
    for(int i = 1; i < header->text->size - 1; i++) {
      append_string(path, header->text->head[i]);
    }
  } else if(header->type == PP_H_NAME && header->text->head[0] == '"') {
    if(header->text->head[1] == '/') {
      for(int i = 1; i < header->text->size - 1; i++) {
        append_string(path, header->text->head[i]);
      }
    } else {
      const unsigned char *file = pp->lexer->src->file;
      int dir_last = 0;
      for(int i = 0; file[i]; i++) {
        if(file[i] == '/') {
          dir_last = i;
        }
      }
      if(dir_last > 0) {
        for(int i = 0; i < dir_last; i++) {
          append_string(path, file[i]);
        }
        append_string(path, '/');
      }
      for(int i = 1; i < header->text->size - 1; i++) {
        append_string(path, header->text->head[i]);
      }
    }

    FILE *fp = fopen(path->head, "r");
    if(fp == NULL) {
      path = allocate_string();
      for(int i = 0; i < 13; i++) {
        append_string(path, "/usr/include/"[i]);
      }
      for(int i = 1; i < header->text->size - 1; i++) {
        append_string(path, header->text->head[i]);
      }
    } else {
      fclose(fp);
    }
  } else {
    error("macro-replaced include directive is not implemented yet.\n");
  }

  printf("#include \"%s\"\n", path->head);

  struct pp_token *next = pop_pp_token(pp);
  if(next->type != PP_NEW_LINE) {
    error("invalid include directive.\n");
  }

  struct preprocessor *new_pp = allocate_preprocessor(path->head);
  for(int i = 0; i < MACRO_TABLE_SIZE; i++) {
    new_pp->macro_table[i] = pp->macro_table[i];
  }

  struct pp_list *list = preprocessing_file(new_pp);
  concat_pp_list(pp->list, list);

  for(int i = 0; i < MACRO_TABLE_SIZE; i++) {
    pp->macro_table[i] = new_pp->macro_table[i];
  }
}

void define_directive(struct preprocessor *pp) {
  struct pp_token *ident = pop_pp_token(pp);
  if(ident->type != PP_IDENT) {
    error("invalid preprocessing macro name.\n");
  }

  struct macro_entry *macro = allocate_macro_entry();
  macro->identifier = ident->text->head;

  if(peek_pp_token(pp)->type == PP_SPACE || peek_pp_token(pp)->type == PP_NEW_LINE) {
    macro->type = MACRO_OBJECT;
    if(peek_pp_token(pp)->type == PP_SPACE) {
      pop_pp_token(pp);
    }
  } else if(peek_pp_token(pp)->type == PP_LPAREN) {
    macro->type = MACRO_FUNCTION;
    macro->parameter_size = 0;
    pop_pp_token_with_space(pp);

    // parse macro parameters
    if(macro->type == MACRO_FUNCTION) {
      if(peek_pp_token(pp)->type == PP_RPAREN) {
        macro->parameter_ellipsis = 0;
        pop_pp_token_with_space(pp);
      } else {
        while(1) {
          struct pp_token *token = pop_pp_token_with_space(pp);
          if(token->type == PP_IDENT) {
            macro->parameters[macro->parameter_size++] = token->text;
            if(peek_pp_token(pp)->type == PP_RPAREN) {
              macro->parameter_ellipsis = 0;
              pop_pp_token_with_space(pp);
              break;
            } else if(peek_pp_token(pp)->type == PP_COMMA) {
              pop_pp_token_with_space(pp);
            } else {
              error("invalid preprocessing function-like macro definition.\n");
            }
          } else if(token->type == PP_ELLIPSIS) {
            if(peek_pp_token(pp)->type == PP_RPAREN) {
              macro->parameter_ellipsis = 1;
              pop_pp_token_with_space(pp);
              break;
            } else {
              error("invalid preprocessing function-like macro definition.\n");
            }
          } else {
            error("invalid preprocessing function-like macro definition.\n");
          }
        }
      }
      if(macro->parameter_size > MACRO_PARAMS_LIMIT) {
        error("too many macro parameters.\n");
      }
    }
    if(macro->parameter_ellipsis) {
      struct string *va_args = allocate_string();
      for(int i = 0; i < 11; i++) {
        append_string(va_args, "__VA_ARGS__"[i]);
      }
      macro->parameters[macro->parameter_size] = va_args;
    }
  } else {
    error("invalid preprocessing macro definition.\n");
  }

  // parse macro replacement list
  while(1) {
    struct pp_token *token = pop_pp_token(pp);
    if(token->type == PP_NEW_LINE) break;
    append_pp_list(macro->replacement_list, token);
  }

  // check # operator
  if(macro->type == MACRO_FUNCTION) {
    for(struct pp_node *node = macro->replacement_list->head; node != NULL; node = node->next) {
      if(node->token->type == PP_SHARP) {
        int parameter = 0;
        if(node->next != NULL) {
          if(node->next->token->type == PP_SPACE) {
            node = node->next;
          }
          if(node->next != NULL ) {
            for(int i = 0; i < macro->parameter_size; i++) {
              if(strcmp(node->next->token->text->head, macro->parameters[i]->head) == 0) {
                parameter = 1;
                break;
              }
            }
          }
        }
        if(!parameter) {
          error("invalid # operator.\n");
        }
      }
    }
  }

  // check ## operator
  for(struct pp_node *node = macro->replacement_list->head; node != NULL; node = node->next) {
    if(node->token->type == PP_CONCAT) {
      if(node == macro->replacement_list->head || node->next == NULL) {
        error("invalid ## operator.\n");
      }
    }
  }

  insert_macro_table(pp, macro);
}

void undef_directive(struct preprocessor *pp) {
  struct pp_token *ident = pop_pp_token(pp);
  if(ident->type != PP_IDENT) {
    error("invalid preprocessing undef directive.\n");
  }
  if(pop_pp_token(pp)->type != PP_NEW_LINE) {
    error("invalid preprocessing undef directive.\n");
  }

  delete_macro_table(pp, ident->text->head);
}

void text_line(struct preprocessor *pp) {
  struct pp_list *text = allocate_pp_list();

  while(1) {
    struct pp_token *token = pop_pp_token(pp);
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

int group(struct preprocessor *pp) {
  while(peek_pp_token(pp)->type != PP_NONE) {
    if(peek_pp_token(pp)->type == PP_SHARP) {
      pop_pp_token_with_space(pp);

      struct pp_token *directive = peek_pp_token(pp);
      if(directive->type == PP_IDENT && strcmp(directive->text->head, "elif") == 0) {
        return 1;
      } else if(directive->type == PP_IDENT && strcmp(directive->text->head, "else") == 0) {
        return 1;
      } else if(directive->type == PP_IDENT && strcmp(directive->text->head, "endif") == 0) {
        return 1;
      }

      pop_pp_token_with_space(pp);

      if(directive->type == PP_IDENT && strcmp(directive->text->head, "if") == 0) {
        if_directive(pp);
      } else if(directive->type == PP_IDENT && strcmp(directive->text->head, "ifdef") == 0) {
        ifdef_directive(pp);
      } else if(directive->type == PP_IDENT && strcmp(directive->text->head, "ifndef") == 0) {
        ifndef_directive(pp);
      } else if(directive->type == PP_IDENT && strcmp(directive->text->head, "include") == 0) {
        include_directive(pp);
      } else if(directive->type == PP_IDENT && strcmp(directive->text->head, "define") == 0) {
        define_directive(pp);
      } else if(directive->type == PP_IDENT && strcmp(directive->text->head, "undef") == 0) {
        undef_directive(pp);
      } else if(directive->type == PP_IDENT && strcmp(directive->text->head, "line") == 0) {
        // #line directive
        skip_line(pp);
      } else if(directive->type == PP_IDENT && strcmp(directive->text->head, "error") == 0) {
        // #error directive
        skip_line(pp);
        printf("#error\n");
        exit(1);
      } else if(directive->type == PP_IDENT && strcmp(directive->text->head, "pragma") == 0) {
        // #pragma directive
        skip_line(pp);
      } else if(directive->type == PP_NEW_LINE) {
        // #null directive
        skip_line(pp);
      } else {
        fprintf(stderr, "invalid preprocessor directive \"%s\".\n", directive->text->head);
        skip_line(pp);
      }
    } else {
      text_line(pp);
    }
  }

  return 0;
}

struct pp_list *preprocessing_file(struct preprocessor *pp) {
  group(pp);
  return pp->list;
}
