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

#ifndef _YAGL_X11_IMAGE_H_
#define _YAGL_X11_IMAGE_H_

#include "yagl_export.h"
#include "yagl_native_image.h"
#include <X11/Xlib.h>
#include <X11/extensions/XShm.h>

struct yagl_x11_image
{
    struct yagl_native_image base;

    XImage *x_image; /* X11 image */
    XShmSegmentInfo x_shm; /* X11 shared memory segment */

    int is_wrapped;
};

struct yagl_native_image
    *yagl_x11_image_create(struct yagl_native_display *dpy,
                           uint32_t width,
                           uint32_t height,
                           uint32_t depth);

/*
 * Takes ownership of 'x_image'.
 */
struct yagl_native_image
    *yagl_x11_image_wrap(struct yagl_native_display *dpy,
                         XImage *x_image);

#endif
