/*
 * YaGL
 *
 * Copyright (c) 2012 Samsung Electronics Co., Ltd. All rights reserved.
 *
 * Contact :
 * Stanislav Vorobiov <s.vorobiov@samsung.com>
 * Jinhyung Jo <jinhyung.jo@samsung.com>
 * YeongKyoon Lee <yeongkyoon.lee@samsung.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 * Contributors:
 * - S-Core Co., Ltd
 *
 */

#ifndef _WAYLAND_DRM_H_
#define _WAYLAND_DRM_H_

#include "yagl_export.h"
#include "yagl_types.h"

struct wl_drm;
struct wl_resource;
struct wl_display;
struct wl_drm_buffer;
struct vigs_drm_surface;

struct wayland_drm_callbacks
{
    int (*authenticate)(void */*user_data*/,
                        uint32_t /*id*/);

    struct vigs_drm_surface *(*acquire_buffer)(void */*user_data*/,
                                               uint32_t /*name*/);
};

struct wl_drm *wayland_drm_create(struct wl_display *display,
                                  char *device_name,
                                  struct wayland_drm_callbacks *callbacks,
                                  void *user_data);

void wayland_drm_destroy(struct wl_drm *drm);

struct wl_drm_buffer *wayland_drm_get_buffer(struct wl_resource *resource);

struct vigs_drm_surface *wayland_drm_buffer_get_sfc(struct wl_drm_buffer *buffer);

#endif
