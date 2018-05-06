#include "main.h"

int main(int argc, char **argv) {
  if(argc < 2) {
    error("usage: skcc [source file name]");
  }

  char *file = argv[1];
  struct pp_token_lexer *lexer = allocate_pp_token_lexer(file);

  for(int i = 0;; i++) {
    struct pp_token *token = next_pp_token(lexer);
    printf("%d: (%d, %s)", i, token->type, token->name);
    if(token->type != PP_NEW_LINE && token->type != PP_SPACE && token->type != PP_NONE) {
      printf(" %s", token->text->head);
    }
    printf("\n");

    if(token->type == PP_NONE) {
      free_pp_token(token);
      break;
    }

    free_pp_token(token);
  }

  free_pp_token_lexer(lexer);

  return 0;
}
