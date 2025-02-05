#include "value_finder.h"

#include <stdio.h>

#define INITIAL_BUFFER_SIZE 1024
#define BUFFER_GROW_SIZE INITIAL_BUFFER_SIZE

// #define DEBUG_MESSAGES

static void grow_buffer_if_necessary(char **buf, size_t *size,
                                     size_t data_size) {
    if ((*size) <= data_size) {
        *size = data_size + 1 + BUFFER_GROW_SIZE;
        char *new_buf = realloc(*buf, *size);
        if (new_buf == NULL)
            exit(EXIT_FAILURE);
        *buf = new_buf;
    }
}

static void save_last_scalar(value_finder_t *f, yaml_event_t *event) {
    grow_buffer_if_necessary(&(f->last_scalar_value),
                             &(f->last_scalar_value_size),
                             event->data.scalar.length);

    strncpy(f->last_scalar_value, (const char *)event->data.scalar.value,
            event->data.scalar.length);
    f->last_scalar_value_length = event->data.scalar.length;
    f->last_scalar_value[f->last_scalar_value_length] = 0;
}

static void start_block_handler(value_finder_t *f) {
    if (f->last_event_type == YAML_SCALAR_EVENT) {
        grow_buffer_if_necessary(&(f->current_block), &(f->current_block_size),
                                 f->last_scalar_value_length + 1);

        strcat(f->current_block, ".");
        strcat(f->current_block, f->last_scalar_value);
        f->current_block_length += f->last_scalar_value_length + 1;
    }

#ifdef DEBUG_MESSAGES
    printf("block start: %s\n", f->current_block);
#endif
}

static int end_block_handler(value_finder_t *f) {
    int ret_code = 0;
    char value_block[strlen(f->value_path) + 1];

    strcpy(value_block, f->value_path);

    char *dot_ptr = strstr(value_block + 1, ".");
    if (dot_ptr != NULL)
        value_block[dot_ptr - value_block] = '\0';

    if (!strcmp(f->current_block, value_block))
        ret_code = 1;

#ifdef DEBUG_MESSAGES
    printf("block end: %s\n", f->current_block);
#endif

    for (int i = f->current_block_length - 1; i >= 0; i--) {
        if (f->current_block[i] == '.') {
            f->current_block[i] = 0;
            f->current_block_length = i;
            break;
        }
    }
    return ret_code;
}

static int key_handler(value_finder_t *f, yaml_event_t *event) {
    const size_t saved_cur_block_length = f->current_block_length;
    int ret_code = 0;

    grow_buffer_if_necessary(&(f->current_block), &(f->current_block_size),
                             f->last_scalar_value_length + 1);

    strcat(f->current_block, ".");
    strcat(f->current_block, (const char *)event->data.scalar.value);

    if (!strcmp(f->current_block, f->value_path))
        ret_code = 1;

#ifdef DEBUG_MESSAGES
    printf(".key: %s\n", f->current_block);
#endif

    f->current_block[saved_cur_block_length] = 0;
    return ret_code;
}

static int value_handler(value_finder_t *f) {
    const size_t saved_cur_block_length = f->current_block_length;
    int ret_code = 0;

    grow_buffer_if_necessary(&(f->current_block), &(f->current_block_size),
                             f->last_scalar_value_length + 1);

    strcat(f->current_block, ".");
    strcat(f->current_block, (const char *)f->last_scalar_value);

    if (!strcmp(f->current_block, f->value_path))
        ret_code = 1;

#ifdef DEBUG_MESSAGES
    printf(".block.key: %s\n", f->current_block);
#endif

    f->current_block[saved_cur_block_length] = 0;
    return ret_code;
}

// Here's how it works:
// These events pop up when a new block starts
// YAML_MAPPING_START_EVENT
// YAML_MAPPING_END_EVENT
// If YAML_MAPPING_START_EVENT is preceded by YAML_SCALAR_EVENT, this is the name of the block.
// Further, if there are two YAML_SCALAR_EVENTs in a row, it means it is key: value.

// For each key:value we create a full path in the form .block.block.key
// and thus search for our key

// Return values:
// 0 - nothing found / value changed
// 1 - value found
// 2 - key found
// 3 - end of block found
// 4 - end of all blocks found

static int on_input_event(void *data, yaml_event_t *event) {
    value_finder_t *f = (value_finder_t *)data;
    int ret_code = 0;

    if (event->type != YAML_SCALAR_EVENT)
        f->scalar_sequence = 0;

    if (event->type == YAML_MAPPING_START_EVENT) {
        start_block_handler(f);
        event->data.mapping_start.style = YAML_BLOCK_MAPPING_STYLE;
    } else if (event->type == YAML_MAPPING_END_EVENT) {
        if (end_block_handler(f))
            ret_code = 3;
        if (f->last_event_type == YAML_MAPPING_END_EVENT)
            ret_code = 4;
    } else if (event->type == YAML_SCALAR_EVENT) {

        if (f->scalar_sequence == 0) {
            if (key_handler(f, event))
                ret_code = 2;
        }
        if (f->last_event_type == YAML_SCALAR_EVENT) {
            if (f->scalar_sequence == 1) {
                if (value_handler(f)) {
                    if (f->output(f, event))
                        ret_code = 1;
                }
#ifdef DEBUG_MESSAGES
                printf("value: %s\n", event->data.scalar.value);
#endif
            }
        }

        save_last_scalar(f, event);
        f->scalar_sequence++;

        if (f->scalar_sequence > 1)
            f->scalar_sequence = 0;
    }

    f->last_event_type = event->type;

    return ret_code;
}

int value_finder_init(value_finder_t *f) {
    f->input = on_input_event;

    f->last_scalar_value_size = INITIAL_BUFFER_SIZE;
    f->last_scalar_value = malloc(f->last_scalar_value_size);
    f->last_scalar_value_length = 0;
    f->last_scalar_value[0] = 0;

    f->current_block_size = INITIAL_BUFFER_SIZE;
    f->current_block_length = 0;
    f->current_block = malloc(f->current_block_size);
    f->current_block[0] = 0;

    f->last_event_type = YAML_NO_EVENT;
    f->scalar_sequence = 0;

    return 1;
}

int value_finder_deinit(value_finder_t *f) {
    free(f->last_scalar_value);
    free(f->current_block);
    return 1;
}
