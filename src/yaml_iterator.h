#ifndef YAML_ITERATOR_H
#define YAML_ITERATOR_H

#ifdef __cplusplus
extern "C" {
#endif

#include <yaml.h>

typedef enum yaml_iterator_event_result_e {
    /** Let the parser choose the encoding. */
    YAML_ITERATOR_EVENT_EATEN,
    YAML_ITERATOR_EVENT_STOP
} yaml_iterator_event_result_t;

typedef yaml_iterator_event_result_t yaml_iterator_event(void *data,
        yaml_event_t *);

typedef struct yaml_iterator_s {
    yaml_read_handler_t *read_handler;
    yaml_iterator_event *event;
    yaml_parser_t parser;
    void *data;
} yaml_iterator_t;

int yaml_iterator_run(struct yaml_iterator_s *);

#ifdef __cplusplus
}
#endif

#endif /* #ifndef YAML_ITERATOR_H */

