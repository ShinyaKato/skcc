#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../../error.h"
#include "../../utf8.h"
#include "../../file.h"
#include "../../string.h"
#include "../../lex.h"

int main(int argc, char **argv) {
  if(argc < 2) exit(1);

  struct pp_token_lexer *lexer = allocate_pp_token_lexer(argv[1]);
  FILE *in = fopen(argv[2], "r");

  int n;
  fscanf(in, "%d", &n);

  for(int i = 0; i < n; i++) {
    char name[32];
    char text[2048];
    fscanf(in, "%s", name);
    int flag = strcmp(name, "new-line") != 0 && strcmp(name, "space") != 0 && strcmp(name, "none") != 0;
    if(flag) fscanf(in, "%s", text);

    fprintf(stderr, "  %d: %s", i, name);
    if(flag) fprintf(stderr, " %s", text);
    fprintf(stderr, "\n");

    struct pp_token *token = next_pp_token(lexer);
    int namediff = strcmp(name, token->name) != 0;
    int textdiff = flag && strcmp(text, token->text->head) != 0;
    if(namediff || textdiff)  {
      fprintf(stderr, "  ! output\n");
      fprintf(stderr, "  ! type: %d\n", token->type);
      fprintf(stderr, "  ! name: %s\n", token->name);
      if(token->type != PP_NEW_LINE && token->type != PP_SPACE && token->type != PP_NONE) {
        fprintf(stderr, "  ! text: %s\n", token->text->head);
      }
      printf("  NG %s\n", argv[2]);
      exit(1);
    }

    free_pp_token(token);
  }

  free_pp_token_lexer(lexer);

  printf("  OK %s\n", argv[2]);

  return 0;
}
