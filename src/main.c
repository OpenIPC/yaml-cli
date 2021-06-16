#include "yaml_iterator.h"
#include "input_data.h"
#include "buffer_emitter.h"

static prog_args_t *g_args = NULL;

static int on_read_input_file(void *data, unsigned char *buffer, size_t size,
                              size_t *size_read) {
    *size_read = fread(buffer, 1, size, g_args->in_file.file);
    return !ferror(g_args->in_file.file);
}

static yaml_iterator_event_result_t on_iterator_event(void *data,
        yaml_event_t *event) {
    buffer_emitter_t *emitter = (buffer_emitter_t *)data;
    if (!emitter->input(data, event))
        return YAML_ITERATOR_EVENT_STOP;

    return YAML_ITERATOR_EVENT_EATEN;
}

int main(int argc, char *argv[]) {
    yaml_iterator_t iterator;
    buffer_emitter_t emitter;
    int ret_code = EXIT_SUCCESS;

    if (!init_input(argc, argv))
        return EXIT_FAILURE;

    g_args = get_args();

    iterator.event = on_iterator_event;
    iterator.data = &emitter;
    iterator.read_handler = on_read_input_file;

    emitter.canonical = g_args->canonical;
    emitter.unicode = g_args->unicode;

    if (!buffer_emitter_init(&emitter))
        return EXIT_FAILURE;

    if (!yaml_iterator_run(&iterator))
        return EXIT_FAILURE;

    if (buffer_emitter_error(&emitter))
        ret_code = EXIT_FAILURE;

    buffer_emitter_deinit(&emitter);
    deinit_input();

    if (ret_code != EXIT_FAILURE) {
        if (!write_to_out_file(emitter.buffer, emitter.buffer_pos))
            return EXIT_FAILURE;
    }

    free(emitter.buffer);
    return ret_code;
}
