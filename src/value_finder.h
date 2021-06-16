#ifndef VALUE_FINDER_H
#define VALUE_FINDER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <yaml.h>

typedef int value_finder_input_event_t(void *data,
                                       yaml_event_t *);

typedef int value_finder_output_event_t(void *data,
                                        yaml_event_t *);

typedef struct value_finder_s {
    char *value_path;
    int in_value_block;
    value_finder_input_event_t *input;
    value_finder_output_event_t *output;
} value_finder_t;

int value_finder_init(value_finder_t *);
int value_finder_deinit(value_finder_t *);

#ifdef __cplusplus
}
#endif

#endif /* #ifndef VALUE_FINDER_H */

