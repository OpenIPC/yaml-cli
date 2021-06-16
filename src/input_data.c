#include "input_data.h"

#include <string.h>

static prog_args_t g_args;

static const char *handle_file_param(int *k, int argc, char *argv[]) {
    if ((*k + 1) >= argc) {
        fprintf(stderr, "%s parameter must be used with file path\n", argv[*k]);
        return NULL;
    }

    return argv[++(*k)];
}

static int parse_args(int argc, char *argv[]) {
    int k = 0;

    memset(&g_args, 0, sizeof(g_args));

    for (k = 1; k < argc; k ++) {
        if (strcmp(argv[k], "-h") == 0
            || strcmp(argv[k], "--help") == 0) {
            g_args.help = 1;
            break;
        } else if (strcmp(argv[k], "-c") == 0
                   || strcmp(argv[k], "--canonical") == 0) {
            g_args.canonical = 1;
        } else if (strcmp(argv[k], "-u") == 0
                   || strcmp(argv[k], "--unicode") == 0) {
            g_args.unicode = 1;
        } else if (strcmp(argv[k], "-i") == 0
                   || strcmp(argv[k], "--input") == 0) {
            g_args.in_file.path = handle_file_param(&k, argc, argv);

            if (g_args.in_file.path == NULL)
                return 0;
        } else if (strcmp(argv[k], "-o") == 0
                   || strcmp(argv[k], "--output") == 0) {
            g_args.out_file.path = handle_file_param(&k, argc, argv);

            if (g_args.out_file.path == NULL)
                return 0;
        } else {
            fprintf(stderr, "Unrecognized option: %s\n"
                    "Try `%s --help` for more information.\n",
                    argv[k], argv[0]);
            return 0;
        }
    }

    /* Display the help string. */

    if (g_args.help) {
        printf("%s <input\n"
               "or\n%s -h | --help\nDeconstruct a YAML stream\n\nOptions:\n"
               "-h, --help\t\tdisplay this help and exit\n"
               "-c, --canonical\t\toutput in the canonical YAML format\n"
               "-u, --unicode\t\toutput unescaped non-ASCII characters\n",
               argv[0], argv[0]);
        return 0;
    }

    if (g_args.in_file.path == NULL) {
        fprintf(stderr, "No input file, use -i <file_path>\n");
        return 0;
    }

    return 1;
}

static FILE *open_file(const char *file_type, const char *path,
                       const char *modes) {
    FILE *file = NULL;
    file = fopen(path, modes);

    if (path == NULL)
        printf("Failed to open %s file %s\n", file_type, path);

    return file;
}

static int open_input_file() {
    g_args.in_file.file = open_file("input", g_args.in_file.path, "r");
    return g_args.in_file.file != NULL;
}

static int open_output_file() {
    g_args.out_file.file = open_file("output", g_args.out_file.path, "w");
    return g_args.out_file.file != NULL;
}

prog_args_t *get_args() {
    return &g_args;
}

int init_input(int argc, char *argv[]) {
    if (!parse_args(argc, argv))
        return 0;

    if (!open_input_file())
        return 0;

    return 1;
}

void deinit_input() {
    if (g_args.in_file.file != NULL)
        fclose(g_args.in_file.file);
}

int write_to_out_file(char *buffer, size_t buffer_size) {
    int ret_code = 1;

    if (g_args.out_file.path == NULL)
        g_args.out_file.path = g_args.in_file.path;

    if (!open_output_file())
        return 0;

    if ((fwrite(buffer, buffer_size, 1, g_args.out_file.file)) != 1) {
        printf("File write error....\n");
        ret_code = 0;
    }

    fclose(g_args.out_file.file);
    return ret_code;
}
