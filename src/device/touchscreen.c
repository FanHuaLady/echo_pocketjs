#include "device/touchscreen.h"

#include <errno.h>
#include <fcntl.h>
#include <linux/input.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

static int read_abs_range(
    int fd,
    unsigned int code,
    int *minimum,
    int *maximum
)
{
    struct input_absinfo info;

    if (ioctl(fd, EVIOCGABS(code), &info) < 0 || info.maximum <= info.minimum) {
        return -1;
    }
    *minimum = info.minimum;
    *maximum = info.maximum;
    return 0;
}

static int has_abs_code(int fd, unsigned int code)
{
    struct input_absinfo info;

    return ioctl(fd, EVIOCGABS(code), &info) == 0;
}

static int clamp(int value, int minimum, int maximum)
{
    if (value < minimum) return minimum;
    if (value > maximum) return maximum;
    return value;
}

static int map_coordinate(
    int value,
    int raw_minimum,
    int raw_maximum,
    int output_size
)
{
    long long numerator;
    int bounded;

    if (output_size <= 1 || raw_maximum <= raw_minimum) {
        return 0;
    }
    bounded = clamp(value, raw_minimum, raw_maximum);
    numerator = (long long)(bounded - raw_minimum) * (output_size - 1);
    return (int)(numerator / (raw_maximum - raw_minimum));
}

int touchscreen_open(
    const char *path,
    int width,
    int height,
    struct touchscreen *touchscreen
)
{
    int mt_min_x;
    int mt_max_x;
    int mt_min_y;
    int mt_max_y;

    memset(touchscreen, 0, sizeof(*touchscreen));
    touchscreen->fd = open(path, O_RDONLY | O_NONBLOCK);
    if (touchscreen->fd < 0) {
        fprintf(stderr, "cannot open touchscreen %s: %s\n", path, strerror(errno));
        return -1;
    }

    touchscreen->width = width;
    touchscreen->height = height;
    touchscreen->tracking_id = -1;

    if (read_abs_range(
        touchscreen->fd,
        ABS_MT_POSITION_X,
        &mt_min_x,
        &mt_max_x
    ) == 0 &&
        read_abs_range(
            touchscreen->fd,
            ABS_MT_POSITION_Y,
            &mt_min_y,
            &mt_max_y
        ) == 0 &&
        has_abs_code(touchscreen->fd, ABS_MT_TRACKING_ID)) {
        touchscreen->raw_min_x = mt_min_x;
        touchscreen->raw_max_x = mt_max_x;
        touchscreen->raw_min_y = mt_min_y;
        touchscreen->raw_max_y = mt_max_y;
        touchscreen->uses_multitouch = 1;
    } else if (
        read_abs_range(
            touchscreen->fd,
            ABS_X,
            &touchscreen->raw_min_x,
            &touchscreen->raw_max_x
        ) != 0 ||
        read_abs_range(
            touchscreen->fd,
            ABS_Y,
            &touchscreen->raw_min_y,
            &touchscreen->raw_max_y
        ) != 0) {
        fprintf(stderr, "cannot query touchscreen coordinate ranges\n");
        close(touchscreen->fd);
        touchscreen->fd = -1;
        return -1;
    }

    printf(
        "touchscreen opened: %s protocol=%s raw_x=[%d,%d] raw_y=[%d,%d] logical=%dx%d\n",
        path,
        touchscreen->uses_multitouch ? "multitouch" : "single-touch",
        touchscreen->raw_min_x,
        touchscreen->raw_max_x,
        touchscreen->raw_min_y,
        touchscreen->raw_max_y,
        width,
        height
    );
    return 0;
}

void touchscreen_close(struct touchscreen *touchscreen)
{
    if (touchscreen->fd >= 0) {
        close(touchscreen->fd);
    }
    touchscreen->fd = -1;
    touchscreen->down = 0;
}

void touchscreen_poll(struct touchscreen *touchscreen)
{
    struct input_event event;
    ssize_t size;

    if (touchscreen->fd < 0) return;

    for (;;) {
        size = read(touchscreen->fd, &event, sizeof(event));
        if (size < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) break;
            if (errno == EINTR) continue;
            break;
        }
        if (size != (ssize_t)sizeof(event)) break;

        if (event.type == EV_ABS) {
            switch (event.code) {
            case ABS_MT_TRACKING_ID:
                touchscreen->tracking_id = event.value;
                break;
            case ABS_MT_POSITION_X:
            case ABS_X:
                touchscreen->raw_x = event.value;
                touchscreen->has_x = 1;
                break;
            case ABS_MT_POSITION_Y:
            case ABS_Y:
                touchscreen->raw_y = event.value;
                touchscreen->has_y = 1;
                break;
            default:
                break;
            }
        } else if (event.type == EV_KEY && event.code == BTN_TOUCH) {
            touchscreen->button_down = event.value != 0;
        } else if (event.type == EV_SYN && event.code == SYN_REPORT) {
            if (touchscreen->uses_multitouch) {
                touchscreen->down = touchscreen->tracking_id >= 0;
            } else {
                touchscreen->down = touchscreen->button_down;
            }
            if (touchscreen->down && touchscreen->has_x && touchscreen->has_y) {
                touchscreen->x = map_coordinate(
                    touchscreen->raw_x,
                    touchscreen->raw_min_x,
                    touchscreen->raw_max_x,
                    touchscreen->width
                );
                touchscreen->y = map_coordinate(
                    touchscreen->raw_y,
                    touchscreen->raw_min_y,
                    touchscreen->raw_max_y,
                    touchscreen->height
                );
            }
        }
    }
}

int touchscreen_is_down(const struct touchscreen *touchscreen)
{
    return touchscreen->down;
}

int touchscreen_x(const struct touchscreen *touchscreen)
{
    return touchscreen->x;
}

int touchscreen_y(const struct touchscreen *touchscreen)
{
    return touchscreen->y;
}
