#include "main.h"

int main(int argc, char **argv) {
  if(argc < 2) {
    error("usage: skcc [source file name]");
  }

  char *file = argv[1];
  struct pp_list *list = preprocess(file);

  /* for(struct pp_node *node = list->head; node != NULL; node = node->next) { */
  /*   struct pp_token *token = node->token; */
  /*   printf("(%d, %s)", token->type, token->name); */
  /*   if(token->type != PP_NEW_LINE && token->type != PP_SPACE && token->type != PP_NONE) { */
  /*     printf(" %s", token->text->head); */
  /*   } */
  /*   printf("\n"); */
  /* } */

  for(struct pp_node *node = list->head; node != NULL; node = node->next) {
    struct pp_token *token = node->token;
    printf("%s", token->text->head);
  }

  return 0;
}
