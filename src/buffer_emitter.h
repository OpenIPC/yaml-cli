#ifndef BUFFER_EMITTER_H
#define BUFFER_EMITTER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <yaml.h>

typedef int buffer_emitter_input_t(void *data, yaml_event_t *);

typedef struct buffer_emitter_s {
    char *buffer;
    size_t buffer_pos;
    size_t buffer_size;
    buffer_emitter_input_t *input;
    yaml_emitter_t emitter;
    int canonical;
    int unicode;
    int error;
} buffer_emitter_t;

int buffer_emitter_init(buffer_emitter_t *);
int buffer_emitter_deinit(buffer_emitter_t *);
int buffer_emitter_error(buffer_emitter_t *);

#ifdef __cplusplus
}
#endif

#endif /* #ifndef BUFFER_EMITTER_H */
