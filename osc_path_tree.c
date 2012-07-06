#include "osc_path_tree.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct _tree_node {
    char *string;
    int string_len;
    int is_root;
    int is_endpoint;
    void *payload;
    struct _tree_node *leaves;
    struct _tree_node *next;
} *tree_node;

int tree_match_string_internal(tree_node root, const char *string, int *status);

int compare_strings(const char *l, char *r)
{
    // TODO: OSC pattern matching!
    // *?![]{}
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
    node->is_root = 1;
    return node;
}

void tree_print(struct _tree_node *root, int level)
{
    if (!root->is_root) {
        int i;
        for (i=0; i<level; i++) {
            printf(" ");
        }
        if (root->is_endpoint)
            printf("%s ] (%i)\n", root->string, root->string_len);
        else
            printf("%s (%i)\n", root->string, root->string_len);
    }
    tree_node leaf = root->leaves;
    while (leaf) {
        tree_print(leaf, level+1);
        leaf = leaf->next;
    }
}

int tree_add_string(tree_node root, char *string)
{
    tree_node leaf;
    if (!root->is_root) {
        int offset = compare_strings(root->string, string);
        string += offset;
        if (!offset) {
            // doesn't match at all.
            return 1;
        }
        else if (!*string) {
            if (offset < root->string_len) {
                // we need to split the root->string and create 1 new leaf.
                leaf = (tree_node) calloc(1, sizeof(struct _tree_node));
                leaf->string = strdup(root->string+offset);;
                leaf->string_len = root->string_len-offset;
                leaf->is_endpoint = 1;
                leaf->next = root->leaves;

                root->string = (char*) realloc(root->string, offset+1);
                root->string[offset] = 0;
                root->string_len = offset;
                root->is_endpoint = 1;
                root->leaves = leaf;

                return 0;
            }
            else {
                // we matched the entire string.
                if (root->is_endpoint) {
                    // is endpoint, we are done!
                    return 0;
                }
                else {
                    // we need to mark this node as an endpoint.
                    root->is_endpoint = 1;
                    return 0;
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
                leaf->is_endpoint = 1;
                leaf->next = root->leaves;

                leaf2 = (tree_node) calloc(1, sizeof(struct _tree_node));
                leaf2->string = strdup(root->string+offset);
                leaf2->string_len = root->string_len-offset;
                leaf2->is_endpoint = 1;
                leaf2->next = leaf;

                root->string = (char*) realloc(root->string, offset+1);
                root->string[offset] = 0;
                root->string_len = offset;
                root->is_endpoint = 0;
                root->leaves = leaf2;

                return 0;
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
        if (!tree_add_string(leaf, string))
            return 0;
        leaf = leaf->next;
    }
    // need to add leaf
    leaf = (tree_node) calloc(1, sizeof(struct _tree_node));
    leaf->string = strdup(string);
    leaf->string_len = strlen(string);
    leaf->is_endpoint = 1;
    leaf->next = root->leaves;
    root->leaves = leaf;
    return 0;
}

void tree_remove_string(tree_node root, const char *string)
{
}

int tree_match_string(tree_node root, const char *string)
{
    int status = 0;
    return tree_match_string_internal(root, string, &status);
}

int tree_match_string_internal(tree_node root, const char *string, int *status)
{
    int internal_status = *status;
    if (!root->is_root) {
        int offset = compare_strings(root->string, (char *)string);
        if (offset < root->string_len) {
            // not a match
            return 1;
        }
        else {
            // substring match, continue to leaves
            (*status)++;
            internal_status++;
            string += offset;
        }
    }
    if (!*string)
        return 0;
    tree_node leaf = root->leaves;
    while (leaf) {
        if (!tree_match_string_internal(leaf, string, status))
            return 0;
        if (*status > internal_status)
            return 1;
        leaf = leaf->next;
    }
    return 1;
}