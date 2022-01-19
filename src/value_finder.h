#ifndef VALUE_FINDER_H
#define VALUE_FINDER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <yaml.h>

typedef int value_finder_input_event_t(void *data, yaml_event_t *);

typedef int value_finder_output_event_t(void *data, yaml_event_t *);

typedef struct value_finder_s {
    char *last_scalar_value;
    size_t last_scalar_value_size;
    size_t last_scalar_value_length;
    yaml_event_type_t last_event_type;
    char *value_path;
    value_finder_input_event_t *input;
    value_finder_output_event_t *output;
    char *current_block;
    size_t current_block_size;
    size_t current_block_length;
    int scalar_sequence;
} value_finder_t;

int value_finder_init(value_finder_t *);
int value_finder_deinit(value_finder_t *);

#ifdef __cplusplus
}
#endif

#endif /* #ifndef VALUE_FINDER_H */
