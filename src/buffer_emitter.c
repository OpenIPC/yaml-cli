#include "buffer_emitter.h"

#define INITIAL_BUFFER_SIZE 10 * 1024 // 10 Kb
#define EXTRA_GROW_SIZE INITIAL_BUFFER_SIZE

static void show_error(yaml_emitter_t *emitter) {
    switch (emitter->error) {
    case YAML_MEMORY_ERROR:
        fprintf(stderr, "Memory error: Not enough memory for emitting\n");
        break;

    case YAML_WRITER_ERROR:
        fprintf(stderr, "Writer error: %s\n", emitter->problem);
        break;

    case YAML_EMITTER_ERROR:
        fprintf(stderr, "Emitter error: %s\n", emitter->problem);
        break;

    default:
        /* Couldn't happen. */
        fprintf(stderr, "Internal error\n");
        break;
    }
}

static int grow_buffer_if_necessary(buffer_emitter_t *e, size_t size) {
    const size_t free_size = (e->buffer_size - e->buffer_pos);

    if (free_size < size) {
        if (e->buffer == NULL) {
            e->buffer_size = size + EXTRA_GROW_SIZE;
            e->buffer = malloc(e->buffer_size);
        } else {
            e->buffer_size += size + EXTRA_GROW_SIZE;
            e->buffer = realloc(e->buffer, e->buffer_size);
        }

        if (e->buffer == NULL)
            return 0;
    }

    return 1;
}

static int write_handler(void *data, unsigned char *buffer, size_t size) {
    buffer_emitter_t *e = (buffer_emitter_t *)data;

    if (!grow_buffer_if_necessary(e, size))
        return 0;

    memcpy(e->buffer + e->buffer_pos, buffer, size);
    e->buffer_pos += size;
    return 1;
}

static int buffer_emitter_input(void *data, yaml_event_t *event) {
    buffer_emitter_t *e = (buffer_emitter_t *)data;
    yaml_emitter_t *emitter = &(e->emitter);

    if (yaml_emitter_emit(emitter, event))
        return 1;

    show_error(emitter);
    e->error = 1;
    return 0;
}

int buffer_emitter_init(buffer_emitter_t *e) {
    yaml_emitter_t *emitter = &(e->emitter);

    memset(emitter, 0, sizeof(*emitter));
    e->input = buffer_emitter_input;

    e->buffer = NULL;
    e->buffer_size = 0;
    e->buffer_pos = 0;
    e->error = 0;

    if (!yaml_emitter_initialize(emitter)) {
        show_error(emitter);
        fprintf(stderr, "Could not inialize the emitter object\n");
        e->error = 1;
        return 0;
    }

    yaml_emitter_set_output(emitter, write_handler, e);
    yaml_emitter_set_canonical(emitter, e->canonical);
    yaml_emitter_set_unicode(emitter, e->unicode);

    if (!yaml_emitter_open(emitter)) {
        show_error(emitter);
        e->error = 1;
        yaml_emitter_delete(emitter);
        return 0;
    }

    if (INITIAL_BUFFER_SIZE > 0) {
        e->buffer_size = INITIAL_BUFFER_SIZE;
        e->buffer = malloc(e->buffer_size);
    }

    return 1;
}

int buffer_emitter_deinit(buffer_emitter_t *e) {
    yaml_emitter_t *emitter = &(e->emitter);

    if (!yaml_emitter_close(emitter)) {
        show_error(emitter);
        return 0;
    }

    yaml_emitter_delete(emitter);
    return 1;
}

int buffer_emitter_error(buffer_emitter_t *e) { return e->error; }
