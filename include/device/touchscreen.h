#ifndef POCKETJS_RV1106_TOUCHSCREEN_H
#define POCKETJS_RV1106_TOUCHSCREEN_H

#include <stdint.h>

struct touchscreen {
    int fd;
    int width;
    int height;
    int raw_min_x;
    int raw_max_x;
    int raw_min_y;
    int raw_max_y;
    int uses_multitouch;
    int tracking_id;
    int button_down;
    int raw_x;
    int raw_y;
    int has_x;
    int has_y;
    int down;
    int x;
    int y;
};

int touchscreen_open(
    const char *path,
    int width,
    int height,
    struct touchscreen *touchscreen
);
void touchscreen_close(struct touchscreen *touchscreen);
void touchscreen_poll(struct touchscreen *touchscreen);
int touchscreen_is_down(const struct touchscreen *touchscreen);
int touchscreen_x(const struct touchscreen *touchscreen);
int touchscreen_y(const struct touchscreen *touchscreen);

#endif
