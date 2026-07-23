#include "input_data.h"
#include "yaml_doc.h"

#include <stdlib.h>
#include <string.h>
#include <yaml.h>

static void print_parser_error(yaml_parser_t *parser) {
    switch (parser->error) {
    case YAML_MEMORY_ERROR:
        fprintf(stderr, "Memory error: Not enough memory for parsing\n");
        break;
    case YAML_READER_ERROR:
        if (parser->problem_value != -1)
            fprintf(stderr, "Reader error: %s: #%X at %zd\n", parser->problem,
                    parser->problem_value, parser->problem_offset);
        else
            fprintf(stderr, "Reader error: %s at %zd\n", parser->problem,
                    parser->problem_offset);
        break;
    case YAML_SCANNER_ERROR:
    case YAML_PARSER_ERROR:
    case YAML_COMPOSER_ERROR:
        if (parser->context)
            fprintf(
                stderr,
                "Parse error: %s at line %lu, column %lu\n"
                "%s at line %lu, column %lu\n",
                parser->context, (unsigned long)parser->context_mark.line + 1,
                (unsigned long)parser->context_mark.column + 1, parser->problem,
                (unsigned long)parser->problem_mark.line + 1,
                (unsigned long)parser->problem_mark.column + 1);
        else
            fprintf(stderr, "Parse error: %s at line %lu, column %lu\n",
                    parser->problem,
                    (unsigned long)parser->problem_mark.line + 1,
                    (unsigned long)parser->problem_mark.column + 1);
        break;
    default:
        fprintf(stderr, "Internal parser error\n");
        break;
    }
}

static void print_emitter_error(yaml_emitter_t *emitter) {
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
        fprintf(stderr, "Internal emitter error\n");
        break;
    }
}

/* Write `document` to the output file. Takes ownership of the document: it is
 * always destroyed by the time this returns (yaml_emitter_dump deletes it, and
 * the error paths delete it explicitly). Returns 1 on success. */
static int emit_document(yaml_document_t *document, prog_args_t *args) {
    yaml_emitter_t emitter;
    FILE *out;
    int ok = 1;

    out = open_output_file();
    if (out == NULL) {
        yaml_document_delete(document);
        return 0;
    }

    memset(&emitter, 0, sizeof(emitter));
    if (!yaml_emitter_initialize(&emitter)) {
        fprintf(stderr, "Could not initialize the emitter object\n");
        yaml_document_delete(document);
        fclose(out);
        return 0;
    }

    yaml_emitter_set_output_file(&emitter, out);
    yaml_emitter_set_canonical(&emitter, args->canonical);
    yaml_emitter_set_unicode(&emitter, args->unicode);

    if (!yaml_emitter_open(&emitter)) {
        ok = 0;
        yaml_document_delete(document); /* not consumed by dump */
    } else if (!yaml_emitter_dump(&emitter, document)) {
        ok = 0; /* dump destroys the document even on failure */
    } else if (!yaml_emitter_close(&emitter)) {
        ok = 0;
    }

    if (!ok)
        print_emitter_error(&emitter);

    yaml_emitter_delete(&emitter);
    fclose(out);
    return ok;
}

int main(int argc, char *argv[]) {
    yaml_parser_t parser;
    yaml_document_t document;
    prog_args_t *args;
    int changed = 0;
    int result = EXIT_SUCCESS;

    if (!init_input(argc, argv))
        return EXIT_FAILURE;

    args = get_args();

    memset(&parser, 0, sizeof(parser));
    memset(&document, 0, sizeof(document));

    if (!yaml_parser_initialize(&parser)) {
        fprintf(stderr, "Could not initialize the parser object\n");
        deinit_input();
        return EXIT_FAILURE;
    }

    yaml_parser_set_input_file(&parser, args->in_file.file);

    if (!yaml_parser_load(&parser, &document)) {
        print_parser_error(&parser);
        yaml_parser_delete(&parser);
        deinit_input();
        return EXIT_FAILURE;
    }

    yaml_parser_delete(&parser);
    deinit_input(); /* release the input file; output may reuse its path */

    if (args->mode == WORK_GET) {
        changed = yaml_doc_get(&document, args->variable_path);
        yaml_document_delete(&document);
        return changed ? EXIT_SUCCESS : EXIT_FAILURE;
    }

    if (args->mode == WORK_SET)
        changed =
            yaml_doc_set(&document, args->variable_path, args->variable_value);
    else /* WORK_DEL */
        changed = yaml_doc_del(&document, args->variable_path);

    /* An empty document would emit as nothing and clobber the file, so skip
     * the write in that (only reachable on essentially empty input) case. */
    if (yaml_document_get_root_node(&document) == NULL) {
        yaml_document_delete(&document);
        return changed ? EXIT_SUCCESS : EXIT_FAILURE;
    }

    if (!emit_document(&document, args)) /* consumes the document */
        result = EXIT_FAILURE;
    else if (!changed)
        result = EXIT_FAILURE;

    return result;
}
