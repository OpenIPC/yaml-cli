#include "yaml_doc.h"

#include <stdio.h>
#include <string.h>

/* libyaml's document root is always the first node in the node array, and node
 * ids are 1-based, so the root's id is 1 whenever the document is non-empty. */
#define ROOT_NODE_ID 1

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

/* Remove one pair from a mapping, shifting the rest down. The detached key and
 * value nodes stay in the document's node array and are freed with the rest of
 * the document, so no per-node cleanup is needed here. */
static void remove_pair(yaml_document_t *doc, int map_id, int pair_pos) {
    yaml_node_t *map = yaml_document_get_node(doc, map_id);
    yaml_node_pair_t *start = map->data.mapping.pairs.start;
    int count = (int)(map->data.mapping.pairs.top - start);

    memmove(&start[pair_pos], &start[pair_pos + 1],
            (count - pair_pos - 1) * sizeof(yaml_node_pair_t));
    map->data.mapping.pairs.top = start + (count - 1);
}

static int mapping_is_empty(yaml_document_t *doc, int map_id) {
    yaml_node_t *map = yaml_document_get_node(doc, map_id);
    return map->data.mapping.pairs.top == map->data.mapping.pairs.start;
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
        int pos = find_pair(doc, cur, tokens[i]);
        if (pos < 0)
            return 0;
        cur = pair_value_id(doc, cur, pos);
    }

    node = yaml_document_get_node(doc, cur);
    if (node == NULL || node->type != YAML_SCALAR_NODE)
        return 0;

    printf("%s\n", (const char *)node->data.scalar.value);
    return 1;
}

int yaml_doc_set(yaml_document_t *doc, const char *path, const char *value) {
    char buf[strlen(path) + 1];
    char *tokens[MAX_PATH_DEPTH];
    int n, i, cur, pos, value_id;
    yaml_node_t *map;

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

    /* Descend through the intermediate keys, creating missing mappings. */
    for (i = 0; i < n - 1; i++) {
        pos = find_pair(doc, cur, tokens[i]);

        if (pos >= 0) {
            int child = pair_value_id(doc, cur, pos);
            yaml_node_t *cn = yaml_document_get_node(doc, child);

            if (cn != NULL && cn->type == YAML_MAPPING_NODE) {
                cur = child;
                continue;
            }

            /* Key exists but is not a mapping: replace it with a fresh one. */
            int new_map =
                yaml_document_add_mapping(doc, NULL, YAML_BLOCK_MAPPING_STYLE);
            if (new_map == 0)
                return 0;
            map = yaml_document_get_node(doc, cur); /* re-fetch after add */
            map->data.mapping.pairs.start[pos].value = new_map;
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

    map = yaml_document_get_node(doc, cur);
    if (map == NULL || map->type != YAML_MAPPING_NODE)
        return 0;

    pos = find_pair(doc, cur, tokens[n - 1]);
    value_id = yaml_document_add_scalar(doc, NULL, (const yaml_char_t *)value,
                                        -1, YAML_ANY_SCALAR_STYLE);
    if (value_id == 0)
        return 0;

    if (pos >= 0) {
        map = yaml_document_get_node(doc, cur); /* re-fetch after add */
        map->data.mapping.pairs.start[pos].value = value_id;
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
    int map_id[MAX_PATH_DEPTH];
    int pair_pos[MAX_PATH_DEPTH];
    int n, i, cur;

    if (yaml_document_get_root_node(doc) == NULL)
        return 0;

    n = split_path(path, buf, tokens, MAX_PATH_DEPTH);
    if (n == 0)
        return 0;

    /* Record the mapping and pair index followed at each level. */
    cur = ROOT_NODE_ID;
    for (i = 0; i < n; i++) {
        int pos = find_pair(doc, cur, tokens[i]);
        if (pos < 0)
            return 0;
        map_id[i] = cur;
        pair_pos[i] = pos;
        cur = pair_value_id(doc, cur, pos);
    }

    /* Remove the target pair, then walk back up removing any mapping that the
     * removal left empty. Each level is a distinct mapping, so the recorded
     * positions in the shallower ones stay valid. The root is never removed. */
    for (i = n - 1; i >= 0; i--) {
        remove_pair(doc, map_id[i], pair_pos[i]);
        if (i == 0 || !mapping_is_empty(doc, map_id[i]))
            break;
    }

    return 1;
}
