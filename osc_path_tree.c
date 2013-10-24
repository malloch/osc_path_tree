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

tree_node tree_add_string_internal(tree_node root, char *string, int length,
                                   int force_as_leaf, int is_endpoint, tree_node temp);
tree_node tree_match_string_internal(tree_node root, const char *string, int *status);

int compare_strings(const char *string1, const char *string2, int length)
{
    // TODO: OSC pattern matching!
    // *?![][-][!]{,}
    // will handle {} at tree level
    if (!string1 || !string2 || !length)
        return 0;
    char *l = (char*)string1;
    char *r = (char*)string2;
    length -= 1;
    int i = 0;
    while (*l && *r && i <= length) {
        if ((*l != *r) && (*l != '?') && (*r != '?'))
            break;
        l++;
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
    return !tree_add_string_internal(root, string+1, strlen(string)-1, 0, 1, root);
}

tree_node tree_add_string_internal(tree_node root, char *string, int length,
                                   int force_as_leaf, int is_endpoint, tree_node temp)
{
    tree_node leaf;

    char *brace = strchr(string, '{');

    if (brace && brace < string+length) {
        int span;
        char *end;
        
        // split the list and add each element to tree
        // Assume string has already been checked for illegal sequences

        // First add string before opening brace
        if (span = strcspn(string, "{")) {
            root = tree_add_string_internal(root, string, span, 0, 0, root);
            string += span+1;
        }
        else
            string += 1;

        span = strcspn(string, "}");
        if (span > length) {
            return 0;
        }
        if (length > span+1) {
            end = string+span+1;
        }
        else
            end = 0;

        // next handle list of possible strings
        while (string != end) {
            span = strcspn(string, ",}");
            leaf = tree_add_string_internal(root, string, span, 1, (end == 0), root);
            if (end)
                tree_add_string_internal(leaf, end, strlen(end), 0, 1, root);
            string += span+1;
        }
        return root;
    }

    // basically 3 possibilities:
    //  no match - go to next or add sibling
    //  partial match - go to leaves or add leaf
    //  full match - go to leaves or add leaf
    tree_node child = root->leaves;
    while (child) {
        int offset = compare_strings(child->string, string, length);
        if (!offset) {
            // doesn't match at all.
            child = child->next;
            continue;
        }
        string += offset;
        if (!*string) {
            if (offset < child->string_len) {
                // partial match
                // we need to split the string and create 1 new leaf.
                leaf = (tree_node) calloc(1, sizeof(struct _tree_node));
                leaf->string = strdup(child->string+offset);
                leaf->string_len = child->string_len-offset;
                leaf->is_endpoint += is_endpoint;

                child->string = (char*) realloc(child->string, offset+1);
                child->string[offset] = 0;
                child->string_len = offset;

                leaf->parent = child;
                if (child->leaves)
                    child->leaves->parent = leaf;
                leaf->leaves = child->leaves;
                child->leaves = leaf;

                return leaf;
            }
            else {
                // we matched the entire string.
                // we need to mark this node as an endpoint.
                child->is_endpoint += is_endpoint;
                // todo: store user_data
                return child;
            }
        }
        else {
            // we matched a substring.
            if (offset < child->string_len) {
                // we need to split the string and create 2 new leaves.
                tree_node leaf2;
                leaf = (tree_node) calloc(1, sizeof(struct _tree_node));
                leaf->string = strdup(string);
                leaf->string_len = strlen(string);
                leaf->is_endpoint += is_endpoint;

                leaf2 = (tree_node) calloc(1, sizeof(struct _tree_node));
                leaf2->string = strdup(child->string+offset);
                leaf2->string_len = child->string_len-offset;
                leaf2->is_endpoint = child->is_endpoint;

                child->string = (char*) realloc(child->string, offset+1);
                child->string[offset] = 0;
                child->string_len = offset;
                child->is_endpoint = 0;

                leaf->parent = leaf2;
                leaf2->next = leaf;
                if (child->leaves)
                    child->leaves->parent = leaf2;
                leaf2->leaves = child->leaves;
                leaf2->parent = child;
                child->leaves = leaf2;

                return child;
            }
            else {
                // we matched entire string, proceed to leaves.
                if (child->leaves) {
                    return tree_add_string_internal(child, string,
                                                    length-offset,
                                                    force_as_leaf, is_endpoint,
                                                    child);
                }
                else {
                    // add new leaf
                    leaf = (tree_node) calloc(1, sizeof(struct _tree_node));
                    leaf->string = strdup(string);
                    leaf->string_len = strlen(string);
                    leaf->is_endpoint += is_endpoint;
                    leaf->parent = child;
                    child->leaves = leaf;
                    return leaf;
                }
            }
        }
        child = child->next;
    }

    // no matches or partial matches found, add as root leaf
    leaf = (tree_node) calloc(1, sizeof(struct _tree_node));
    leaf->string = calloc(1, length+1);
    strncpy(leaf->string, string, length);
    leaf->string_len = length;
    leaf->is_endpoint += is_endpoint;
    leaf->parent = root;
    leaf->next = root->leaves;
    root->leaves = leaf;
    return leaf;
}

int tree_remove_string_internal(tree_node root, const char *string)
{
    int result;
    if (root->parent) {
        int offset = compare_strings(root->string, string, strlen(string));
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
        result = tree_remove_string_internal(leaf, string);
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

int tree_remove_string(tree_node root, const char *string)
{
    if (!string || string[0] != '/')
        return 1;
    else
        return tree_remove_string_internal(root, string+1);
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
        int offset = compare_strings(root->string, (char *)string, strlen(string));
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