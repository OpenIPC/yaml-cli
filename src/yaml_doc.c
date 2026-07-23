#include "yaml_doc.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* libyaml's document root is always the first node in the node array, and node
 * ids are 1-based, so the root's id is 1 whenever the document is non-empty.
 * A node id of 0 therefore means "no such node" throughout this file. */
#define ROOT_NODE_ID 1
#define NO_NODE 0

/* Deep enough for any realistic config; extra components are ignored. */
#define MAX_PATH_DEPTH 64

/* Split ".a.b.c" into its components. `buf` (caller-owned, >= strlen(path)+1)
 * receives a mutable copy that the returned token pointers point into. */
static int split_path(const char *path, char *buf, char *tokens[], int max) {
    int n = 0;
    char *tok;

    strcpy(buf, path);
    for (tok = strtok(buf, "."); tok != NULL && n < max;
         tok = strtok(NULL, "."))
        tokens[n++] = tok;

    return n;
}

/* Parse a whole-string non-negative decimal index (a sequence subscript). */
static int parse_index(const char *token, int *out) {
    const char *p;
    long value;
    char *end;

    if (token[0] == '\0')
        return 0;
    for (p = token; *p != '\0'; p++)
        if (*p < '0' || *p > '9')
            return 0;

    value = strtol(token, &end, 10);
    if (*end != '\0' || value > INT_MAX)
        return 0;

    *out = (int)value;
    return 1;
}

static int seq_length(yaml_node_t *seq) {
    return (int)(seq->data.sequence.items.top - seq->data.sequence.items.start);
}

/* Index within `map_id`'s pairs of the pair whose key scalar equals `key`, or
 * -1 if there is no such pair (or the node is not a mapping). */
static int find_pair(yaml_document_t *doc, int map_id, const char *key) {
    yaml_node_t *map = yaml_document_get_node(doc, map_id);
    yaml_node_pair_t *pair;

    if (map == NULL || map->type != YAML_MAPPING_NODE)
        return -1;

    for (pair = map->data.mapping.pairs.start;
         pair < map->data.mapping.pairs.top; pair++) {
        yaml_node_t *k = yaml_document_get_node(doc, pair->key);

        if (k != NULL && k->type == YAML_SCALAR_NODE &&
            strcmp((const char *)k->data.scalar.value, key) == 0)
            return (int)(pair - map->data.mapping.pairs.start);
    }

    return -1;
}

static int pair_value_id(yaml_document_t *doc, int map_id, int pair_pos) {
    yaml_node_t *map = yaml_document_get_node(doc, map_id);
    return map->data.mapping.pairs.start[pair_pos].value;
}

/* Follow one path component from `container_id`: a key in a mapping, or a
 * numeric index in a sequence. Returns the child node id, or NO_NODE if the
 * component does not resolve. When it does resolve, *pos is the pair/item index
 * and *is_seq says whether the container was a sequence. */
static int descend(yaml_document_t *doc, int container_id, const char *token,
                   int *pos, int *is_seq) {
    yaml_node_t *node = yaml_document_get_node(doc, container_id);

    if (node == NULL)
        return NO_NODE;

    if (node->type == YAML_MAPPING_NODE) {
        int p = find_pair(doc, container_id, token);
        if (p < 0)
            return NO_NODE;
        if (pos)
            *pos = p;
        if (is_seq)
            *is_seq = 0;
        return node->data.mapping.pairs.start[p].value;
    }

    if (node->type == YAML_SEQUENCE_NODE) {
        int idx;
        if (!parse_index(token, &idx) || idx >= seq_length(node))
            return NO_NODE;
        if (pos)
            *pos = idx;
        if (is_seq)
            *is_seq = 1;
        return node->data.sequence.items.start[idx];
    }

    return NO_NODE;
}

/* Remove one pair from a mapping / one item from a sequence, shifting the rest
 * down. Detached nodes stay in the document's node array and are freed with the
 * whole document, so no per-node cleanup is needed here. */
static void remove_entry(yaml_document_t *doc, int container_id, int pos,
                         int is_seq) {
    yaml_node_t *node = yaml_document_get_node(doc, container_id);

    if (is_seq) {
        yaml_node_item_t *start = node->data.sequence.items.start;
        int count = seq_length(node);
        memmove(&start[pos], &start[pos + 1],
                (count - pos - 1) * sizeof(yaml_node_item_t));
        node->data.sequence.items.top = start + (count - 1);
    } else {
        yaml_node_pair_t *start = node->data.mapping.pairs.start;
        int count = (int)(node->data.mapping.pairs.top - start);
        memmove(&start[pos], &start[pos + 1],
                (count - pos - 1) * sizeof(yaml_node_pair_t));
        node->data.mapping.pairs.top = start + (count - 1);
    }
}

static int container_is_empty(yaml_document_t *doc, int container_id,
                              int is_seq) {
    yaml_node_t *node = yaml_document_get_node(doc, container_id);

    return is_seq
               ? node->data.sequence.items.top ==
                     node->data.sequence.items.start
               : node->data.mapping.pairs.top == node->data.mapping.pairs.start;
}

int yaml_doc_get(yaml_document_t *doc, const char *path) {
    char buf[strlen(path) + 1];
    char *tokens[MAX_PATH_DEPTH];
    int n, i, cur;
    yaml_node_t *node;

    if (yaml_document_get_root_node(doc) == NULL)
        return 0;

    n = split_path(path, buf, tokens, MAX_PATH_DEPTH);
    if (n == 0)
        return 0;

    cur = ROOT_NODE_ID;
    for (i = 0; i < n; i++) {
        cur = descend(doc, cur, tokens[i], NULL, NULL);
        if (cur == NO_NODE)
            return 0;
    }

    node = yaml_document_get_node(doc, cur);
    if (node == NULL)
        return 0;

    if (node->type == YAML_SCALAR_NODE) {
        printf("%s\n", (const char *)node->data.scalar.value);
        return 1;
    }

    /* A whole sequence prints its scalar elements, one per line. */
    if (node->type == YAML_SEQUENCE_NODE) {
        yaml_node_item_t *item;
        int printed = 0;

        for (item = node->data.sequence.items.start;
             item < node->data.sequence.items.top; item++) {
            yaml_node_t *el = yaml_document_get_node(doc, *item);
            if (el != NULL && el->type == YAML_SCALAR_NODE) {
                printf("%s\n", (const char *)el->data.scalar.value);
                printed = 1;
            }
        }
        return printed;
    }

    return 0;
}

int yaml_doc_set(yaml_document_t *doc, const char *path, const char *value) {
    char buf[strlen(path) + 1];
    char *tokens[MAX_PATH_DEPTH];
    int n, i, cur, pos, value_id;
    yaml_node_t *node;

    n = split_path(path, buf, tokens, MAX_PATH_DEPTH);
    if (n == 0)
        return 0;

    if (yaml_document_get_root_node(doc) == NULL) {
        if (yaml_document_add_mapping(doc, NULL, YAML_BLOCK_MAPPING_STYLE) == 0)
            return 0;
        /* Freshly created document: keep the start/end indicators implicit so
         * the output has no explicit "---"/"..." markers, matching the
         * in-place edits of an existing file. */
        doc->start_implicit = 1;
        doc->end_implicit = 1;
    }
    cur = ROOT_NODE_ID;

    /* Descend through the intermediate components. Sequences are traversed by
     * index (the element must already exist); missing mapping keys are created
     * as nested mappings. */
    for (i = 0; i < n - 1; i++) {
        node = yaml_document_get_node(doc, cur);

        if (node != NULL && node->type == YAML_SEQUENCE_NODE) {
            int idx;
            if (!parse_index(tokens[i], &idx) || idx >= seq_length(node))
                return 0;
            cur = node->data.sequence.items.start[idx];
            continue;
        }

        if (node == NULL || node->type != YAML_MAPPING_NODE)
            return 0;

        pos = find_pair(doc, cur, tokens[i]);

        if (pos >= 0) {
            int child = pair_value_id(doc, cur, pos);
            yaml_node_t *cn = yaml_document_get_node(doc, child);

            /* Descend into an existing container (mapping or sequence). */
            if (cn != NULL && (cn->type == YAML_MAPPING_NODE ||
                               cn->type == YAML_SEQUENCE_NODE)) {
                cur = child;
                continue;
            }

            /* Key holds a scalar: replace it with a fresh mapping to nest into.
             */
            int new_map =
                yaml_document_add_mapping(doc, NULL, YAML_BLOCK_MAPPING_STYLE);
            if (new_map == 0)
                return 0;
            node = yaml_document_get_node(doc, cur); /* re-fetch after add */
            node->data.mapping.pairs.start[pos].value = new_map;
            cur = new_map;
        } else {
            int key_id = yaml_document_add_scalar(
                doc, NULL, (const yaml_char_t *)tokens[i], -1,
                YAML_ANY_SCALAR_STYLE);
            int map_id =
                yaml_document_add_mapping(doc, NULL, YAML_BLOCK_MAPPING_STYLE);
            if (key_id == 0 || map_id == 0)
                return 0;
            if (!yaml_document_append_mapping_pair(doc, cur, key_id, map_id))
                return 0;
            cur = map_id;
        }
    }

    node = yaml_document_get_node(doc, cur);
    if (node == NULL)
        return 0;

    /* Setting an element of a sequence: replace in-bounds, or append when the
     * index is exactly the current length. */
    if (node->type == YAML_SEQUENCE_NODE) {
        int idx, count;
        if (!parse_index(tokens[n - 1], &idx))
            return 0;
        count = seq_length(node);
        if (idx > count)
            return 0;

        value_id = yaml_document_add_scalar(
            doc, NULL, (const yaml_char_t *)value, -1, YAML_ANY_SCALAR_STYLE);
        if (value_id == 0)
            return 0;

        node = yaml_document_get_node(doc, cur); /* re-fetch after add */
        if (idx < count)
            node->data.sequence.items.start[idx] = value_id;
        else if (!yaml_document_append_sequence_item(doc, cur, value_id))
            return 0;
        return 1;
    }

    if (node->type != YAML_MAPPING_NODE)
        return 0;

    pos = find_pair(doc, cur, tokens[n - 1]);
    value_id = yaml_document_add_scalar(doc, NULL, (const yaml_char_t *)value,
                                        -1, YAML_ANY_SCALAR_STYLE);
    if (value_id == 0)
        return 0;

    if (pos >= 0) {
        node = yaml_document_get_node(doc, cur); /* re-fetch after add */
        node->data.mapping.pairs.start[pos].value = value_id;
    } else {
        int key_id = yaml_document_add_scalar(
            doc, NULL, (const yaml_char_t *)tokens[n - 1], -1,
            YAML_ANY_SCALAR_STYLE);
        if (key_id == 0)
            return 0;
        if (!yaml_document_append_mapping_pair(doc, cur, key_id, value_id))
            return 0;
    }

    return 1;
}

int yaml_doc_del(yaml_document_t *doc, const char *path) {
    char buf[strlen(path) + 1];
    char *tokens[MAX_PATH_DEPTH];
    int container_id[MAX_PATH_DEPTH];
    int entry_pos[MAX_PATH_DEPTH];
    int entry_is_seq[MAX_PATH_DEPTH];
    int n, i, cur;

    if (yaml_document_get_root_node(doc) == NULL)
        return 0;

    n = split_path(path, buf, tokens, MAX_PATH_DEPTH);
    if (n == 0)
        return 0;

    /* Record the container and pair/item index followed at each level. */
    cur = ROOT_NODE_ID;
    for (i = 0; i < n; i++) {
        int pos, is_seq;
        int child = descend(doc, cur, tokens[i], &pos, &is_seq);
        if (child == NO_NODE)
            return 0;
        container_id[i] = cur;
        entry_pos[i] = pos;
        entry_is_seq[i] = is_seq;
        cur = child;
    }

    /* Remove the target entry, then walk back up removing any container the
     * removal left empty. Each level is a distinct container, so the recorded
     * positions in the shallower ones stay valid. The root is never removed. */
    for (i = n - 1; i >= 0; i--) {
        remove_entry(doc, container_id[i], entry_pos[i], entry_is_seq[i]);
        if (i == 0 ||
            !container_is_empty(doc, container_id[i], entry_is_seq[i]))
            break;
    }

    return 1;
}
