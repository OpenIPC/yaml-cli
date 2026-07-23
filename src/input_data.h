#ifndef INPUT_DATA_H
#define INPUT_DATA_H

#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct file_info_s {
    const char *path;
    FILE *file;
} file_info_t;

typedef enum work_mode_e {
    WORK_SET,
    WORK_GET,
    WORK_DEL,
    WORK_UNKNOWN
} work_mode_t;

typedef struct prog_args_s {
    int help;
    int canonical;
    int unicode;
    char *variable_path;
    char *variable_value;
    work_mode_t mode;
    file_info_t in_file;
    file_info_t out_file;
} prog_args_t;

prog_args_t *get_args();

/* Parse argv and open the input file for reading. Returns 1 on success. */
int init_input(int argc, char *argv[]);

/* Close the input file (safe to call more than once). Call this once the
 * document has been fully loaded, so the output file may reuse its path. */
void deinit_input();

/* Open the output file for writing and return it (caller closes). Defaults to
 * the input path when -o was not given. Returns NULL on error. */
FILE *open_output_file();

#ifdef __cplusplus
}
#endif

#endif /* #ifndef INPUT_DATA_H */
