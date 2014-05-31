#ifndef _YAGL_OFFSCREEN_IMAGE_PIXMAP_H_
#define _YAGL_OFFSCREEN_IMAGE_PIXMAP_H_

#include "yagl_export.h"
#include "yagl_types.h"
#include "yagl_image.h"

struct yagl_native_drawable;
struct yagl_client_interface;

struct yagl_offscreen_image_pixmap
{
    struct yagl_image base;

    struct yagl_native_drawable *native_pixmap;

    uint32_t width;
    uint32_t height;
};

struct yagl_offscreen_image_pixmap
    *yagl_offscreen_image_pixmap_create(struct yagl_display *dpy,
                                        struct yagl_native_drawable *native_pixmap,
                                        struct yagl_client_interface *iface);

#endif
