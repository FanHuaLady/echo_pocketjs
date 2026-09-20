#include "device/framebuffer.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>

static uint32_t scale_channel(
    uint8_t value,
    const struct fb_bitfield *field
)
{
    uint32_t maximum;

    if (field->length == 0) {
        return 0;
    }
    maximum = (1U << field->length) - 1U;
    return ((uint32_t)value * maximum / 255U) << field->offset;
}

static uint16_t pack_rgb(
    const struct fb_var_screeninfo *variable,
    uint8_t red,
    uint8_t green,
    uint8_t blue
)
{
    uint32_t pixel = scale_channel(red, &variable->red);
    pixel |= scale_channel(green, &variable->green);
    pixel |= scale_channel(blue, &variable->blue);
    return (uint16_t)pixel;
}

int framebuffer_open(const char *path, struct framebuffer *framebuffer)
{
    framebuffer->fd = open(path, O_RDWR);
    if (framebuffer->fd < 0) {
        fprintf(stderr, "cannot open %s: %s\n", path, strerror(errno));
        return -1;
    }
    if (ioctl(
        framebuffer->fd,
        FBIOGET_FSCREENINFO,
        &framebuffer->fixed
    ) < 0 ||
        ioctl(
            framebuffer->fd,
            FBIOGET_VSCREENINFO,
            &framebuffer->variable
        ) < 0) {
        fprintf(stderr, "cannot query %s: %s\n", path, strerror(errno));
        close(framebuffer->fd);
        framebuffer->fd = -1;
        return -1;
    }
    if (framebuffer->variable.bits_per_pixel != 16 ||
        framebuffer->fixed.smem_len == 0) {
        fprintf(stderr, "unsupported framebuffer format\n");
        close(framebuffer->fd);
        framebuffer->fd = -1;
        return -1;
    }
    framebuffer->map_length = framebuffer->fixed.smem_len;
    framebuffer->memory = mmap(
        NULL,
        framebuffer->map_length,
        PROT_READ | PROT_WRITE,
        MAP_SHARED,
        framebuffer->fd,
        0
    );
    if (framebuffer->memory == MAP_FAILED) {
        fprintf(stderr, "mmap %s failed: %s\n", path, strerror(errno));
        close(framebuffer->fd);
        framebuffer->fd = -1;
        return -1;
    }
    printf(
        "framebuffer opened: %s %ux%u %ubpp stride=%u\n",
        path,
        framebuffer->variable.xres,
        framebuffer->variable.yres,
        framebuffer->variable.bits_per_pixel,
        framebuffer->fixed.line_length
    );
    return 0;
}

void framebuffer_close(struct framebuffer *framebuffer)
{
    if (framebuffer->memory != NULL &&
        framebuffer->memory != MAP_FAILED) {
        munmap(framebuffer->memory, framebuffer->map_length);
    }
    if (framebuffer->fd >= 0) {
        close(framebuffer->fd);
    }
    framebuffer->memory = NULL;
    framebuffer->fd = -1;
    framebuffer->map_length = 0;
}

int framebuffer_present_bgra(
    struct framebuffer *framebuffer,
    const uint8_t *bgra,
    uint32_t width,
    uint32_t height,
    uint32_t stride
)
{
    uint32_t copy_width = width < framebuffer->variable.xres
        ? width
        : framebuffer->variable.xres;
    uint32_t copy_height = height < framebuffer->variable.yres
        ? height
        : framebuffer->variable.yres;
    uint32_t y;

    for (y = 0; y < copy_height; y++) {
        uint32_t x;
        size_t row_offset = (size_t)(y + framebuffer->variable.yoffset) *
                            framebuffer->fixed.line_length;
        uint8_t *row = framebuffer->memory + row_offset;
        const uint8_t *source = bgra + (size_t)y * stride;

        for (x = 0; x < copy_width; x++) {
            const uint8_t *pixel = source + (size_t)x * 4U;
            uint16_t output = pack_rgb(
                &framebuffer->variable,
                pixel[2],
                pixel[1],
                pixel[0]
            );
            size_t offset = (size_t)(x + framebuffer->variable.xoffset) * 2U;
            if (row_offset + offset + sizeof(output) > framebuffer->map_length) {
                return -1;
            }
            memcpy(row + offset, &output, sizeof(output));
        }
    }

    if (msync(framebuffer->memory, framebuffer->map_length, MS_SYNC) < 0) {
        fprintf(stderr, "msync framebuffer failed: %s\n", strerror(errno));
        return -1;
    }
    return 0;
}
