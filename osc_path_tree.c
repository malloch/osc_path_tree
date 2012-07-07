#include "osc_path_tree.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct _tree_node {
    char *string;
    int string_len;
    int is_endpoint;
    void *payload;
    struct _tree_node *parent;
    struct _tree_node *leaves;
    struct _tree_node *next;
} *tree_node;

tree_node tree_add_string_internal(tree_node root, char *string, int is_endpoint);
tree_node tree_match_string_internal(tree_node root, const char *string, int *status);

int compare_strings(const char *l, char *r)
{
    // TODO: OSC pattern matching!
    // *?![][-][!]{,}
    // will handle {} at tree level
    char *str = (char*)l;
    int i = 0;
    while (*str && *r) {
        if (*(str++) != *(r++))
            break;
        i++;
    }
    return i;
}

tree_node tree_new()
{
    tree_node node = (tree_node) calloc(1, sizeof(struct _tree_node));
    return node;
}

void tree_print(struct _tree_node *root, int level)
{
    if (root->parent) {
        int i;
        for (i=0; i<level; i++) {
            printf(" ");
        }
        if (root->is_endpoint)
            printf("%s •    (address: %p) (parent: %p)\n", root->string, root, root->parent);
        else
            printf("%s ¬    (address: %p) (parent: %p)\n", root->string, root, root->parent);
    }
    tree_node leaf = root->leaves;
    while (leaf) {
        tree_print(leaf, level+1);
        leaf = leaf->next;
    }
}

int tree_check_string(const char *string)
{
    if (string[0] != '/')
        return 1;
    int i = 1, open_bracket = 0, open_brace = 0;
    char *end;
    while (1) {
        switch (string[i++]) {
            case 0:
                return 0;
            case '#':
            case ' ':
                return 1;
            case '[':
                end = strpbrk(string+i+1, "]");
                if (!end)
                    return 1;
                open_bracket = 1;
                break;
            case ']':
                if (!open_bracket)
                    return 1;
                open_bracket = 0;
                break;
            case '{':
                end = strpbrk(string+i+1, "}");
                if (!end)
                    return 1;
                open_brace = 1;
                break;
            case ',':
                if (!open_brace)
                    return 1;
                open_brace ++;
                break;
            case '}':
                if (open_brace < 2)
                    return 1;
                open_brace = 0;
                break;
            default:
                break;
        }
    }
}

int tree_add_string(tree_node root, char *string)
{
    int result = tree_check_string(string);
    if (result)
        return 1;
    return !tree_add_string_internal(root, string+1, 1);
}

tree_node tree_add_string_internal(tree_node root, char *string, int is_endpoint)
{
    tree_node leaf;

    if (strchr(string, '{')) {
        char *ptr;
        // split the list and add each element to tree
        // Assume string has already been checked for illegal sequences

        // First add string before opening brace
        if (string[0] != '{') {
            ptr = strtok(string, "{");
            root = tree_add_string_internal(root, ptr, 0);
            string += strlen(ptr)+1;
        }
        else
            string += 1;

        // next handle list of possible strings
        char *list = strdup(string);
        ptr = strchr(list, '}');
        ptr[0] = 0;
        string += strlen(list)+1;

        ptr = strtok(list, ",");
        while (ptr != NULL) {
            leaf = tree_add_string_internal(root, ptr, 0);
            if (leaf) {
                if (*string) {
                    // Add the rest of the string
                    if (leaf->is_endpoint)
                        tree_add_string_internal(leaf, string, 1);
                    else {
                        leaf->string_len += strlen(string);
                        leaf->string = (char*) realloc(leaf->string,
                                                       leaf->string_len + 1);
                        strcat(leaf->string, string);
                        leaf->is_endpoint = 1;
                    }
                }
                else
                    leaf->is_endpoint++;
            }
            ptr = strtok(NULL, ",");
        }
        free(list);
        return root;
    }
    if (root->parent) {
        int offset = compare_strings(root->string, string);
        string += offset;
        if (!offset) {
            // doesn't match at all.
            // TODO: need to clean up?
            return 0;
        }
        else if (!*string) {
            if (offset < root->string_len) {
                // we need to split the root->string and create 1 new leaf.
                leaf = (tree_node) calloc(1, sizeof(struct _tree_node));
                leaf->string = strdup(root->string+offset);
                leaf->string_len = root->string_len-offset;
                leaf->is_endpoint += is_endpoint;

                root->string = (char*) realloc(root->string, offset+1);
                root->string[offset] = 0;
                root->string_len = offset;

                leaf->parent = root;
                if (root->leaves)
                    root->leaves->parent = leaf;
                leaf->leaves = root->leaves;
                root->leaves = leaf;

                return root;
            }
            else {
                // we matched the entire string.
                if (root->is_endpoint) {
                    // is endpoint, we are done!
                    return root;
                }
                else {
                    // we need to mark this node as an endpoint.
                    root->is_endpoint += is_endpoint;
                    return root;
                }
            }
        }
        else {
            // we matched a substring.
            if (offset < root->string_len) {
                // we need to split the root->string and create 2 new leaves.
                tree_node leaf2;
                leaf = (tree_node) calloc(1, sizeof(struct _tree_node));
                leaf->string = strdup(string);
                leaf->string_len = strlen(string);
                leaf->is_endpoint += is_endpoint;

                leaf2 = (tree_node) calloc(1, sizeof(struct _tree_node));
                leaf2->string = strdup(root->string+offset);
                leaf2->string_len = root->string_len-offset;
                leaf2->is_endpoint = root->is_endpoint;

                root->string = (char*) realloc(root->string, offset+1);
                root->string[offset] = 0;
                root->string_len = offset;
                root->is_endpoint = 0;

                leaf->parent = leaf2;
                leaf2->next = leaf;
                if (root->leaves)
                    root->leaves->parent = leaf2;
                leaf2->leaves = root->leaves;
                leaf2->parent = root;
                root->leaves = leaf2;

                return leaf;
            }
            else {
                // we matched entire root->string, proceed to leaves.
                goto process_leaves;
            }
        }
    }
    else {
        // is root: proceed directly to leaves.
        goto process_leaves;
    }

    printf("something went wrong!");
    return 0;

process_leaves:
    leaf = root->leaves;
    while (leaf) {
        tree_node result = tree_add_string_internal(leaf, string, is_endpoint);
        if (result)
            return result;
        leaf = leaf->next;
    }
    // need to add leaf
    leaf = (tree_node) calloc(1, sizeof(struct _tree_node));
    leaf->string = strdup(string);
    leaf->string_len = strlen(string);
    leaf->is_endpoint += is_endpoint;
    leaf->parent = root;
    if (root->leaves) {
        root->leaves->parent = leaf;
        leaf->next = root->leaves;
    }
    root->leaves = leaf;
    return leaf;
}

int tree_remove_string(tree_node root, const char *string)
{
    int result;
    if (root->parent) {
        int offset = compare_strings(root->string, (char *)string);
        if (offset < root->string_len)
            return 1;
        // substring match
        string += offset;
        if (!*string) {
            // finished matching, can delete?
            return 0;
        }
    }
    // continue to leaves
    tree_node leaf = root->leaves;
    while (leaf) {
        result = tree_remove_string(leaf, string);
        if (!result && !leaf->leaves) {
            // can delete this leaf
            if (leaf->next)
                leaf->next->parent = leaf->parent;
            if (leaf->parent != root)
                leaf->parent->next = leaf->next;
            else
                root->leaves = leaf->next;
            if (leaf->string)
                free(leaf->string);
            free(leaf);
            // check if we can consolidate strings
            result = 0;
            leaf = root->leaves;
            while (leaf) {
                result++;
                leaf = leaf->next;
            }
            if (result == 1) {
                // we can consolidate strings!
                leaf = root->leaves;
                root->string = (char*) realloc(root->string, root->string_len
                                               + leaf->string_len + 1);
                strcat(root->string, leaf->string);
                root->leaves = leaf->leaves;
                free(leaf->string);
                free(leaf);
            }
            return 0;
        }
        leaf = leaf->next;
    }
    return 1;
}

int tree_match_string(tree_node root, const char *string)
{
    int status = 0;
    return !tree_match_string_internal(root, string, &status);
}

tree_node tree_match_string_internal(tree_node root, const char *string, int *status)
{
    int internal_status = *status;
    tree_node node;
    if (root->parent) {
        int offset = compare_strings(root->string, (char *)string);
        if (offset < root->string_len)
            return 0;
        else {
            // substring match
            (*status)++;
            internal_status++;
            string += offset;
        }
    }
    if (!*string) 
        return root;
    // continue to leaves
    tree_node leaf = root->leaves;
    while (leaf) {
        node = tree_match_string_internal(leaf, string, status);
        if (node)
            return node;
        if (*status > internal_status)
            return 0;
        leaf = leaf->next;
    }
    return 0;
}