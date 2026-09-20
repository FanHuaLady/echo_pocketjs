#include "device/framebuffer.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>

static int framebuffer_remap(struct framebuffer *framebuffer)
{
    if (framebuffer->memory != NULL &&
        framebuffer->memory != MAP_FAILED) {
        munmap(framebuffer->memory, framebuffer->map_length);
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
        fprintf(stderr, "mmap framebuffer failed: %s\n", strerror(errno));
        framebuffer->memory = NULL;
        framebuffer->map_length = 0;
        return -1;
    }
    return 0;
}

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
    framebuffer->original_variable = framebuffer->variable;
    framebuffer->has_original_variable = 1;
    if (framebuffer_remap(framebuffer) != 0) {
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
    if (framebuffer->fd >= 0 && framebuffer->has_original_variable) {
        struct fb_var_screeninfo restore = framebuffer->original_variable;

        restore.activate = FB_ACTIVATE_NOW;
        ioctl(framebuffer->fd, FBIOPUT_VSCREENINFO, &restore);
    }
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
    framebuffer->has_original_variable = 0;
}

uint32_t framebuffer_buffer_count(const struct framebuffer *framebuffer)
{
    size_t visible_bytes;
    size_t memory_buffers;
    uint32_t virtual_buffers;
    uint32_t buffers;

    if (framebuffer->variable.yres == 0 ||
        framebuffer->fixed.line_length == 0) {
        return 0;
    }
    visible_bytes = (size_t)framebuffer->fixed.line_length *
                    framebuffer->variable.yres;
    if (visible_bytes == 0) {
        return 0;
    }
    memory_buffers = framebuffer->map_length / visible_bytes;
    virtual_buffers = framebuffer->variable.yres_virtual /
                      framebuffer->variable.yres;
    buffers = memory_buffers < virtual_buffers
        ? (uint32_t)memory_buffers
        : virtual_buffers;
    return buffers == 0 ? 1 : buffers;
}

int framebuffer_try_enable_double_buffer(struct framebuffer *framebuffer)
{
    struct fb_var_screeninfo requested;
    size_t required_bytes;

    if (framebuffer->fd < 0) {
        return -1;
    }
    if (framebuffer_buffer_count(framebuffer) >= 2) {
        return 0;
    }

    requested = framebuffer->variable;
    requested.xoffset = 0;
    requested.yoffset = 0;
    requested.yres_virtual = framebuffer->variable.yres * 2U;
    requested.activate = FB_ACTIVATE_NOW;
    if (ioctl(framebuffer->fd, FBIOPUT_VSCREENINFO, &requested) < 0) {
        fprintf(
            stderr,
            "FBIOPUT_VSCREENINFO double buffer failed: %s\n",
            strerror(errno)
        );
        return -1;
    }
    if (ioctl(framebuffer->fd, FBIOGET_FSCREENINFO, &framebuffer->fixed) < 0 ||
        ioctl(framebuffer->fd, FBIOGET_VSCREENINFO, &framebuffer->variable) < 0) {
        fprintf(stderr, "cannot re-query framebuffer: %s\n", strerror(errno));
        return -1;
    }

    required_bytes = (size_t)framebuffer->fixed.line_length *
                     framebuffer->variable.yres * 2U;
    if (framebuffer->variable.yres_virtual < framebuffer->variable.yres * 2U ||
        framebuffer->fixed.smem_len < required_bytes) {
        fprintf(
            stderr,
            "framebuffer double buffer unavailable: yres_virtual=%u smem_len=%u required=%zu\n",
            framebuffer->variable.yres_virtual,
            framebuffer->fixed.smem_len,
            required_bytes
        );
        return -1;
    }

    if (framebuffer_remap(framebuffer) != 0) {
        return -1;
    }
    printf(
        "framebuffer double buffer enabled: yres_virtual=%u buffers=%u\n",
        framebuffer->variable.yres_virtual,
        framebuffer_buffer_count(framebuffer)
    );
    return 0;
}

int framebuffer_fill_rgb(
    struct framebuffer *framebuffer,
    uint32_t buffer_index,
    uint8_t red,
    uint8_t green,
    uint8_t blue
)
{
    uint16_t output = pack_rgb(&framebuffer->variable, red, green, blue);
    uint32_t buffer_count = framebuffer_buffer_count(framebuffer);
    uint32_t y;

    if (buffer_count == 0 || buffer_index >= buffer_count) {
        return -1;
    }
    for (y = 0; y < framebuffer->variable.yres; y++) {
        uint32_t x;
        size_t row_offset = (size_t)(buffer_index * framebuffer->variable.yres + y) *
                            framebuffer->fixed.line_length;
        uint8_t *row = framebuffer->memory + row_offset;

        for (x = 0; x < framebuffer->variable.xres; x++) {
            size_t offset = (size_t)(x + framebuffer->variable.xoffset) * 2U;
            if (row_offset + offset + sizeof(output) > framebuffer->map_length) {
                return -1;
            }
            memcpy(row + offset, &output, sizeof(output));
        }
    }
    return 0;
}

int framebuffer_pan(struct framebuffer *framebuffer, uint32_t buffer_index)
{
    struct fb_var_screeninfo requested = framebuffer->variable;
    uint32_t buffer_count = framebuffer_buffer_count(framebuffer);

    if (buffer_count == 0 || buffer_index >= buffer_count) {
        return -1;
    }
    requested.xoffset = 0;
    requested.yoffset = buffer_index * framebuffer->variable.yres;
    requested.activate = FB_ACTIVATE_VBL;
    if (ioctl(framebuffer->fd, FBIOPAN_DISPLAY, &requested) < 0) {
        requested.activate = FB_ACTIVATE_NOW;
        if (ioctl(framebuffer->fd, FBIOPAN_DISPLAY, &requested) < 0) {
            fprintf(stderr, "FBIOPAN_DISPLAY failed: %s\n", strerror(errno));
            return -1;
        }
    }
    framebuffer->variable.yoffset = requested.yoffset;
    return 0;
}

int framebuffer_sync(struct framebuffer *framebuffer)
{
    if (msync(framebuffer->memory, framebuffer->map_length, MS_SYNC) < 0) {
        fprintf(stderr, "msync framebuffer failed: %s\n", strerror(errno));
        return -1;
    }
    return 0;
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

    return framebuffer_sync(framebuffer);
}
