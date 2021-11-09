#include "input_data.h"
#include "yaml_iterator.h"
#include "buffer_emitter.h"
#include "value_finder.h"

static prog_args_t *g_args = NULL;
static value_finder_t *g_value_finder = NULL;
static int g_value_finded = 0;

static int on_read_input_file(void *data, unsigned char *buffer, size_t size,
                              size_t *size_read) {
    *size_read = fread(buffer, 1, size, g_args->in_file.file);
    return !ferror(g_args->in_file.file);
}

static yaml_iterator_event_result_t on_iterator_event(void *data,
        yaml_event_t *event) {
    buffer_emitter_t *emitter = (buffer_emitter_t *)data;
    yaml_event_t ins_event;
    int g_value_finder_ret = g_value_finder->input(g_value_finder, event);

    if (g_args->mode == WORK_GET) {
        if (g_value_finder_ret == 1) {
            return YAML_ITERATOR_EVENT_STOP;
        }
    }

    if (g_args->mode == WORK_SET) {
        if (g_value_finder_ret == 4) {
            if (!g_value_finded) {
                char value_block[strlen(g_args->variable_path)];
                strcpy(value_block, g_args->variable_path + 1);
                value_block[strstr(value_block + 1, ".") - value_block] = '\0';

                if (!yaml_scalar_event_initialize(&ins_event, NULL, NULL, (yaml_char_t *) value_block, strlen(value_block), 1, 0, YAML_PLAIN_SCALAR_STYLE))
                    return YAML_ITERATOR_EVENT_STOP;

                if (!emitter->input(data, &ins_event))
                    return YAML_ITERATOR_EVENT_STOP;

                if (!yaml_mapping_start_event_initialize(&ins_event, NULL, NULL, 0, YAML_BLOCK_MAPPING_STYLE))
                    return YAML_ITERATOR_EVENT_STOP;

                if (!emitter->input(data, &ins_event)) {
                    return YAML_ITERATOR_EVENT_STOP;
                }
            }
        }
        if ((g_value_finder_ret == 3) ||
                (g_value_finder_ret == 4)) {
            if (!g_value_finded) {

                if (!yaml_scalar_event_initialize(&ins_event, NULL, NULL, (yaml_char_t *) strstr(g_args->variable_path + 1, ".") + 1, strlen(strstr(g_args->variable_path + 1, ".") + 1), 1, 0, YAML_PLAIN_SCALAR_STYLE))
                    return YAML_ITERATOR_EVENT_STOP;

                if (!emitter->input(data, &ins_event)) {
                    return YAML_ITERATOR_EVENT_STOP;
                }
                if (!yaml_scalar_event_initialize(&ins_event, NULL, NULL, (yaml_char_t *) g_args->variable_value, strlen(g_args->variable_value), 1, 0, YAML_PLAIN_SCALAR_STYLE))
                    return YAML_ITERATOR_EVENT_STOP;

                if (!emitter->input(data, &ins_event)) {
                    return YAML_ITERATOR_EVENT_STOP;
                }
                if (g_value_finder_ret == 3)
                    g_value_finded = 1;
            }
        }
        if (g_value_finder_ret == 4) {
            if (!g_value_finded) {

                if (!yaml_mapping_end_event_initialize(&ins_event))
                    return YAML_ITERATOR_EVENT_STOP;

                if (!emitter->input(data, &ins_event)) {
                    return YAML_ITERATOR_EVENT_STOP;
                }
            }
        }
        if (!emitter->input(data, event)) {
            return YAML_ITERATOR_EVENT_STOP;
        }
        return YAML_ITERATOR_EVENT_EATEN;
    }

    if (g_args->mode == WORK_DEL) {
        if ((g_value_finder_ret == 1) ||
                (g_value_finder_ret == 2)) {
            return YAML_ITERATOR_EVENT_EATEN;
        }
        if (!emitter->input(data, event))
            return YAML_ITERATOR_EVENT_STOP;

        return YAML_ITERATOR_EVENT_EATEN;
    }

    return YAML_ITERATOR_EVENT_CONTINUE;
}

static int on_value_finded(void *data,
                           yaml_event_t *event) {
    value_finder_t *f = (value_finder_t *)data;

    if (g_args->mode == WORK_GET) {
        fprintf(stdout, "%s\n", event->data.scalar.value);

        g_value_finded = 1;
        return 1;
    }

    if (g_args->mode == WORK_DEL) {
        g_value_finded = 1;
        return 1;
    }

    g_value_finded = 1;
    free(event->data.scalar.value);
    event->data.scalar.length = strlen(g_args->variable_value);
    event->data.scalar.value = malloc(event->data.scalar.length + 1);
    strncpy((char *)event->data.scalar.value, g_args->variable_value,
            event->data.scalar.length);
    event->data.scalar.value [event->data.scalar.length] = 0;

    return 0;
}

int main(int argc, char *argv[]) {
    yaml_iterator_t iterator;
    buffer_emitter_t emitter;
    value_finder_t value_finder;
    int emitter_ret_code = EXIT_SUCCESS;

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

    g_value_finder = &value_finder;
    value_finder.value_path = g_args->variable_path;
    value_finder.output = on_value_finded;

    if (!value_finder_init(&value_finder))
        return EXIT_FAILURE;

    if (!yaml_iterator_run(&iterator))
        return EXIT_FAILURE;

    if (buffer_emitter_error(&emitter))
        emitter_ret_code = EXIT_FAILURE;

    buffer_emitter_deinit(&emitter);
    value_finder_deinit(&value_finder);
    deinit_input();

    if ((g_args->mode == WORK_SET) ||
            (g_args->mode == WORK_DEL)) {
        if (emitter_ret_code != EXIT_FAILURE) {
            if (!write_to_out_file(emitter.buffer, emitter.buffer_pos))
                return EXIT_FAILURE;
        }
    }

    free(emitter.buffer);

    if (g_args->mode == WORK_GET) {
        return !g_value_finded;
    } else {
        if (emitter_ret_code != EXIT_SUCCESS)
            return EXIT_FAILURE;

        return !g_value_finded;
    }
}
