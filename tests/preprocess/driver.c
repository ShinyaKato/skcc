#include "../../main.h"

int main(int argc, char **argv) {
  char *file = argv[1];
  struct pp_list *list = preprocess(file);

  FILE *out = fopen(argv[2], "w");
  for(struct pp_node *node = list->head; node != NULL; node = node->next) {
    struct pp_token *token = node->token;
    fprintf(out, "%s", token->text->head);
  }

  return 0;
}
