#include "yaml_iterator.h"

static void show_parser_error(yaml_parser_t *parser) {
    switch (parser->error) {
    case YAML_MEMORY_ERROR:
        fprintf(stderr, "Memory error: Not enough memory for parsing\n");
        break;

    case YAML_READER_ERROR:
        if (parser->problem_value != -1) {
            fprintf(stderr, "Reader error: %s: #%X at %zd\n", parser->problem,
                    parser->problem_value, parser->problem_offset);
        } else {
            fprintf(stderr, "Reader error: %s at %zd\n", parser->problem,
                    parser->problem_offset);
        }
        break;

    case YAML_SCANNER_ERROR:
        if (parser->context) {
            fprintf(stderr,
                    "Scanner error: %s at line %lu, column %lu\n"
                    "%s at line %lu, column %lu\n",
                    parser->context, parser->context_mark.line + 1,
                    parser->context_mark.column + 1, parser->problem,
                    parser->problem_mark.line + 1,
                    parser->problem_mark.column + 1);
        } else {
            fprintf(stderr, "Scanner error: %s at line %lu, column %lu\n",
                    parser->problem, parser->problem_mark.line + 1,
                    parser->problem_mark.column + 1);
        }
        break;

    case YAML_PARSER_ERROR:
        if (parser->context) {
            fprintf(stderr,
                    "Parser error: %s at line %lu, column %lu\n"
                    "%s at line %lu, column %lu\n",
                    parser->context, parser->context_mark.line + 1,
                    parser->context_mark.column + 1, parser->problem,
                    parser->problem_mark.line + 1,
                    parser->problem_mark.column + 1);
        } else {
            fprintf(stderr, "Parser error: %s at line %lu, column %lu\n",
                    parser->problem, parser->problem_mark.line + 1,
                    parser->problem_mark.column + 1);
        }
        break;

    default:
        /* Couldn't happen. */
        fprintf(stderr, "Internal error\n");
        break;
    }
}

static void close_parser(yaml_parser_t *p, int error) {
    if (error)
        show_parser_error(p);

    yaml_parser_delete(p);
}

int yaml_iterator_run(struct yaml_iterator_s *i) {
    yaml_parser_t *parser = &(i->parser);
    yaml_event_t input_event;
    int document_opened = 0;
    int done = 0;
    int event_eaten = 0;
    yaml_iterator_event_result_t event_result;

    memset(parser, 0, sizeof(*parser));
    memset(&input_event, 0, sizeof(input_event));

    if (!yaml_parser_initialize(parser)) {
        fprintf(stderr, "Could not initialize the parser object\n");
        return 0;
    }

    yaml_parser_set_input(parser, i->read_handler, NULL);

    while (!done) {
        if (!yaml_parser_parse(parser, &input_event)) {
            close_parser(parser, 1);
            return 0;
        }

        if (input_event.type == YAML_STREAM_END_EVENT) {
            done = 1;
        } else if (input_event.type == YAML_DOCUMENT_START_EVENT) {
            document_opened = 1;
        }

        event_eaten = 0;

        if (document_opened) {
            event_result = i->event(i->data, &input_event);

            if (event_result == YAML_ITERATOR_EVENT_EATEN)
                event_eaten = 1;
            else if (event_result == YAML_ITERATOR_EVENT_STOP)
                done = 1;

            if (input_event.type == YAML_DOCUMENT_END_EVENT) {
                document_opened = 0;
            }
        }

        if (!event_eaten)
            yaml_event_delete(&input_event);
    }

    close_parser(parser, 0);
    return 1;
}
