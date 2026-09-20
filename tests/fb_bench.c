#include "device/framebuffer.h"

#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define REPORT_INTERVAL_NS 1000000000LL

enum bench_mode {
    BenchAuto,
    BenchSingle,
    BenchSingleMsync,
    BenchPan,
    BenchPanMsync,
};

struct color {
    uint8_t red;
    uint8_t green;
    uint8_t blue;
};

static const struct color colors[] = {
    {255, 0, 0},
    {0, 255, 0},
    {0, 0, 255},
    {255, 255, 255},
    {0, 0, 0},
};

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

static long long monotonic_ns(void)
{
    struct timespec now;

    if (clock_gettime(CLOCK_MONOTONIC, &now) != 0) {
        return -1;
    }
    return (long long)now.tv_sec * 1000000000LL + now.tv_nsec;
}

static int parse_mode(const char *text, enum bench_mode *mode)
{
    if (strcmp(text, "auto") == 0) {
        *mode = BenchAuto;
    } else if (strcmp(text, "single") == 0) {
        *mode = BenchSingle;
    } else if (strcmp(text, "single-msync") == 0) {
        *mode = BenchSingleMsync;
    } else if (strcmp(text, "pan") == 0) {
        *mode = BenchPan;
    } else if (strcmp(text, "pan-msync") == 0) {
        *mode = BenchPanMsync;
    } else {
        return -1;
    }
    return 0;
}

static int parse_seconds(const char *text, unsigned long *seconds)
{
    char *end;
    unsigned long value;

    value = strtoul(text, &end, 10);
    if (text[0] == '\0' || *end != '\0') {
        return -1;
    }
    *seconds = value;
    return 0;
}

static const char *mode_name(enum bench_mode mode)
{
    switch (mode) {
    case BenchAuto:
        return "auto";
    case BenchSingle:
        return "single";
    case BenchSingleMsync:
        return "single-msync";
    case BenchPan:
        return "pan";
    case BenchPanMsync:
        return "pan-msync";
    }
    return "unknown";
}

static int mode_uses_msync(enum bench_mode mode)
{
    return mode == BenchSingleMsync || mode == BenchPanMsync;
}

static int mode_uses_pan(enum bench_mode mode)
{
    return mode == BenchPan || mode == BenchPanMsync;
}

static void print_usage(const char *program)
{
    fprintf(
        stderr,
        "usage: %s [auto|single|single-msync|pan|pan-msync] [seconds]\n",
        program
    );
}

int main(int argc, char **argv)
{
    struct framebuffer framebuffer = {
        .fd = -1,
        .map_length = 0,
        .memory = NULL,
    };
    enum bench_mode mode = BenchAuto;
    unsigned long seconds = 0;
    unsigned long long frames = 0;
    unsigned long long interval_frames = 0;
    long long start_ns;
    long long report_ns;
    long long end_ns = 0;
    int pan_enabled = 0;
    uint32_t draw_buffer = 0;

    if (argc > 3 ||
        (argc >= 2 && parse_mode(argv[1], &mode) != 0) ||
        (argc == 3 && parse_seconds(argv[2], &seconds) != 0)) {
        print_usage(argv[0]);
        return 2;
    }
    if (install_signal_handlers() != 0) {
        return 1;
    }
    if (framebuffer_open("/dev/fb0", &framebuffer) != 0) {
        return 1;
    }

    if (mode == BenchAuto || mode_uses_pan(mode)) {
        pan_enabled = framebuffer_try_enable_double_buffer(&framebuffer) == 0 &&
                      framebuffer_buffer_count(&framebuffer) >= 2;
        if (!pan_enabled && mode_uses_pan(mode)) {
            framebuffer_close(&framebuffer);
            return 1;
        }
        if (mode == BenchAuto) {
            mode = pan_enabled ? BenchPan : BenchSingle;
        }
    }

    printf(
        "fb_bench mode=%s msync=%s pan=%s buffers=%u seconds=%lu\n",
        mode_name(mode),
        mode_uses_msync(mode) ? "yes" : "no",
        mode_uses_pan(mode) ? "yes" : "no",
        framebuffer_buffer_count(&framebuffer),
        seconds
    );

    start_ns = monotonic_ns();
    if (start_ns < 0) {
        perror("clock_gettime");
        framebuffer_close(&framebuffer);
        return 1;
    }
    report_ns = start_ns;
    if (seconds != 0) {
        end_ns = start_ns + (long long)seconds * REPORT_INTERVAL_NS;
    }

    while (!stop_requested) {
        const struct color *color = &colors[frames % (sizeof(colors) / sizeof(colors[0]))];
        long long now_ns;

        if (mode_uses_pan(mode)) {
            draw_buffer = draw_buffer == 0 ? 1 : 0;
        } else {
            draw_buffer = 0;
        }
        if (framebuffer_fill_rgb(
                &framebuffer,
                draw_buffer,
                color->red,
                color->green,
                color->blue
            ) != 0) {
            framebuffer_close(&framebuffer);
            return 1;
        }
        if (mode_uses_msync(mode) && framebuffer_sync(&framebuffer) != 0) {
            framebuffer_close(&framebuffer);
            return 1;
        }
        if (mode_uses_pan(mode) &&
            framebuffer_pan(&framebuffer, draw_buffer) != 0) {
            framebuffer_close(&framebuffer);
            return 1;
        }

        frames++;
        interval_frames++;
        now_ns = monotonic_ns();
        if (now_ns < 0) {
            perror("clock_gettime");
            framebuffer_close(&framebuffer);
            return 1;
        }
        if (now_ns - report_ns >= REPORT_INTERVAL_NS) {
            double fps = (double)interval_frames * 1000000000.0 /
                         (double)(now_ns - report_ns);

            printf("fps=%.1f total=%llu\n", fps, frames);
            fflush(stdout);
            interval_frames = 0;
            report_ns = now_ns;
        }
        if (end_ns != 0 && now_ns >= end_ns) {
            break;
        }
    }

    framebuffer_close(&framebuffer);
    return 0;
}
