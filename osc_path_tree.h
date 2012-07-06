

struct _tree_node;

int compare_strings(const char *l, char *r);

struct _tree_node *tree_new();

void tree_print(struct _tree_node *root, int indent);

int tree_add_string(struct _tree_node *root, char *string);

void tree_remove_string(struct _tree_node *root, const char *string);

int tree_match_string(struct _tree_node *root, const char *string);