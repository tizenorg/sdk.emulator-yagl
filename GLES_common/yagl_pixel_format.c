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

#include "GL/gl.h"
#include "yagl_pixel_format.h"
#include "yagl_state.h"
#include <assert.h>

static GLsizei yagl_pixel_format_get_strides(const struct yagl_gles_pixelstore* ps,
                                             GLsizei width,
                                             GLsizei height,
                                             GLsizei bpp,
                                             GLsizei *image_stride)
{
    GLsizei num_columns = (ps->row_length > 0) ? ps->row_length : width;
    GLsizei row_stride = ((num_columns * bpp) + ps->alignment - 1) & ~(ps->alignment - 1);
    GLsizei num_rows = (ps->image_height > 0) ? ps->image_height : height;

    assert(width >= 0);
    assert(height >= 0);
    assert(bpp > 0);

    *image_stride = row_stride * num_rows;

    return row_stride;
}

GLsizei yagl_pixel_format_get_info(struct yagl_pixel_format *pf,
                                   const struct yagl_gles_pixelstore *ps,
                                   GLsizei width,
                                   GLsizei height,
                                   GLsizei depth,
                                   GLsizei *size)
{
    GLsizei src_row_stride, src_image_stride;
    GLsizei dst_row_stride, dst_image_stride;

    assert(width >= 0);
    assert(height >= 0);
    assert(depth >= 0);

    src_row_stride = yagl_pixel_format_get_strides(ps, width, height,
                                                   pf->src_bpp,
                                                   &src_image_stride);

    dst_row_stride = yagl_pixel_format_get_strides(ps, width, height,
                                                   pf->dst_bpp,
                                                   &dst_image_stride);

    *size = (width * pf->dst_bpp) +
            ((height - 1) * dst_row_stride) +
            ((depth - 1) * dst_image_stride);

    return (ps->skip_images * src_image_stride) +
           (ps->skip_rows * src_row_stride) +
           (ps->skip_pixels * pf->src_bpp);
}

const GLvoid *yagl_pixel_format_unpack(struct yagl_pixel_format *pf,
                                       const struct yagl_gles_pixelstore *ps,
                                       GLsizei width,
                                       GLsizei height,
                                       GLsizei depth,
                                       const GLvoid *pixels)
{
    GLsizei src_row_stride, src_image_stride;
    GLsizei dst_row_stride, dst_image_stride;
    GLsizei i;
    GLvoid *res, *it;

    assert(width >= 0);
    assert(height >= 0);
    assert(depth >= 0);

    if (!pixels || !pf->unpack) {
        return pixels;
    }

    src_row_stride = yagl_pixel_format_get_strides(ps, width, height,
                                                   pf->src_bpp,
                                                   &src_image_stride);

    dst_row_stride = yagl_pixel_format_get_strides(ps, width, height,
                                                   pf->dst_bpp,
                                                   &dst_image_stride);

    res = it = yagl_get_tmp_buffer(dst_image_stride * depth);

    for (i = 0; i < depth; ++i) {
        pf->unpack(pixels, src_row_stride, it, dst_row_stride,
                   width, height);
        pixels += src_image_stride;
        it += dst_image_stride;
    }

    return res;
}

GLvoid *yagl_pixel_format_pack_alloc(struct yagl_pixel_format *pf,
                                     const struct yagl_gles_pixelstore *ps,
                                     GLsizei width,
                                     GLsizei height,
                                     GLvoid *pixels)
{
    GLsizei dst_row_stride, dst_image_stride;

    assert(width >= 0);
    assert(height >= 0);

    if (!pf->pack) {
        return pixels;
    }

    dst_row_stride = yagl_pixel_format_get_strides(ps, width, height,
                                                   pf->dst_bpp,
                                                   &dst_image_stride);

    return yagl_get_tmp_buffer(dst_row_stride * height);
}

void yagl_pixel_format_pack(struct yagl_pixel_format *pf,
                            const struct yagl_gles_pixelstore *ps,
                            GLsizei width,
                            GLsizei height,
                            const GLvoid *pixels_src,
                            GLvoid *pixels_dst)
{
    GLsizei src_row_stride, src_image_stride;
    GLsizei dst_row_stride, dst_image_stride;

    assert(width >= 0);
    assert(height >= 0);

    if (!pf->pack) {
        return;
    }

    src_row_stride = yagl_pixel_format_get_strides(ps, width, height,
                                                   pf->src_bpp,
                                                   &src_image_stride);

    dst_row_stride = yagl_pixel_format_get_strides(ps, width, height,
                                                   pf->dst_bpp,
                                                   &dst_image_stride);

    /*
     * No mistake here, when we 'pack' we need to
     * swap src and dst strides.
     */

    pf->pack(pixels_src, dst_row_stride, pixels_dst, src_row_stride,
             width, height);
}
