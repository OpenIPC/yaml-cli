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
int init_input(int argc, char *argv[]);
void deinit_input();

int write_to_out_file(char *buffer, size_t buffer_size);

#ifdef __cplusplus
}
#endif

#endif /* #ifndef INPUT_DATA_H */
