#ifndef _YAGL_ONSCREEN_IMAGE_WL_BUFFER_H_
#define _YAGL_ONSCREEN_IMAGE_WL_BUFFER_H_

#include "yagl_export.h"
#include "yagl_types.h"
#include "yagl_image.h"

struct wl_resource;
struct wl_drm_buffer;
struct vigs_drm_surface;
struct yagl_client_interface;

struct yagl_onscreen_image_wl_buffer
{
    struct yagl_image base;

    struct wl_drm_buffer *buffer;
};

struct yagl_onscreen_image_wl_buffer
    *yagl_onscreen_image_wl_buffer_create(struct yagl_display *dpy,
                                          struct wl_resource *buffer,
                                          struct yagl_client_interface *iface);

#endif
