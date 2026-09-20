#ifndef POCKETJS_RV1106_FRAMEBUFFER_H
#define POCKETJS_RV1106_FRAMEBUFFER_H

#include <linux/fb.h>
#include <stddef.h>
#include <stdint.h>

struct framebuffer {
    int fd;
    size_t map_length;
    uint8_t *memory;
    struct fb_fix_screeninfo fixed;
    struct fb_var_screeninfo variable;
};

int framebuffer_open(const char *path, struct framebuffer *framebuffer);
void framebuffer_close(struct framebuffer *framebuffer);
int framebuffer_present_bgra(
    struct framebuffer *framebuffer,
    const uint8_t *bgra,
    uint32_t width,
    uint32_t height,
    uint32_t stride
);

#endif
