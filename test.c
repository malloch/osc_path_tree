#include <stdio.h>
#include <stdlib.h>
#include "osc_path_tree.h"

int main (int argc, const char * argv[]) {
    struct _tree_node *root = tree_new();
    tree_add_string(root, "foo");
    tree_add_string(root, "barbie");
    tree_add_string(root, "food");
    tree_add_string(root, "bbq");
    tree_add_string(root, "barbeque");
    tree_add_string(root, "barby");
    tree_add_string(root, "baa");
    tree_print(root, 0);

    printf("search %i\n", tree_match_string(root, "barbeque"));
    return 0;
}
