#ifndef __LEX_INCLUDE__
#define __LEX_INCLUDE__

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "error.h"
#include "utf8.h"
#include "file.h"
#include "string.h"

enum pp_token_lexer_state {
  ST_START,

  ST_H_CHAR, ST_Q_CHAR, ST_H_NAME,

  ST_IDENT_UL, ST_IDENT_UU, ST_IDENT_LU, ST_IDENT_LU8, ST_IDENT_ESC,
  ST_IDENT_LUNIV, ST_IDENT_LUNIV_1, ST_IDENT_LUNIV_2, ST_IDENT_LUNIV_3, ST_IDENT_UUNIV,
  ST_IDENT_UUNIV_1, ST_IDENT_UUNIV_2, ST_IDENT_UUNIV_3, ST_IDENT_UUNIV_4, ST_IDENT,

  ST_NUM, ST_NUM_SIGN, ST_NUM_ESC, ST_NUM_LUNIV, ST_NUM_LUNIV_1, ST_NUM_LUNIV_2,
  ST_NUM_LUNIV_3, ST_NUM_UUNIV, ST_NUM_UUNIV_1, ST_NUM_UUNIV_2, ST_NUM_UUNIV_3, ST_NUM_UUNIV_4,

  ST_CHAR, ST_C_MEMBER, ST_C_ESC, ST_C_LUNIV, ST_C_LUNIV_1, ST_C_LUNIV_2, ST_C_LUNIV_3,
  ST_C_UUNIV, ST_C_UUNIV_1, ST_C_UUNIV_2, ST_C_UUNIV_3, ST_C_UUNIV_4,
  ST_C_OCT_1, ST_C_OCT_2, ST_C_HEX, ST_C_HEX_R, ST_CHAR_CNST,

  ST_STR, ST_S_MEMBER, ST_S_ESC, ST_S_LUNIV, ST_S_LUNIV_1, ST_S_LUNIV_2, ST_S_LUNIV_3,
  ST_S_UUNIV, ST_S_UUNIV_1, ST_S_UUNIV_2, ST_S_UUNIV_3, ST_S_UUNIV_4,
  ST_S_OCT_1, ST_S_OCT_2, ST_S_HEX, ST_S_HEX_R, ST_STR_LTRL,

  ST_LBRACKET, ST_RBRACKET, ST_LPAREN, ST_RPAREN, ST_LBRACE, ST_RBRACE,
  ST_DOT, ST_DOT_2, ST_ELIPSIS, ST_COMMA, ST_PLUS, ST_INC, ST_PLUS_ASGN,
  ST_MINUS, ST_DEC, ST_MINUS_ASGN, ST_ARROW, ST_MUL, ST_MUL_ASGN, ST_DIV, ST_DIV_ASGN,
  ST_MOD, ST_MOD_ASGN, ST_DIG_RBRACE, ST_DIG_SHARP, ST_DIG_SHARP_MOD, ST_DIG_CONCAT,
  ST_AND, ST_LAND, ST_AND_ASGN, ST_OR, ST_LOR, ST_OR_ASGN, ST_XOR, ST_XOR_ASGN,
  ST_ASGN, ST_EQ, ST_NOT, ST_NEQ, ST_LT, ST_LSHIFT, ST_LSHIFT_ASGN, ST_LTE,
  ST_DIG_LBRACKET, ST_DIG_LBRACE, ST_GT, ST_RSHIFT, ST_RSHIFT_ASGN, ST_GTE,
  ST_TILDE, ST_QUESTION, ST_COLON, ST_DIG_RBRACKET, ST_SEMICOLON, ST_SHARP, ST_CONCAT,

  ST_NEW_LINE, ST_SPACE,
};

enum pp_token_type {
  PP_H_NAME, PP_IDENT, PP_NUM, PP_CHAR, PP_STR,

  PP_LBRACKET, PP_RBRACKET, PP_LPAREN, PP_RPAREN, PP_LBRACE, PP_RBRACE,
  PP_DOT, PP_ELIPSIS, PP_COMMA, PP_PLUS, PP_INC, PP_PLUS_ASGN,
  PP_MINUS, PP_DEC, PP_MINUS_ASGN, PP_ARROW, PP_MUL, PP_MUL_ASGN, PP_DIV, PP_DIV_ASGN,
  PP_MOD, PP_MOD_ASGN, PP_DIG_RBRACE, PP_DIG_SHARP, PP_DIG_CONCAT,
  PP_AND, PP_LAND, PP_AND_ASGN, PP_OR, PP_LOR, PP_OR_ASGN, PP_XOR, PP_XOR_ASGN,
  PP_ASGN, PP_EQ, PP_NOT, PP_NEQ, PP_LT, PP_LSHIFT, PP_LSHIFT_ASGN, PP_LTE,
  PP_DIG_LBRACKET, PP_DIG_LBRACE, PP_GT, PP_RSHIFT, PP_RSHIFT_ASGN, PP_GTE,
  PP_TILDE, PP_QUESTION, PP_COLON, PP_DIG_RBRACKET, PP_SEMICOLON, PP_SHARP, PP_CONCAT,

  PP_NEW_LINE, PP_SPACE, PP_OTHER, PP_NONE
};

enum pp_token_lexer_context { CTX_NL, CTX_SHARP, CTX_INCLUDE, CTX_NORMAL };

struct pp_token_lexer {
  struct source *src;
  int comment_queue_size;
  struct utf8c comment_queue[2];
  struct utf8c *queue;
  int queue_head;
  int queue_size;
  int queue_allocate_size;
  enum pp_token_lexer_context context;
};

struct pp_token {
  enum pp_token_type type;
  const unsigned char *name;
  struct string *text;
};

extern const unsigned char pp_token_name[][32];

extern struct pp_token_lexer *allocate_pp_token_lexer(const unsigned char *file);
extern void free_pp_token_lexer(struct pp_token_lexer *lexer);
extern struct pp_token *allocate_token();
extern void free_pp_token(struct pp_token *token);
extern struct pp_token *next_pp_token(struct pp_token_lexer *lexer);

#endif
