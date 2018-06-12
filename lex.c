#include <stdlib.h>
#include <string.h>
#include "lex.h"

struct pp_token_lexer *allocate_pp_token_lexer(const unsigned char *file);
void free_pp_token_lexer(struct pp_token_lexer *lexer);
struct pp_token *allocate_pp_token();
void free_pp_token(struct pp_token *token);
struct utf8c remove_comment(struct pp_token_lexer *lexer);
void push_char_queue(struct pp_token_lexer *lexer, struct utf8c uc);
struct utf8c pop_char_queue(struct pp_token_lexer *lexer);
int one_of(char c, char *s);
int non_digit(unsigned char c);
int digit(unsigned char c);
int hex_digit(unsigned char c);
int octal_digit(unsigned char c);
int ident_allowed_code(int code);
int ident_disallowed_init_code(int code);
struct pp_token *next_pp_token(struct pp_token_lexer *lexer);

const unsigned char pp_token_name[][32] = {
  "header-name", "identifier", "preprocessing-number", "character-constant", "string-literal",
  "left-bracket", "right-bracket", "left-paren", "right-paren", "left-brace", "right-brace",
  "dot", "elipsis", "comma", "plus", "increment", "plus-assign", "minus", "decrement",
  "minus-assign", "arrow", "mul", "mul-assign", "div", "div-assign", "mod", "mod-assign",
  "digraph-right-brace", "digraph-sharp", "digraph-concat", "and", "logical-and",
  "and-assign", "or", "logical-or", "or-assign", "xor", "xor-assign", "assign", "equal",
  "not", "not-equal", "less-than", "left-shift", "left-shift-assign", "less-than-equal",
  "digraph-left-bracket", "digraph-left-brace", "greater-than", "right-shift",
  "right-shift-assign", "greater-than-equal", "tilde", "question", "colon",
  "digraph-right-bracket", "semicolon", "sharp", "concat", "new-line", "space",
  "other", "none", "place marker"
};

struct pp_token_lexer *allocate_pp_token_lexer(const unsigned char *file) {
  const int INIT_SIZE = 64;

  struct pp_token_lexer *lexer = (struct pp_token_lexer *) malloc(sizeof(struct pp_token_lexer));
  if(lexer == NULL) {
    perror("malloc");
    exit(1);
  }

  lexer->src = allocate_source(file);
  lexer->comment_queue_size = 0;
  lexer->queue = (struct utf8c *) malloc(sizeof(struct utf8c) * INIT_SIZE);
  lexer->queue_head = 0;
  lexer->queue_size = 0;
  lexer->queue_allocate_size = INIT_SIZE;
  lexer->context = CTX_NL;

  while(1) {
    struct utf8c uc = remove_comment(lexer);
    unsigned char c = uc.sequence[0];
    if(!(c == ' ' || c == '\t' || c == '\v' || c == '\f' || c == '\n')) {
      push_char_queue(lexer, uc);
      break;
    }
  }

  return lexer;
};

struct pp_token *allocate_pp_token() {
  struct pp_token *token = (struct pp_token *) malloc(sizeof(struct pp_token));
  if(token == NULL) {
    perror("malloc");
    exit(1);
  }
  token->text = allocate_string();
  return token;
}

void free_pp_token(struct pp_token *token) {
  free_string(token->text);
  free(token);
}

void free_pp_token_lexer(struct pp_token_lexer *lexer) {
  free_source(lexer->src);
  free(lexer->queue);
  free(lexer);
}

struct utf8c remove_comment(struct pp_token_lexer *lexer) {
  for(; lexer->comment_queue_size < 2; lexer->comment_queue_size++) {
    lexer->comment_queue[lexer->comment_queue_size] = next_source_char(lexer->src);
    if(lexer->comment_queue[lexer->comment_queue_size].bytes == 0) {
      break;
    }
  }

  if(lexer->comment_queue_size == 0) {
    return ueof;
  }

  if(lexer->comment_queue_size == 2) {
    if(lexer->comment_queue[0].sequence[0] == '/') {
      if(lexer->comment_queue[1].sequence[0] == '/') {
        while(1) {
          struct utf8c uc = next_source_char(lexer->src);
          if(uc.bytes == 0) {
            error("reached end of file while removing \"//...\" comment.\n");
          }
          if(uc.sequence[0] == '\n') {
            break;
          }
        }
        struct utf8c uc = single_byte_char('\n');
        lexer->comment_queue_size = 0;
        return uc;
      } else if(lexer->comment_queue[1].sequence[0] == '*') {
        struct utf8c last = next_source_char(lexer->src);
        while(1) {
          struct utf8c uc = next_source_char(lexer->src);
          if(uc.bytes == 0) {
            error("reached end of file while removing \"/* ... */\" comment.\n");
          }
          if(last.sequence[0] == '*' && uc.sequence[0] == '/') {
            break;
          }
          last = uc;
        }
        struct utf8c uc = single_byte_char(' ');
        lexer->comment_queue_size = 0;
        return uc;
      }
    }
  }

  struct utf8c uc = lexer->comment_queue[0];
  lexer->comment_queue_size--;
  for(int i = 0; i < lexer->comment_queue_size; i++) {
    lexer->comment_queue[i] = lexer->comment_queue[i + 1];
  }
  return uc;
}

void push_char_queue(struct pp_token_lexer *lexer, struct utf8c uc) {
  if(lexer->queue_size >= lexer->queue_allocate_size) {
    struct utf8c *t = (struct utf8c *) malloc(sizeof(struct utf8c) * lexer->queue_allocate_size * 2);
    if(t == NULL) {
      perror("malloc");
      exit(1);
    }
    for(int i = 0; i < lexer->queue_size; i++) {
      t[i] = lexer->queue[(lexer->queue_head + i) % lexer->queue_allocate_size];
    }
    lexer->queue = t;
    lexer->queue_head = 0;
    lexer->queue_allocate_size *= 2;
  }
  lexer->queue[(lexer->queue_head + lexer->queue_size) % lexer->queue_allocate_size] = uc;
  lexer->queue_size++;
}

struct utf8c pop_char_queue(struct pp_token_lexer *lexer) {
  if(lexer->queue_size == 0) {
    error("internal: lexer character queue is empty\n");
  }

  struct utf8c uc = lexer->queue[lexer->queue_head];
  lexer->queue_head++;
  lexer->queue_head %= lexer->queue_allocate_size;
  lexer->queue_size--;
  return uc;
}

int one_of(char c, char *s) {
  for(int i = 0; s[i] != '\0'; i++) {
    if(c == s[i]) {
      return 1;
    }
  }
  return 0;
}

int non_digit(unsigned char c) {
  if('A' <= c && c <= 'Z') return 1;
  if('a' <= c && c <= 'z') return 1;
  if(c == '_') return 1;
  return 0;
}

int digit(unsigned char c) {
  return '0' <= c && c <= '9';
  return 0;
}

int hex_digit(unsigned char c) {
  if(digit(c)) return 1;
  if('A' <= c && c <= 'F') return 1;
  if('a' <= c && c <= 'f') return 1;
  return 0;
}

int octal_digit(unsigned char c) {
  return '0' <= c && c <= '7';
}

int ident_allowed_code(int code) {
  if(code == 0xA8) return 1;
  if(code == 0xAA) return 1;
  if(code == 0xAD) return 1;
  if(code == 0xAF) return 1;
  if(0xB2 <= code && code <= 0xB5) return 1;
  if(0xB7 <= code && code <= 0xBA) return 1;
  if(0xBC <= code && code <= 0xBE) return 1;
  if(0xC0 <= code && code <= 0xD6) return 1;
  if(0xD8 <= code && code <= 0xF6) return 1;
  if(0xF8 <= code && code <= 0xFF) return 1;
  if(0x100 <= code && code <= 0x167F) return 1;
  if(0x1681 <= code && code <= 0x180D) return 1;
  if(0x180F <= code && code <= 0x1FFF) return 1;
  if(0x200B <= code && code <= 0x200D) return 1;
  if(0x202A <= code && code <= 0x202E) return 1;
  if(0x203F <= code && code <= 0x2040) return 1;
  if(code == 2054) return 1;
  if(0x2060 <= code && code <= 0x206F) return 1;
  if(0x2070 <= code && code <= 0x218F) return 1;
  if(0x2460 <= code && code <= 0x24FF) return 1;
  if(0x2776 <= code && code <= 0x2793) return 1;
  if(0x2C00 <= code && code <= 0x2DFF) return 1;
  if(0x2E80 <= code && code <= 0x2FFF) return 1;
  if(0x3004 <= code && code <= 0x3007) return 1;
  if(0x3021 <= code && code <= 0x302F) return 1;
  if(0x3031 <= code && code <= 0x303F) return 1;
  if(0x3040 <= code && code <= 0xD7FF) return 1;
  if(0xF900 <= code && code <= 0xFD3D) return 1;
  if(0xFD40 <= code && code <= 0xFDCF) return 1;
  if(0xFDF0 <= code && code <= 0xFE44) return 1;
  if(0xFE47 <= code && code <= 0xFFFD) return 1;
  if(0x10000 <= code && code <= 0x1FFFD) return 1;
  if(0x20000 <= code && code <= 0x2FFFD) return 1;
  if(0x30000 <= code && code <= 0x3FFFD) return 1;
  if(0x40000 <= code && code <= 0x4FFFD) return 1;
  if(0x50000 <= code && code <= 0x5FFFD) return 1;
  if(0x60000 <= code && code <= 0x6FFFD) return 1;
  if(0x70000 <= code && code <= 0x7FFFD) return 1;
  if(0x80000 <= code && code <= 0x8FFFD) return 1;
  if(0x90000 <= code && code <= 0x9FFFD) return 1;
  if(0xA0000 <= code && code <= 0xAFFFD) return 1;
  if(0xB0000 <= code && code <= 0xBFFFD) return 1;
  if(0xC0000 <= code && code <= 0xCFFFD) return 1;
  if(0xD0000 <= code && code <= 0xDFFFD) return 1;
  if(0xE0000 <= code && code <= 0xEFFFD) return 1;
  return 0;
}

int ident_disallowed_init_code(int code) {
  if(0x300 <= code && code <= 0x36F) return 1;
  if(0x1DC0 <= code && code <= 0x1DFF) return 1;
  if(0x20D0 <= code && code <= 0x20FF) return 1;
  if(0xFE20 <= code && code <= 0xFE2F) return 1;
  return 0;
}

struct pp_token *next_pp_token(struct pp_token_lexer *lexer) {
  enum pp_token_lexer_state state = ST_START;
  enum pp_token_type type;
  int count = 0;

  for(int i = 0;; i++) {
    if(i >= lexer->queue_size) {
      push_char_queue(lexer, remove_comment(lexer));
    }

    struct utf8c uc = lexer->queue[(lexer->queue_head + i) % lexer->queue_allocate_size];
    unsigned char c = uc.sequence[0];

    if(state == ST_START) {
      if(c == 'L') {
        state = ST_IDENT_UL;
      } else if(c == 'U') {
        state = ST_IDENT_UU;
      } else if(c == 'u') {
        state = ST_IDENT_LU;
      } else if(c == '\\') {
        state = ST_IDENT_ESC;
      } else if(non_digit(c)) {
        state = ST_IDENT;
      } else if(digit(c)) {
        state = ST_NUM;
      } else if(c == '\'') {
        state = ST_CHAR;
      } else if(c == '"') {
        state = ST_STR;
      } else if(c == '[') {
        state = ST_LBRACKET;
      } else if(c == ']') {
        state = ST_RBRACKET;
      } else if(c == '(') {
        state = ST_LPAREN;
      } else if(c == ')') {
        state = ST_RPAREN;
      } else if(c == '{') {
        state = ST_LBRACE;
      } else if(c == '}') {
        state = ST_RBRACE;
      } else if(c == '.') {
        state = ST_DOT;
      } else if(c == ',') {
        state = ST_COMMA;
      } else if(c == '+') {
        state = ST_PLUS;
      } else if(c == '-') {
        state = ST_MINUS;
      } else if(c == '*') {
        state = ST_MUL;
      } else if(c == '/') {
        state = ST_DIV;
      } else if(c == '%') {
        state = ST_MOD;
      } else if(c == '&') {
        state = ST_AND;
      } else if(c == '|') {
        state = ST_OR;
      } else if(c == '^') {
        state = ST_XOR;
      } else if(c == '=') {
        state = ST_ASGN;
      } else if(c == '!') {
        state = ST_NOT;
      } else if(c == '<') {
        state = ST_LT;
      } else if(c == '>') {
        state = ST_GT;
      } else if(c == '~') {
        state = ST_TILDE;
      } else if(c == '?') {
        state = ST_QUESTION;
      } else if(c == ':') {
        state = ST_COLON;
      } else if(c == ';') {
        state = ST_SEMICOLON;
      } else if(c == '#') {
        state = ST_SHARP;
      } else if(c == '\n') {
        state = ST_NEW_LINE;
      } else if(c == ' ' || c == '\t' || c == '\v' || c == '\f') {
        state = ST_SPACE;
      } else {
        break;
      }
    }
    else if(state == ST_H_CHAR) {
      if(c == '>') {
        state = ST_H_NAME;
      } else if(c == '\n') {
        break;
      } else {
        state = ST_H_CHAR;
      }
    }
    else if(state == ST_Q_CHAR) {
      if(c == '\"') {
        state = ST_H_NAME;
      } else if(c == '\n') {
        break;
      } else {
        state = ST_Q_CHAR;
      }
    }
    else if(state == ST_H_NAME) {
      type = PP_H_NAME;
      count = i;
      break;
    }
    else if(state == ST_IDENT_UL) {
      type = PP_IDENT;
      count = i;
      if(non_digit(c) || digit(c)) {
        state = ST_IDENT;
      } else if(c == '\\') {
        state = ST_IDENT_ESC;
      } else if(c == '\'') {
        state = ST_CHAR;
      } else if(c == '"') {
        state = ST_STR;
      } else {
        break;
      }
    }
    else if(state == ST_IDENT_UU) {
      type = PP_IDENT;
      count = i;
      if(non_digit(c) || digit(c)) {
        state = ST_IDENT;
      } else if(c == '\\') {
        state = ST_IDENT_ESC;
      } else if(c == '\'') {
        state = ST_CHAR;
      } else if(c == '"') {
        state = ST_STR;
      } else {
        break;
      }
    }
    else if(state == ST_IDENT_LU) {
      type = PP_IDENT;
      count = i;
      if(c == '8') {
        ST_IDENT_LU8;
      } else if(non_digit(c) || digit(c)) {
        state = ST_IDENT;
      } else if(c == '\\') {
        state = ST_IDENT_ESC;
      } else if(c == '\'') {
        state = ST_CHAR;
      } else if(c == '"') {
        state = ST_STR;
      } else {
        break;
      }
    }
    else if(state == ST_IDENT_LU8) {
      type = PP_IDENT;
      count = i;
      if(non_digit(c) || digit(c)) {
        state = ST_IDENT;
      } else if(c == '\\') {
        state = ST_IDENT_ESC;
      } else if(c == '"') {
        state = ST_STR;
      } else {
        break;
      }
    }
    else if(state == ST_IDENT_ESC) {
      if(c == 'u') {
        state = ST_IDENT_LUNIV;
      } else if(c == 'U') {
        state = ST_IDENT_UUNIV;
      } else {
        break;
      }
    }
    else if(state == ST_IDENT_LUNIV) {
      if(hex_digit(c)) {
        state = ST_IDENT_LUNIV_1;
      } else {
        break;
      }
    }
    else if(state == ST_IDENT_LUNIV_1) {
      if(hex_digit(c)) {
        state = ST_IDENT_LUNIV_2;
      } else {
        break;
      }
    }
    else if(state == ST_IDENT_LUNIV_2) {
      if(hex_digit(c)) {
        state = ST_IDENT_LUNIV_3;
      } else {
        break;
      }
    }
    else if(state == ST_IDENT_LUNIV_3) {
      if(hex_digit(c)) {
        state = ST_IDENT;
      } else {
        break;
      }
    }
    else if(state == ST_IDENT_UUNIV) {
      if(hex_digit(c)) {
        state = ST_IDENT_UUNIV_1;
      } else {
        break;
      }
    }
    else if(state == ST_IDENT_UUNIV_1) {
      if(hex_digit(c)) {
        state = ST_IDENT_UUNIV_2;
      } else {
        break;
      }
    }
    else if(state == ST_IDENT_UUNIV_2) {
      if(hex_digit(c)) {
        state = ST_IDENT_UUNIV_3;
      } else {
        break;
      }
    }
    else if(state == ST_IDENT_UUNIV_3) {
      if(hex_digit(c)) {
        state = ST_IDENT_UUNIV_4;
      } else {
        break;
      }
    }
    else if(state == ST_IDENT_UUNIV_4) {
      if(hex_digit(c)) {
        state = ST_IDENT_LUNIV_1;
      } else {
        break;
      }
    }
    else if(state == ST_IDENT) {
      type = PP_IDENT;
      count = i;
      if(non_digit(c) || digit(c)) {
        state = ST_IDENT;
      } else if(c == '\\') {
        state = ST_IDENT_ESC;
      } else {
        break;
      }
    }
    else if(state == ST_NUM) {
      type = PP_NUM;
      count = i;
      if(c == 'e' || c == 'E' || c == 'p' || c == 'P') {
        state = ST_NUM_SIGN;
      } else if(c == '.' || digit(c) || non_digit(c)) {
        state = ST_NUM;
      } else if(c == '\\') {
        state = ST_NUM_ESC;
      } else {
        break;
      }
    }
    else if(state == ST_NUM_SIGN) {
      type = PP_NUM;
      count = i;
      if(c == '+' || c == '-' || c == '.' || digit(c) || non_digit(c)) {
        state = ST_NUM;
      } else if(c == '\\') {
        state = ST_NUM_ESC;
      } else {
        break;
      }
    }
    else if(state == ST_NUM_ESC) {
      if(c == 'u') {
        state = ST_NUM_LUNIV;
      } else if(c == 'U') {
        state = ST_NUM_UUNIV;
      } else {
        break;
      }
    }
    else if(state == ST_NUM_LUNIV) {
      if(hex_digit(c)) {
        state = ST_NUM_LUNIV_1;
      } else {
        break;
      }
    }
    else if(state == ST_NUM_LUNIV_1) {
      if(hex_digit(c)) {
        state = ST_NUM_LUNIV_2;
      } else {
        break;
      }
    }
    else if(state == ST_NUM_LUNIV_2) {
      if(hex_digit(c)) {
        state = ST_NUM_LUNIV_3;
      } else {
        break;
      }
    }
    else if(state == ST_NUM_LUNIV_3) {
      if(hex_digit(c)) {
        state = ST_NUM;
      } else {
        break;
      }
    }
    else if(state == ST_NUM_UUNIV) {
      if(hex_digit(c)) {
        state = ST_NUM_UUNIV_1;
      } else {
        break;
      }
    }
    else if(state == ST_NUM_UUNIV_1) {
      if(hex_digit(c)) {
        state = ST_NUM_UUNIV_2;
      } else {
        break;
      }
    }
    else if(state == ST_NUM_UUNIV_2) {
      if(hex_digit(c)) {
        state = ST_NUM_UUNIV_3;
      } else {
        break;
      }
    }
    else if(state == ST_NUM_UUNIV_3) {
      if(hex_digit(c)) {
        state = ST_NUM;
      } else {
        break;
      }
    }
    else if(state == ST_NUM_UUNIV_4) {
      if(hex_digit(c)) {
        state = ST_NUM_LUNIV_1;
      } else {
        break;
      }
    }
    else if(state == ST_CHAR) {
      if(c == '\\') {
        state = ST_C_ESC;
      } else if(c == '\'' || c == '\n') {
        break;
      } else {
        state = ST_C_MEMBER;
      }
    }
    else if(state == ST_C_ESC) {
      if(one_of(c, "'\"?\\abfnrtv")) {
        state = ST_C_MEMBER;
      } else if(c == 'u') {
        state = ST_C_LUNIV;
      } else if(c == 'U') {
        state = ST_C_UUNIV;
      } else if(octal_digit(c)) {
        state = ST_C_OCT_1;
      } else if(c == 'x') {
        state = ST_C_HEX;
      } else {
        break;
      }
    }
    else if(state == ST_C_LUNIV) {
      if(hex_digit(c)) {
        state = ST_C_LUNIV_1;
      } else {
        break;
      }
    }
    else if(state == ST_C_LUNIV_1) {
      if(hex_digit(c)) {
        state = ST_C_LUNIV_2;
      } else {
        break;
      }
    } else if(state == ST_C_LUNIV_2) {
      if(hex_digit(c)) {
        state = ST_C_LUNIV_3;
      } else {
        break;
      }
    }
    else if(state == ST_C_LUNIV_3) {
      if(hex_digit(c)) {
        state = ST_C_MEMBER;
      } else {
        break;
      }
    }
    else if(state == ST_C_UUNIV) {
      if(hex_digit(c)) {
        state = ST_C_UUNIV_1;
      } else {
        break;
      }
    }
    else if(state == ST_C_UUNIV_1) {
      if(hex_digit(c)) {
        state = ST_C_UUNIV_2;
      } else {
        break;
      }
    }
    else if(state == ST_C_UUNIV_2) {
      if(hex_digit(c)) {
        state = ST_C_UUNIV_3;
      } else {
        break;
      }
    }
    else if(state == ST_C_UUNIV_3) {
      if(hex_digit(c)) {
        state = ST_C_UUNIV_4;
      } else {
        break;
      }
    }
    else if(state == ST_C_UUNIV_4) {
      if(hex_digit(c)) {
        state = ST_C_LUNIV_1;
      } else {
        break;
      }
    }
    else if(state == ST_C_OCT_1) {
      if(octal_digit(c)) {
        state = ST_C_OCT_2;
      } else if(c == '\'') {
        state = ST_CHAR_CNST;
      } else {
        break;
      }
    }
    else if(state == ST_C_OCT_2) {
      if(octal_digit(c)) {
        state = ST_C_MEMBER;
      } else if(c == '\'') {
        state = ST_CHAR_CNST;
      }
    }
    else if(state == ST_C_HEX) {
      if(hex_digit(c)) {
        state = ST_C_HEX_R;
      } else {
        break;
      }
    }
    else if(state == ST_C_HEX_R) {
      if(hex_digit(c)) {
        state = ST_C_HEX_R;
      } else if(c == '\'') {
        state = ST_CHAR_CNST;
      } else {
        break;
      }
    }
    else if(state == ST_C_MEMBER) {
      if(c == '\'') {
        state = ST_CHAR_CNST;
      } else {
        break;
      }
    }
    else if(state == ST_CHAR_CNST) {
      type = PP_CHAR;
      count = i;
      break;
    }
    else if(state == ST_STR) {
      if(lexer->context == CTX_INCLUDE && c != '"' && c != '\n') {
        state = ST_Q_CHAR;
      } else if(c == '\\') {
        state = ST_S_ESC;
      } else if(c == '"') {
        state = ST_STR_LTRL;
      } else if(c == '\n') {
        break;
      } else {
        state = ST_STR;
      }
    }
    else if(state == ST_S_ESC) {
      if(one_of(c, "'\"?\\abfnrtv")) {
        state = ST_STR;
      } else if(c == 'u') {
        state = ST_S_LUNIV;
      } else if(c == 'U') {
        state = ST_S_UUNIV;
      } else if(octal_digit(c)) {
        state = ST_S_OCT_1;
      } else if(c == 'x') {
        state = ST_S_HEX;
      } else {
        break;
      }
    }
    else if(state == ST_S_LUNIV) {
      if(hex_digit(c)) {
        state = ST_S_LUNIV_1;
      } else {
        break;
      }
    }
    else if(state == ST_S_LUNIV_1) {
      if(hex_digit(c)) {
        state = ST_S_LUNIV_2;
      } else {
        break;
      }
    }
    else if(state == ST_S_LUNIV_2) {
      if(hex_digit(c)) {
        state = ST_S_LUNIV_3;
      } else {
        break;
      }
    }
    else if(state == ST_S_LUNIV_3) {
      if(hex_digit(c)) {
        state = ST_STR;
      } else {
        break;
      }
    }
    else if(state == ST_S_UUNIV) {
      if(hex_digit(c)) {
        state = ST_S_UUNIV_1;
      } else {
        break;
      }
    }
    else if(state == ST_S_UUNIV_1) {
      if(hex_digit(c)) {
        state = ST_S_UUNIV_2;
      } else {
        break;
      }
    }
    else if(state == ST_S_UUNIV_2) {
      if(hex_digit(c)) {
        state = ST_S_UUNIV_3;
      } else {
        break;
      }
    }
    else if(state == ST_S_UUNIV_3) {
      if(hex_digit(c)) {
        state = ST_S_UUNIV_4;
      } else {
        break;
      }
    }
    else if(state == ST_S_UUNIV_4) {
      if(hex_digit(c)) {
        state = ST_S_LUNIV_1;
      } else {
        break;
      }
    }
    else if(state == ST_S_OCT_1) {
      if(octal_digit(c)) {
        state = ST_S_OCT_2;
      } else if(c == '"') {
        state = ST_STR_LTRL;
      } else if(c == '\\') {
        state = ST_S_ESC;
      } else if(c == '\n') {
        break;
      } else {
        state = ST_STR;
      }
    }
    else if(state == ST_S_OCT_2) {
      if(c == '"') {
        state = ST_STR_LTRL;
      } else if(c == '\\') {
        state = ST_S_ESC;
      } else if(c == '\n') {
        break;
      } else {
        state = ST_STR;
      }
    }
    else if(state == ST_S_HEX) {
      if(hex_digit(c)) {
        state = ST_S_HEX_R;
      } else {
        break;
      }
    }
    else if(state == ST_S_HEX_R) {
      if(digit(c)) {
        state = ST_S_HEX_R;
      } else if(c == '"') {
        state = ST_STR_LTRL;
      } else if(c == '\\') {
        state = ST_S_ESC;
      } else if(c == '\n') {
        break;
      } else {
        state = ST_STR;
      }
    }
    else if(state == ST_STR_LTRL) {
      type = PP_STR;
      count = i;
      break;
    }
    else if(state == ST_LBRACKET) {
      type = PP_LBRACKET;
      count = i;
      break;
    }
    else if(state == ST_RBRACKET) {
      type = PP_RBRACKET;
      count = i;
      break;
    }
    else if(state == ST_LPAREN) {
      type = PP_LPAREN;
      count = i;
      break;
    }
    else if(state == ST_RPAREN) {
      type = PP_RPAREN;
      count = i;
      break;
    }
    else if(state == ST_LBRACE) {
      type = PP_LBRACE;
      count = i;
      break;
    }
    else if(state == ST_RBRACE) {
      type = PP_RBRACE;
      count = i;
      break;
    }
    else if(state == ST_DOT) {
      type = PP_DOT;
      count = i;
      if(c == '.') {
        state = ST_DOT_2;
      } else if(digit(c)) {
        state = ST_NUM;
      } else {
        break;
      }
    }
    else if(state == ST_DOT_2) {
      if(c == '.') {
        state = ST_ELLIPSIS;
      } else {
        break;
      }
    }
    else if(state == ST_ELLIPSIS) {
      type = PP_ELLIPSIS;
      count = i;
      break;
    }
    else if(state == ST_COMMA) {
      type = PP_COMMA;
      count = i;
      break;
    }
    else if(state == ST_PLUS) {
      type = PP_PLUS;
      count = i;
      if(c == '+') {
        state = ST_INC;
      } else if(c == '=') {
        state = ST_PLUS_ASGN;
      } else {
        break;
      }
    }
    else if(state == ST_INC) {
      type = PP_INC;
      count = i;
      break;
    }
    else if(state == ST_PLUS_ASGN) {
      type = PP_PLUS_ASGN;
      count = i;
      break;
    }
    else if(state == ST_MINUS) {
      type = PP_MINUS;
      count = i;
      if(c == '-') {
        state = ST_DEC;
      } else if(c == '=') {
        state = ST_MINUS_ASGN;
      } else if(c == '>') {
        state = ST_ARROW;
      } else {
        break;
      }
    }
    else if(state == ST_DEC) {
      type = PP_DEC;
      count = i;
      break;
    }
    else if(state == ST_MINUS_ASGN) {
      type = PP_MINUS_ASGN;
      count = i;
      break;
    }
    else if(state == ST_ARROW) {
      type = PP_ARROW;
      count = i;
      break;
    }
    else if(state == ST_MUL) {
      type = PP_MUL;
      count = i;
      if(c == '=') {
        state = ST_MUL_ASGN;
      } else {
        break;
      }
    }
    else if(state == ST_MUL_ASGN) {
      type = PP_MUL_ASGN;
      count = i;
      break;
    }
    else if(state == ST_DIV) {
      type = PP_DIV;
      count = i;
      if(c == '=') {
        state = ST_DIV_ASGN;
      } else {
        break;
      }
    }
    else if(state == ST_DIV_ASGN) {
      type = PP_DIV_ASGN;
      count = i;
      break;
    }
    else if(state == ST_MOD) {
      type = PP_MOD;
      count = i;
      if(c == '=') {
        state = ST_MOD_ASGN;
      } else if(c == '>') {
        state = ST_DIG_RBRACE;
      } else if(c == ':') {
        state = ST_DIG_SHARP;
      } else {
        break;
      }
    }
    else if(state == ST_MOD_ASGN) {
      type = PP_MOD_ASGN;
      count = i;
      break;
    }
    else if(state == ST_DIG_RBRACE) {
      type = PP_DIG_RBRACE;
      count = i;
      break;
    }
    else if(state == ST_DIG_SHARP) {
      type = PP_DIG_SHARP;
      count = i;
      if(c == '%') {
        state = ST_DIG_SHARP_MOD;
      } else {
        break;
      }
    }
    else if(state == ST_DIG_SHARP_MOD) {
      if(c == ':') {
        state = ST_DIG_CONCAT;
      } else {
        break;
      }
    }
    else if(state == ST_DIG_CONCAT) {
      type = PP_DIG_CONCAT;
      count = i;
      break;
    }
    else if(state == ST_AND) {
      type = PP_AND;
      count = i;
      if(c == '&') {
        state = ST_LAND;
      } else if(c == '=') {
        state = ST_AND_ASGN;
      } else {
        break;
      }
    }
    else if(state == ST_LAND) {
      type = PP_LAND;
      count = i;
      break;
    }
    else if(state == ST_AND_ASGN) {
      type = PP_AND_ASGN;
      count = i;
      break;
    }
    else if(state == ST_OR) {
      type = PP_OR;
      count = i;
      if(c == '|') {
        state = ST_LOR;
      } else if(c == '=') {
        state = ST_OR_ASGN;
      } else {
        break;
      }
    }
    else if(state == ST_LOR) {
      type = PP_LOR;
      count = i;
      break;
    }
    else if(state == ST_OR_ASGN) {
      type = PP_OR_ASGN;
      count = i;
      break;
    }
    else if(state == ST_XOR) {
      type = PP_XOR;
      count = i;
      if(c == '=') {
        state = ST_XOR_ASGN;
      } else {
        break;
      }
    }
    else if(state == ST_XOR_ASGN) {
      type = PP_XOR_ASGN;
      count = i;
      break;
    }
    else if(state == ST_ASGN) {
      type = PP_ASGN;
      count = i;
      if(c == '=') {
        state = ST_EQ;
      } else {
        break;
      }
    }
    else if(state == ST_EQ) {
      type = PP_EQ;
      count = i;
      break;
    }
    else if(state == ST_NOT) {
      type = PP_NOT;
      count = i;
      if(c == '=') {
        state = ST_NEQ;
      } else {
        break;
      }
    }
    else if(state == ST_NEQ) {
      type = PP_NEQ;
      count = i;
      break;
    }
    else if(state == ST_LT) {
      type = PP_LT;
      count = i;
      if(lexer->context == CTX_INCLUDE && c != '>' && c != '\n') {
        state = ST_H_CHAR;
      } else if(c == '<') {
        state = ST_LSHIFT;
      } else if(c == '=') {
        state = ST_LTE;
      } else if(c == ':') {
        state = ST_DIG_LBRACKET;
      } else if(c == '%') {
        state = ST_DIG_LBRACE;
      } else {
        break;
      }
    }
    else if(state == ST_LSHIFT) {
      type = PP_LSHIFT;
      count = i;
      if(c == '=') {
        state = ST_LSHIFT_ASGN;
      } else {
        break;
      }
    }
    else if(state == ST_LSHIFT_ASGN) {
      type = PP_LSHIFT_ASGN;
      count = i;
      break;
    }
    else if(state == ST_LTE) {
      type = PP_LTE;
      count = i;
      break;
    }
    else if(state == ST_DIG_LBRACKET) {
      type = PP_DIG_LBRACKET;
      count = i;
      break;
    }
    else if(state == ST_DIG_LBRACE) {
      type = PP_DIG_LBRACE;
      count = i;
      break;
    }
    else if(state == ST_GT) {
      type = PP_GT;
      count = i;
      if(c == '>') {
        state = ST_RSHIFT;
      } else if(c == '=') {
        state = ST_GTE;
      } else {
        break;
      }
    }
    else if(state == ST_RSHIFT) {
      type = PP_RSHIFT;
      count = i;
      if(c == '=') {
        state = ST_RSHIFT_ASGN;
      } else {
        break;
      }
    }
    else if(state == ST_RSHIFT_ASGN) {
      type = PP_RSHIFT_ASGN;
      count = i;
      break;
    }
    else if(state == ST_GTE) {
      type = PP_GTE;
      count = i;
      break;
    }
    else if(state == ST_TILDE) {
      type = PP_TILDE;
      count = i;
      break;
    }
    else if(state == ST_QUESTION) {
      type = PP_QUESTION;
      count = i;
      break;
    }
    else if(state == ST_COLON) {
      type = PP_COLON;
      count = i;
      if(c == '<') {
        state = ST_DIG_RBRACKET;
      } else {
        break;
      }
    }
    else if(state == ST_DIG_RBRACKET) {
      type = PP_DIG_RBRACKET;
      count = i;
      break;
    }
    else if(state == ST_SEMICOLON) {
      type = PP_SEMICOLON;
      count = i;
      break;
    }
    else if(state == ST_SHARP) {
      type = PP_SHARP;
      count = i;
      if(c == '#') {
        state = ST_CONCAT;
      } else {
        break;
      }
    }
    else if(state == ST_CONCAT) {
      type = PP_CONCAT;
      count = i;
      break;
    }
    else if(state == ST_NEW_LINE) {
      type = PP_NEW_LINE;
      count = i;
      if(c == ' ' || c == '\t' || c == '\v' || c == '\f' || c == '\n') {
        state = ST_NEW_LINE;
      } else {
        break;
      }
    }
    else if(state == ST_SPACE) {
      type = PP_SPACE;
      count = i;
      if(c == ' ' || c == '\t' || c == '\v' || c == '\f') {
        state = ST_SPACE;
      } else if(c == '\n') {
        state = ST_NEW_LINE;
      } else {
        break;
      }
    }
  }

  struct pp_token *token = allocate_pp_token();
  if(count == 0) {
    struct utf8c uc = pop_char_queue(lexer);
    if(uc.bytes == 0) {
      token->type = PP_NONE;
    } else {
      token->type = PP_OTHER;
      for(int i = 0; i < uc.bytes; i++) {
        append_string(token->text, uc.sequence[i]);
      }
    }
  } else if(type == PP_NEW_LINE) {
    token->type = PP_NEW_LINE;
    append_string(token->text, '\n');
    for(int i = 0; i < count; i++) {
      pop_char_queue(lexer);
    }
  } else if(type == PP_SPACE) {
    token->type = PP_SPACE;
    append_string(token->text, ' ');
    for(int i = 0; i < count; i++) {
      pop_char_queue(lexer);
    }
  } else if(type == PP_IDENT) {
    token->type = PP_IDENT;
    for(int i = 0; i < count; i++) {
      struct utf8c uc = pop_char_queue(lexer);
      if(uc.sequence[0] == '\\') {
        struct utf8c univ = pop_char_queue(lexer);
        for(int j = 0; j < uc.bytes; j++) {
          append_string(token->text, uc.sequence[j]);
        }
        int n;
        if(univ.sequence[0] == 'u') {
          n = 4;
        } else if(univ.sequence[0] == 'U') {
          n = 8;
        }
        i += n + 1;
        append_string(token->text, 'U');
        if(univ.sequence[0] == 'u') {
          for(int j = 0; j < 4; j++) {
            append_string(token->text, '0');
          }
        }
        int code = 0;
        for(int j = 0; j < n; j++) {
          unsigned char hex = pop_char_queue(lexer).sequence[0];
          if('0' <= hex && hex <= '9') {
            code = code * 16 + (hex - '0');
          } else if('a' <= hex && hex <= 'f') {
            code = code * 16 + (hex - 'a' + 10);
          } else if('A' <= hex && hex <= 'F') {
            code = code * 16 + (hex - 'A' + 10);
          }
          append_string(token->text, hex);
        }
        struct utf8c dc = code_point(code);
        if(i == 0 && ident_disallowed_init_code(code)) {
          error("'%s' is not allowed for initial character of identifier\n", dc.sequence);
        }
        if(!ident_allowed_code(code)) {
          error("'%s' is not allowed for identifier\n", dc.sequence);
        }
      } else {
        for(int j = 0; j < uc.bytes; j++) {
          append_string(token->text, uc.sequence[j]);
        }
      }
    }
  } else {
    token->type = type;
    for(int i = 0; i < count; i++) {
      struct utf8c uc = pop_char_queue(lexer);
      for(int j = 0; j < uc.bytes; j++) {
        append_string(token->text, uc.sequence[j]);
      }
    }
  }
  token->name = pp_token_name[token->type];
  token->concat = 0;

  if(lexer->context == CTX_NL) {
    if(token->type == PP_SHARP) {
      lexer->context = CTX_SHARP;
    } else if(token->type != PP_SPACE) {
      lexer->context = CTX_NORMAL;
    }
  } else if(lexer->context == CTX_SHARP) {
    if(token->type == PP_IDENT && strcmp(token->text->head, "include") == 0) {
      lexer->context = CTX_INCLUDE;
    } else if(token->type != PP_SPACE) {
      lexer->context = CTX_NORMAL;
    }
  } else if(lexer->context == CTX_INCLUDE) {
    if(token->type != PP_SPACE) {
      lexer->context = CTX_NORMAL;
    }
  } else if(lexer->context == CTX_NORMAL) {
    if(token->type == PP_NEW_LINE) {
      lexer->context = CTX_NL;
    }
  }

  return token;
}
