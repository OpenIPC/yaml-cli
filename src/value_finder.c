#include "value_finder.h"

static int on_input_event(void *data,
                          yaml_event_t *event) {
    value_finder_t *f = (value_finder_t *)data;

    // YAML_SCALAR_EVENT - имя блока
    // потом следует YAML_MAPPING_START_EVENT
    // переменные и саб-блоки
    // переменные в виде YAML_SCALAR_EVENT : YAML_SCALAR_EVENT
    // потом следует YAML_MAPPING_END_EVENT

    int aa = 1;
    aa = 2;
    //    f->value_path;

}


int value_finder_init(value_finder_t *f) {
    f->input = on_input_event;
    return 1;
}

int value_finder_deinit(value_finder_t *) {
    return 1;
}
