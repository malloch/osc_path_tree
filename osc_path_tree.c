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

tree_node tree_add_string_internal(tree_node root, char *string,
                                   int force_as_leaf, int is_endpoint, tree_node temp);
tree_node tree_match_string_internal(tree_node root, const char *string, int *status);

int compare_strings(const char *l, char *r)
{
    // TODO: OSC pattern matching!
    // *?![][-][!]{,}
    // will handle {} at tree level
    if (!l || !r)
        return 0;
    char *str = (char*)l;
    int i = 0;
    while (*str && *r) {
        if ((*str != *r) && (*str != '?') && (*r != '?'))
            break;
        str++;
        r++;
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
    if (root->leaves)
        tree_print(root->leaves, level+1);
    if (root->next) {
        tree_print(root->next, level);
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
    return !tree_add_string_internal(root, string+1, 0, 1, root);
}

tree_node tree_add_string_internal(tree_node root, char *string,
                                   int force_as_leaf, int is_endpoint, tree_node temp)
{
    printf("tree_add_string_internal: %p, %s\n", root, string);
    printf("\n\n");
    tree_print(temp, 0);
    printf("\n\n");
    tree_node leaf;

    if (strchr(string, '{')) {
        char *dup, *end;
        int len, span;
        // split the list and add each element to tree
        // Assume string has already been checked for illegal sequences
printf("string = %s\n", string);
        // First add string before opening brace
        if (span = strcspn(string, "{")) {
            string[span] = 0;
            root = tree_add_string_internal(root, string, 0, 0, root);
            string += span+1;
        }
        else
            string += 1;
printf("string = %s\n", string);

        // next handle list of possible strings
        len = strlen(string);
        printf("string length is %i\n", len);
        span = strcspn(string, "}");
        printf("span = %i\n", span);
        printf("string[span] = %c\n", string[span]);
        dup = (char*) calloc(1, span+1);
        snprintf(dup, span+1, "%s", string);
        printf("string is now %s\n", dup);
        if (len > span+1) {
            end = strdup(string+span+1);
            printf("end is %s\n", end);
        }
        else
            end = 0;

        // next handle list of possible strings
        while (span = strcspn(dup, ",")) {
            dup[span] = 0;
            printf("gonna add %s to %p\n", dup, root);
            leaf = tree_add_string_internal(root, dup, 1, (end == 0), root);
            if (end)
                tree_add_string_internal(leaf, end, 0, 1, root);
            dup += span+1;
        }

        if (end)
            free(end);
        free(dup);
        return root;
    }

    // basically 3 possibilities:
    //  no match - go to next or add sibling
    //  partial match - go to leaves or add leaf
    //  full match - go to leaves or add leaf
    int offset = compare_strings(root->string, string);
    string += offset;
    if (!offset) {
        printf("no match\n");
        // doesn't match at all.
        if (root->next) {
            printf("checking next\n");
            return tree_add_string_internal(root->next, string,
                                            force_as_leaf, is_endpoint, root);
        }
        else {
            // need to add sibling or child leaf
            printf("adding sibling leaf %s\n", string);
            leaf = (tree_node) calloc(1, sizeof(struct _tree_node));
            leaf->string = strdup(string);
            leaf->string_len = strlen(string);
            leaf->is_endpoint += is_endpoint;
            leaf->parent = root;
            if (force_as_leaf) {
                leaf->next = root->leaves;
                root->leaves = leaf;
            }
            else {
                leaf->next = root->next;
                root->next = leaf;
            }
            return leaf;
        }
    }
    else if (!*string) {
        if (offset < root->string_len) {
            printf("partial match!\n");
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
            printf("we matched entire string\n");
            // we matched the entire string.
            // we need to mark this node as an endpoint.
            root->is_endpoint += is_endpoint;
            return root;
        }
    }
    else {
        printf("we matched a substring\n");
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
            printf("we matched entire root string\n");
            // we matched entire root->string, proceed to leaves.
            if (root->leaves) {
                printf("checking leaves\n");
                return tree_add_string_internal(root->leaves, string,
                                                force_as_leaf, is_endpoint, root);
            }
            else {
                printf("adding leaf\n");
                leaf = (tree_node) calloc(1, sizeof(struct _tree_node));
                leaf->string = strdup(string);
                leaf->string_len = strlen(string);
                leaf->is_endpoint += is_endpoint;
                leaf->parent = root;
                root->leaves = leaf;
                return leaf;
            }
        }
    }

    printf("something went wrong!\n");
    return 0;
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