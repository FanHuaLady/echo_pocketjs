#include "device/framebuffer.h"
#include "device/touchscreen.h"
#include "pocket_runtime.h"
#include "runtime/guest_file.h"

#include <errno.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define TARGET_FRAME_NS 16666667LL
#define FPS_SAMPLE_NS 1000000000LL
#define HOST_STATS_OPCODE_FPS 1

static volatile sig_atomic_t stop_requested = 0;

static void request_stop(int signal_number)
{
    (void)signal_number;
    stop_requested = 1;
}

static int install_signal_handlers(void)
{
    struct sigaction action;

    sigemptyset(&action.sa_mask);
    action.sa_handler = request_stop;
    action.sa_flags = 0;
    if (sigaction(SIGINT, &action, NULL) != 0 ||
        sigaction(SIGTERM, &action, NULL) != 0) {
        perror("sigaction");
        return -1;
    }
    return 0;
}

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

static long long monotonic_ns(void)
{
    struct timespec now;

    if (clock_gettime(CLOCK_MONOTONIC, &now) != 0) {
        return -1;
    }
    return (long long)now.tv_sec * 1000000000LL + now.tv_nsec;
}

static void sleep_until_ns(long long deadline_ns)
{
    while (!stop_requested) {
        long long now_ns = monotonic_ns();
        long long remaining_ns;
        struct timespec remaining;

        if (now_ns < 0 || now_ns >= deadline_ns) {
            break;
        }
        remaining_ns = deadline_ns - now_ns;
        remaining.tv_sec = remaining_ns / 1000000000LL;
        remaining.tv_nsec = remaining_ns % 1000000000LL;
        if (nanosleep(&remaining, NULL) == 0) {
            break;
        }
        if (errno != EINTR) {
            break;
        }
    }
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
    unsigned long frames_presented = 0;
    unsigned long dropped_frames = 0;
    unsigned long sample_frames = 0;
    long long next_frame_ns;
    long long sample_start_ns;
    int previous_touch_down = 0;
    int touch_hit = 0;
    int exit_code = 0;
    int ok;

    if (argc < 2 || argc > 4) {
        fprintf(stderr, "usage: %s guest.js [guest.pak] [frames]\n", argv[0]);
        return 2;
    }
    if (argc == 4 && parse_frame_limit(argv[3], &frame_limit) != 0) {
        fprintf(stderr, "frames must be a non-negative integer\n");
        return 2;
    }
    if (install_signal_handlers() != 0) {
        return 1;
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
        getenv("RV1106_TOUCH_DEVICE"),
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
        exit_code = 1;
        goto cleanup_devices;
    }
    if (!pocket_runtime_harness_bind("__rv1106HostStats")) {
        fprintf(stderr, "PocketJS stats bridge disabled: %s\n", pocket_runtime_error());
    }

    next_frame_ns = monotonic_ns();
    if (next_frame_ns < 0) {
        perror("clock_gettime");
        exit_code = 1;
        goto cleanup_runtime;
    }
    sample_start_ns = next_frame_ns;

    while (!stop_requested &&
           (frame_limit == 0 || frames_presented < frame_limit)) {
        PocketRuntimeInput frame_input = input;
        long long now_ns;

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
            exit_code = 1;
            goto cleanup_runtime;
        }

        rendered = pocket_runtime_render();
        if (rendered == NULL) {
            fprintf(stderr, "PocketJS render failed: %s\n", pocket_runtime_error());
            exit_code = 1;
            goto cleanup_runtime;
        }
        if (framebuffer_present_bgra(
            &framebuffer,
            rendered,
            pocket_runtime_width(),
            pocket_runtime_height(),
            pocket_runtime_stride()
        ) != 0) {
            exit_code = 1;
            goto cleanup_runtime;
        }

        frames_presented++;
        sample_frames++;
        now_ns = monotonic_ns();
        if (now_ns < 0) {
            perror("clock_gettime");
            exit_code = 1;
            goto cleanup_runtime;
        }
        if (now_ns - sample_start_ns >= FPS_SAMPLE_NS) {
            long long elapsed_ns = now_ns - sample_start_ns;
            int fps = (int)((sample_frames * 1000000000ULL +
                             (unsigned long long)elapsed_ns / 2ULL) /
                            (unsigned long long)elapsed_ns);

            if (!pocket_runtime_harness_call(
                    HOST_STATS_OPCODE_FPS,
                    fps,
                    NULL
                )) {
                fprintf(stderr, "PocketJS stats update failed: %s\n", pocket_runtime_error());
                exit_code = 1;
                goto cleanup_runtime;
            }
            sample_frames = 0;
            sample_start_ns = now_ns;
        }

        next_frame_ns += TARGET_FRAME_NS;
        if (now_ns > next_frame_ns) {
            unsigned long missed = (unsigned long)(
                (now_ns - next_frame_ns) / TARGET_FRAME_NS
            ) + 1UL;

            dropped_frames += missed;
            next_frame_ns = now_ns + TARGET_FRAME_NS;
        }
        if (!stop_requested &&
            (frame_limit == 0 || frames_presented < frame_limit)) {
            sleep_until_ns(next_frame_ns);
        }
    }

    printf(
        "PocketJS frames presented: %lu, dropped=%lu, %ux%u stride=%u damage_pixels=%lu\n",
        frames_presented,
        dropped_frames,
        pocket_runtime_width(),
        pocket_runtime_height(),
        pocket_runtime_stride(),
        pocket_runtime_damage_pixels()
    );

cleanup_runtime:
    pocket_runtime_shutdown();

cleanup_devices:
    if (argc >= 3) free(pack);
    framebuffer_close(&framebuffer);
    touchscreen_close(&touchscreen);
    return exit_code;
}
