#include "device/framebuffer.h"
#include "device/touchscreen.h"
#include "pocket_runtime.h"
#include "runtime/guest_file.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

static int parse_frame_limit(const char *text, unsigned long *limit)
{
    char *end;
    unsigned long value;

    value = strtoul(text, &end, 10);
    if (text[0] == '\0' || *end != '\0') {
        return -1;
    }
    *limit = value;
    return 0;
}

static void wait_for_next_frame(void)
{
    struct timespec interval = {
        .tv_sec = 0,
        .tv_nsec = 16666667L,
    };
    nanosleep(&interval, NULL);
}

int main(int argc, char **argv)
{
    static const uint8_t empty_pack[1] = {0};
    struct framebuffer framebuffer = {
        .fd = -1,
        .map_length = 0,
        .memory = NULL,
    };
    struct touchscreen touchscreen = {
        .fd = -1,
    };
    PocketRuntimeInput input = {
        .buttons = 0,
        .touch_down = 0,
        .touch_x = 0,
        .touch_y = 0,
        .touch_hit = 0,
    };
    const uint8_t *rendered;
    uint8_t *script;
    uint8_t *pack = (uint8_t *)empty_pack;
    size_t script_length;
    size_t pack_length = 0;
    unsigned long frame_limit = 0;
    unsigned long frame_index;
    int previous_touch_down = 0;
    int touch_hit = 0;
    int ok;

    if (argc < 2 || argc > 4) {
        fprintf(stderr, "usage: %s guest.js [guest.pak] [frames]\n", argv[0]);
        return 2;
    }
    if (argc == 4 && parse_frame_limit(argv[3], &frame_limit) != 0) {
        fprintf(stderr, "frames must be a non-negative integer\n");
        return 2;
    }

    script = guest_file_read(argv[1], &script_length);
    if (script == NULL) {
        return 1;
    }
    if (argc >= 3) {
        pack = guest_file_read(argv[2], &pack_length);
        if (pack == NULL) {
            free(script);
            return 1;
        }
    }
    if (framebuffer_open("/dev/fb0", &framebuffer) != 0) {
        free(script);
        if (argc >= 3) free(pack);
        return 1;
    }
    if (touchscreen_open(
        getenv("RV1106_TOUCH_DEVICE") != NULL
            ? getenv("RV1106_TOUCH_DEVICE")
            : "/dev/input/event0",
        (int)framebuffer.variable.xres,
        (int)framebuffer.variable.yres,
        &touchscreen
    ) != 0) {
        fprintf(stderr, "touchscreen input disabled\n");
    }

    ok = pocket_runtime_boot(
        (const char *)script,
        script_length,
        pack,
        pack_length,
        (int)framebuffer.variable.xres,
        (int)framebuffer.variable.yres
    );
    free(script);
    if (!ok) {
        fprintf(stderr, "PocketJS boot failed: %s\n", pocket_runtime_error());
        if (argc >= 3) free(pack);
        framebuffer_close(&framebuffer);
        touchscreen_close(&touchscreen);
        return 1;
    }

    for (frame_index = 0;
         frame_limit == 0 || frame_index < frame_limit;
         frame_index++) {
        PocketRuntimeInput frame_input = input;

        touchscreen_poll(&touchscreen);
        frame_input.touch_down = touchscreen_is_down(&touchscreen);
        frame_input.touch_x = touchscreen_x(&touchscreen);
        frame_input.touch_y = touchscreen_y(&touchscreen);
        if (frame_input.touch_down && !previous_touch_down) {
            touch_hit = pocket_runtime_hit_test_bounds(
                (float)frame_input.touch_x,
                (float)frame_input.touch_y
            );
            frame_input.touch_hit = touch_hit;
        } else if (frame_input.touch_down) {
            frame_input.touch_hit = touch_hit;
        } else if (!frame_input.touch_down) {
            touch_hit = 0;
            frame_input.touch_hit = 0;
        }
        previous_touch_down = frame_input.touch_down;

        ok = pocket_runtime_tick(&frame_input);
        if (!ok) {
            fprintf(stderr, "PocketJS tick failed: %s\n", pocket_runtime_error());
            pocket_runtime_shutdown();
            if (argc >= 3) free(pack);
            framebuffer_close(&framebuffer);
            touchscreen_close(&touchscreen);
            return 1;
        }

        rendered = pocket_runtime_render();
        if (rendered == NULL) {
            fprintf(stderr, "PocketJS render failed: %s\n", pocket_runtime_error());
            pocket_runtime_shutdown();
            if (argc >= 3) free(pack);
            framebuffer_close(&framebuffer);
            touchscreen_close(&touchscreen);
            return 1;
        }
        if (framebuffer_present_bgra(
            &framebuffer,
            rendered,
            pocket_runtime_width(),
            pocket_runtime_height(),
            pocket_runtime_stride()
        ) != 0) {
            pocket_runtime_shutdown();
            if (argc >= 3) free(pack);
            framebuffer_close(&framebuffer);
            touchscreen_close(&touchscreen);
            return 1;
        }

        if (frame_limit == 0 || frame_index + 1 < frame_limit) {
            wait_for_next_frame();
        }
    }

    printf(
        "PocketJS frames presented: %lu, %ux%u stride=%u damage_pixels=%lu\n",
        frame_limit == 0 ? frame_index : frame_limit,
        pocket_runtime_width(),
        pocket_runtime_height(),
        pocket_runtime_stride(),
        pocket_runtime_damage_pixels()
    );
    pocket_runtime_shutdown();
    if (argc >= 3) free(pack);
    framebuffer_close(&framebuffer);
    touchscreen_close(&touchscreen);
    return 0;
}
