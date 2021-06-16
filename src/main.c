#include "input_data.h"
#include "yaml_iterator.h"
#include "buffer_emitter.h"
#include "value_finder.h"

static prog_args_t *g_args = NULL;
static value_finder_t *g_value_finder = NULL;

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

    if (!g_value_finder->input(g_value_finder, event))
        return YAML_ITERATOR_EVENT_STOP;

    return YAML_ITERATOR_EVENT_EATEN;
}

static int on_value_finded(void *data,
                           yaml_event_t *event) {
    if (g_args->mode == WORK_GET) {
        fprintf(stdout, "%s", event->data.scalar.value);
        return 0;
    }


    // TODO modify value and return
    int aa = 1;
    aa = 2;

    return 1;
}

int main(int argc, char *argv[]) {
    yaml_iterator_t iterator;
    buffer_emitter_t emitter;
    value_finder_t value_finder;

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

    if (!value_finder_init(&value_finder))
        return EXIT_FAILURE;

    g_value_finder = &value_finder;
    value_finder.value_path = g_args->variable_path;
    value_finder.output = on_value_finded;

    if (!yaml_iterator_run(&iterator))
        return EXIT_FAILURE;

    if (buffer_emitter_error(&emitter))
        ret_code = EXIT_FAILURE;

    buffer_emitter_deinit(&emitter);
    value_finder_deinit(&value_finder);
    deinit_input();

    if (ret_code != EXIT_FAILURE) {
        if (!write_to_out_file(emitter.buffer, emitter.buffer_pos))
            return EXIT_FAILURE;
    }

    free(emitter.buffer);
    return ret_code;
}
