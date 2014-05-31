#include "yagl_wayland_platform.h"
#include "yagl_wayland_display.h"
#include "yagl_native_platform.h"
#include "yagl_log.h"
#include "EGL/egl.h"
#include <wayland-client.h>

static int yagl_wayland_platform_probe(yagl_os_display os_dpy)
{
    void *first_pointer;

    if (os_dpy == (yagl_os_display)EGL_DEFAULT_DISPLAY) {
        return 0;
    }

    first_pointer = *(void**)os_dpy;

    return (first_pointer == &wl_display_interface);
}

static struct yagl_native_display
    *yagl_wayland_wrap_display(yagl_os_display os_dpy,
                               int enable_drm)
{
    struct yagl_native_display *dpy = NULL;

    YAGL_LOG_FUNC_SET(eglGetDisplay);

    if (os_dpy == (yagl_os_display)EGL_DEFAULT_DISPLAY) {
        struct wl_display *wl_dpy;

        wl_dpy = wl_display_connect(NULL);

        if (!wl_dpy) {
            YAGL_LOG_ERROR("Unable to connect to default display");
            return NULL;
        }

        dpy = yagl_wayland_display_create(&yagl_wayland_platform,
                                          (yagl_os_display)wl_dpy,
                                          1);

        if (!dpy) {
            wl_display_disconnect(wl_dpy);
        }
    } else {
        dpy = yagl_wayland_display_create(&yagl_wayland_platform, os_dpy, 0);
    }

    return dpy;
}

struct yagl_native_platform yagl_wayland_platform =
{
    .pixmaps_supported = 0,
    .buffer_age_supported = 1,
    .probe = yagl_wayland_platform_probe,
    .wrap_display = yagl_wayland_wrap_display
};
