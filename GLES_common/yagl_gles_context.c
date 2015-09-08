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
#include "GL/glext.h"
#include "yagl_gles_context.h"
#include "yagl_gles_vertex_array.h"
#include "yagl_gles_array.h"
#include "yagl_gles_buffer.h"
#include "yagl_gles_texture.h"
#include "yagl_gles_texture_unit.h"
#include "yagl_gles_framebuffer.h"
#include "yagl_gles_renderbuffer.h"
#include "yagl_gles_validate.h"
#include "yagl_gles_pixel_formats.h"
#include "yagl_gles_utils.h"
#include "yagl_tex_image_binding.h"
#include "yagl_sharegroup.h"
#include "yagl_log.h"
#include "yagl_malloc.h"
#include "yagl_utils.h"
#include "yagl_state.h"
#include "yagl_render.h"
#include "yagl_host_gles_calls.h"
#include <string.h>
#include <stdlib.h>
#include <assert.h>

/*
 * We can't include GLES2/gl2ext.h here
 */
#define GL_HALF_FLOAT_OES 0x8D61
#define GL_RGB565_OES 0x8D62

#define YAGL_SET_ERR(err) \
    yagl_gles_context_set_error(ctx, err); \
    YAGL_LOG_ERROR("error = 0x%X", err)

static void yagl_get_minmax_index(const GLvoid *indices,
                                  GLsizei count,
                                  GLenum type,
                                  uint32_t *min_idx,
                                  uint32_t *max_idx)
{
    int i;

    *min_idx = UINT32_MAX;
    *max_idx = 0;

    switch (type) {
    case GL_UNSIGNED_BYTE:
        for (i = 0; i < count; ++i) {
            uint32_t idx = ((uint8_t*)indices)[i];
            if (idx < *min_idx) {
                *min_idx = idx;
            }
            if (idx > *max_idx) {
                *max_idx = idx;
            }
        }
        break;
    case GL_UNSIGNED_SHORT:
        for (i = 0; i < count; ++i) {
            uint32_t idx = ((uint16_t*)indices)[i];
            if (idx < *min_idx) {
                *min_idx = idx;
            }
            if (idx > *max_idx) {
                *max_idx = idx;
            }
        }
        break;
    case GL_UNSIGNED_INT:
        for (i = 0; i < count; ++i) {
            uint32_t idx = ((uint32_t*)indices)[i];
            if (idx < *min_idx) {
                *min_idx = idx;
            }
            if (idx > *max_idx) {
                *max_idx = idx;
            }
        }
        break;
    default:
        break;
    }
}

static int yagl_gles_context_bind_tex_image(struct yagl_client_context *ctx,
                                            struct yagl_client_image *image,
                                            struct yagl_tex_image_binding *binding)
{
    struct yagl_gles_context *gles_ctx = (struct yagl_gles_context*)ctx;
    struct yagl_gles_texture_target_state *texture_target_state =
        yagl_gles_context_get_active_texture_target_state(gles_ctx,
                                                          yagl_gles_texture_target_2d);

    if (texture_target_state->texture == texture_target_state->texture_zero) {
        return 0;
    }

    yagl_gles_texture_bind_tex_image(texture_target_state->texture,
                                     (struct yagl_gles_image*)image,
                                     binding);

    binding->cookie = texture_target_state->texture;

    return 1;
}

static int yagl_gles_context_validate_readbuffer(struct yagl_gles_context *ctx,
                                                 GLenum *internalformat)
{
    if (ctx->fbo_read) {
        struct yagl_gles_framebuffer_attachment_state *attachment_state;

        if (ctx->fbo_read->read_buffer == GL_NONE) {
            return 0;
        }

        attachment_state = &ctx->fbo_read->attachment_states[
            yagl_gles_framebuffer_attachment_color0 +
            ctx->fbo_read->read_buffer - GL_COLOR_ATTACHMENT0];

        return yagl_gles_framebuffer_attachment_internalformat(attachment_state,
                                                               internalformat);
    } else {
        if (ctx->fb0_read_buffer == GL_NONE) {
            return 0;
        }

        *internalformat = GL_RGBA;
        return 1;
    }
}

static int yagl_gles_context_validate_drawbuffer(struct yagl_gles_context *ctx,
                                                 GLenum *internalformat)
{
    GLenum fmt = 0;

    if (ctx->fbo_draw) {
        int i;

        for (i = yagl_gles_framebuffer_attachment_color0;
             i < (yagl_gles_framebuffer_attachment_color0 +
                  ctx->max_color_attachments); ++i) {
            struct yagl_gles_framebuffer_attachment_state *attachment_state;
            GLenum tmp;

            attachment_state = &ctx->fbo_draw->attachment_states[i];

            if (yagl_gles_framebuffer_attachment_internalformat(attachment_state,
                                                                &tmp)) {
                if ((fmt != 0) && (tmp != fmt)) {
                    return 0;
                }

                fmt = tmp;
            }
        }

        if (fmt != 0) {
            *internalformat = fmt;
            return 1;
        } else {
            return 0;
        }
    } else {
        if (ctx->fb0_draw_buffer == GL_NONE) {
            return 0;
        }

        *internalformat = GL_RGBA;
        return 1;
    }
}

void yagl_gles_context_init(struct yagl_gles_context *ctx,
                            yagl_client_api client_api,
                            struct yagl_sharegroup *sg)
{
    yagl_sharegroup_acquire(sg);

    yagl_namespace_init(&ctx->framebuffers);
    yagl_namespace_init(&ctx->vertex_arrays);

    ctx->base.bind_tex_image = &yagl_gles_context_bind_tex_image;

    ctx->base.client_api = client_api;
    ctx->base.sg = sg;

    ctx->error = GL_NO_ERROR;

    ctx->pack.alignment = 4;
    ctx->unpack.alignment = 4;

    ctx->blend_equation_rgb = GL_FUNC_ADD;
    ctx->blend_equation_alpha = GL_FUNC_ADD;

    ctx->blend_src_rgb = GL_ONE;
    ctx->blend_dst_rgb = GL_ZERO;

    ctx->blend_src_alpha = GL_ONE;
    ctx->blend_dst_alpha = GL_ZERO;

    ctx->clear_depth = 1.0f;

    ctx->color_writemask[0] = GL_TRUE;
    ctx->color_writemask[1] = GL_TRUE;
    ctx->color_writemask[2] = GL_TRUE;
    ctx->color_writemask[3] = GL_TRUE;

    ctx->cull_face_mode = GL_BACK;

    ctx->depth_func = GL_LESS;

    ctx->depth_writemask = GL_TRUE;

    ctx->depth_range[0] = 0.0f;
    ctx->depth_range[1] = 1.0f;

    ctx->front_face_mode = GL_CCW;

    ctx->fb0_draw_buffer = GL_BACK;
    ctx->fb0_read_buffer = GL_BACK;

    ctx->generate_mipmap_hint = GL_DONT_CARE;

    ctx->sample_coverage_value = 1.0f;
    ctx->sample_coverage_invert = GL_FALSE;

    ctx->clear_stencil = 0;

    ctx->stencil_front.func = GL_ALWAYS;
    ctx->stencil_front.ref = 0;
    ctx->stencil_front.mask = ~0U;
    ctx->stencil_front.writemask = ~0U;
    ctx->stencil_front.fail = GL_KEEP;
    ctx->stencil_front.zfail = GL_KEEP;
    ctx->stencil_front.zpass = GL_KEEP;

    ctx->stencil_back.func = GL_ALWAYS;
    ctx->stencil_back.ref = 0;
    ctx->stencil_back.mask = ~0U;
    ctx->stencil_back.writemask = ~0U;
    ctx->stencil_back.fail = GL_KEEP;
    ctx->stencil_back.zfail = GL_KEEP;
    ctx->stencil_back.zpass = GL_KEEP;

    ctx->line_width = 1.0f;
    ctx->polygon_offset_factor = 0.0f;
    ctx->polygon_offset_units = 0.0f;

    ctx->dither_enabled = GL_TRUE;
}

void yagl_gles_context_prepare(struct yagl_gles_context *ctx,
                               int num_texture_units,
                               int num_arrays)
{
    int i;
    int32_t size = 0;
    char *extensions;
    char *min_mag_blits;

    if (num_texture_units < 1) {
        num_texture_units = 1;
    }

    YAGL_LOG_FUNC_ENTER(yagl_gles_context_prepare,
                        "num_texture_units = %d",
                        num_texture_units);

    /*
     * Currently minifying and magnifying blits in
     * glBlitFramebuffer are disabled because of host
     * OpenGL driver problems on many GPUs, they simply crash.
     * But we want to be able to enable this temporarily for
     * some apps that require it.
     */

    min_mag_blits = getenv("YAGL_MIN_MAG_BLITS");

    if (min_mag_blits && atoi(min_mag_blits)) {
        ctx->min_mag_blits = 1;
    } else {
        ctx->min_mag_blits = 0;
    }

    ctx->num_texture_units = num_texture_units;
    ctx->texture_units =
        yagl_malloc(ctx->num_texture_units * sizeof(*ctx->texture_units));

    for (i = 0; i < ctx->num_texture_units; ++i) {
        yagl_gles_texture_unit_init(&ctx->texture_units[i], ctx->base.sg);
    }

    ctx->num_arrays = num_arrays;

    yagl_host_glGetString(GL_EXTENSIONS, NULL, 0, &size);
    extensions = yagl_malloc0(size);
    yagl_host_glGetString(GL_EXTENSIONS, extensions, size, NULL);

    if (yagl_get_host_gl_version() > yagl_gl_2) {
        ctx->packed_depth_stencil = 1;
    } else {
        ctx->packed_depth_stencil = (strstr(extensions, "GL_EXT_packed_depth_stencil ") != NULL);
    }

    ctx->texture_npot = 1;

    ctx->texture_filter_anisotropic = (strstr(extensions, "GL_EXT_texture_filter_anisotropic ") != NULL);

    yagl_free(extensions);

    yagl_host_glGetIntegerv(GL_MAX_COLOR_ATTACHMENTS,
                            &ctx->max_color_attachments, 1, NULL);

    if (ctx->max_color_attachments <= 0) {
        ctx->max_color_attachments = 1;
    }

    if (ctx->max_color_attachments > YAGL_MAX_GLES_FRAMEBUFFER_COLOR_ATTACHMENTS) {
        ctx->max_color_attachments = YAGL_MAX_GLES_FRAMEBUFFER_COLOR_ATTACHMENTS;
    }

    yagl_host_glGetIntegerv(GL_MAX_DRAW_BUFFERS,
                            &ctx->max_draw_buffers, 1, NULL);

    if (ctx->max_draw_buffers <= 0) {
        ctx->max_draw_buffers = 1;
    }

    if (ctx->max_draw_buffers > YAGL_MAX_GLES_FRAMEBUFFER_COLOR_ATTACHMENTS) {
        ctx->max_draw_buffers = YAGL_MAX_GLES_FRAMEBUFFER_COLOR_ATTACHMENTS;
    }

    ctx->vertex_arrays_supported = (yagl_get_host_gl_version() > yagl_gl_2);

    YAGL_LOG_FUNC_EXIT(NULL);
}

void yagl_gles_context_prepare_end(struct yagl_gles_context *ctx,
                                   const GLchar **extensions,
                                   int num_extensions)
{
    int i, str_size = 1;
    struct yagl_gles_array *arrays = ctx->create_arrays(ctx);
    GLchar *ptr;

    if (ctx->vertex_arrays_supported) {
        ctx->va_zero = yagl_gles_vertex_array_create(0, arrays, ctx->num_arrays);

        yagl_gles_context_bind_vertex_array(ctx, NULL);
    } else {
        ctx->va_zero = yagl_gles_vertex_array_create(1, arrays, ctx->num_arrays);

        /*
         * Don't bind, VAOs are not supported, just reference.
         */
        yagl_gles_vertex_array_acquire(ctx->va_zero);
        ctx->vao = ctx->va_zero;
    }

    ctx->extensions = extensions;
    ctx->num_extensions = num_extensions;

    for (i = 0; i < num_extensions; ++i) {
        str_size += strlen(extensions[i]) + 1;
    }

    ctx->extension_string = ptr = yagl_malloc(str_size);

    for (i = 0; i < num_extensions; ++i) {
        int len = strlen(extensions[i]);

        memcpy(ptr, extensions[i], len);

        ptr += len;

        *ptr = ' ';
        ++ptr;
    }

    ctx->extension_string[str_size - 1] = '\0';
}

void yagl_gles_context_cleanup(struct yagl_gles_context *ctx)
{
    int i;

    yagl_gles_buffer_release(ctx->unpack.pbo);
    yagl_gles_buffer_release(ctx->pack.pbo);
    yagl_gles_renderbuffer_release(ctx->rbo);
    yagl_gles_framebuffer_release(ctx->fbo_read);
    yagl_gles_framebuffer_release(ctx->fbo_draw);
    yagl_gles_buffer_release(ctx->vbo);
    yagl_gles_vertex_array_release(ctx->vao);

    for (i = 0; i < ctx->num_texture_units; ++i) {
        yagl_gles_texture_unit_cleanup(&ctx->texture_units[i]);
    }

    yagl_free(ctx->texture_units);

    yagl_gles_vertex_array_release(ctx->va_zero);

    yagl_free(ctx->extension_string);
    yagl_free(ctx->extensions);

    yagl_namespace_cleanup(&ctx->vertex_arrays);
    yagl_namespace_cleanup(&ctx->framebuffers);

    yagl_sharegroup_release(ctx->base.sg);
}

void yagl_gles_context_set_error(struct yagl_gles_context *ctx, GLenum error)
{
    if (ctx->error == GL_NO_ERROR) {
        ctx->error = error;
    }
}

GLenum yagl_gles_context_get_error(struct yagl_gles_context *ctx)
{
    GLenum error = ctx->error;

    ctx->error = GL_NO_ERROR;

    return error;
}

void yagl_gles_context_bind_vertex_array(struct yagl_gles_context *ctx,
                                         struct yagl_gles_vertex_array *va)
{
    if (!va) {
        va = ctx->va_zero;
    }

    yagl_gles_vertex_array_acquire(va);
    yagl_gles_vertex_array_release(ctx->vao);
    ctx->vao = va;

    yagl_gles_vertex_array_bind(va);
}

void yagl_gles_context_unbind_vertex_array(struct yagl_gles_context *ctx,
                                           yagl_object_name va_local_name)
{
    if ((ctx->vao != ctx->va_zero) &&
        (ctx->vao->base.local_name == va_local_name)) {
        yagl_gles_vertex_array_acquire(ctx->va_zero);
        yagl_gles_vertex_array_release(ctx->vao);
        ctx->vao = ctx->va_zero;

        /*
         * And bind vertex array 0, otherwise we'll end up with no
         * vertex array on host.
         */
        yagl_gles_vertex_array_bind(ctx->vao);
    }
}

int yagl_gles_context_convert_textures(struct yagl_gles_context *ctx)
{
    /*
     * Currently GLESv1_CM is implemented via legacy context always, so
     * don't convert textures in this case.
     */
    return (ctx->base.client_api != yagl_client_api_gles1) &&
           (yagl_get_host_gl_version() > yagl_gl_2);
}

int yagl_gles_context_validate_texture_target(struct yagl_gles_context *ctx,
                                              GLenum target,
                                              yagl_gles_texture_target *texture_target)
{
    switch (target) {
    case GL_TEXTURE_2D:
        *texture_target = yagl_gles_texture_target_2d;
        break;
    default:
        return ctx->validate_texture_target(ctx, target, texture_target);
    }

    return 1;
}

struct yagl_pixel_format
    *yagl_gles_context_validate_teximage_format(struct yagl_gles_context *ctx,
                                                GLenum internalformat,
                                                GLenum format,
                                                GLenum type)
{
    struct yagl_pixel_format *pf;

    YAGL_LOG_FUNC_SET(yagl_gles_context_validate_teximage_format);

    if ((format == internalformat) || (internalformat == GL_BGRA)) {
        switch (format) {
        case GL_RGB:
            switch (type) {
            YAGL_PIXEL_FORMAT_CASE(gles, GL_RGB, GL_RGB, GL_UNSIGNED_BYTE);
            YAGL_PIXEL_FORMAT_CASE(gles, GL_RGB, GL_RGB, GL_UNSIGNED_SHORT_5_6_5);
            YAGL_PIXEL_FORMAT_CASE(gles, GL_RGB, GL_RGB, GL_FLOAT);
            YAGL_PIXEL_FORMAT_CASE(gles, GL_RGB, GL_RGB, GL_HALF_FLOAT_OES);
            }
            break;
        case GL_RGBA:
            switch (type) {
            YAGL_PIXEL_FORMAT_CASE(gles, GL_RGBA, GL_RGBA, GL_UNSIGNED_BYTE);
            YAGL_PIXEL_FORMAT_CASE(gles, GL_RGBA, GL_RGBA, GL_UNSIGNED_SHORT_4_4_4_4);
            YAGL_PIXEL_FORMAT_CASE(gles, GL_RGBA, GL_RGBA, GL_UNSIGNED_SHORT_5_5_5_1);
            YAGL_PIXEL_FORMAT_CASE(gles, GL_RGBA, GL_RGBA, GL_FLOAT);
            YAGL_PIXEL_FORMAT_CASE(gles, GL_RGBA, GL_RGBA, GL_HALF_FLOAT_OES);
            }
            break;
        case GL_BGRA:
            switch (type) {
            YAGL_PIXEL_FORMAT_CASE(gles, GL_BGRA, GL_BGRA, GL_UNSIGNED_BYTE);
            YAGL_PIXEL_FORMAT_CASE(gles, GL_BGRA, GL_BGRA, GL_FLOAT);
            YAGL_PIXEL_FORMAT_CASE(gles, GL_BGRA, GL_BGRA, GL_HALF_FLOAT_OES);
            }
            break;
        case GL_DEPTH_COMPONENT:
            switch (type) {
            YAGL_PIXEL_FORMAT_CASE(gles, GL_DEPTH_COMPONENT, GL_DEPTH_COMPONENT, GL_UNSIGNED_SHORT);
            YAGL_PIXEL_FORMAT_CASE(gles, GL_DEPTH_COMPONENT, GL_DEPTH_COMPONENT, GL_UNSIGNED_INT);
            }
            break;
        case GL_DEPTH_STENCIL:
            switch (type) {
            YAGL_PIXEL_FORMAT_CASE(gles, GL_DEPTH_STENCIL, GL_DEPTH_STENCIL, GL_UNSIGNED_INT_24_8);
            }
            break;
        }

        if (yagl_gles_context_convert_textures(ctx)) {
            switch (format) {
            case GL_ALPHA:
                switch (type) {
                YAGL_PIXEL_FORMAT_CASE(gles_gl3, GL_ALPHA, GL_ALPHA, GL_UNSIGNED_BYTE);
                YAGL_PIXEL_FORMAT_CASE(gles_gl3, GL_ALPHA, GL_ALPHA, GL_FLOAT);
                YAGL_PIXEL_FORMAT_CASE(gles_gl3, GL_ALPHA, GL_ALPHA, GL_HALF_FLOAT_OES);
                }
                break;
            case GL_LUMINANCE:
                switch (type) {
                YAGL_PIXEL_FORMAT_CASE(gles_gl3, GL_LUMINANCE, GL_LUMINANCE, GL_UNSIGNED_BYTE);
                YAGL_PIXEL_FORMAT_CASE(gles_gl3, GL_LUMINANCE, GL_LUMINANCE, GL_FLOAT);
                YAGL_PIXEL_FORMAT_CASE(gles_gl3, GL_LUMINANCE, GL_LUMINANCE, GL_HALF_FLOAT_OES);
                }
                break;
            case GL_LUMINANCE_ALPHA:
                switch (type) {
                YAGL_PIXEL_FORMAT_CASE(gles_gl3, GL_LUMINANCE_ALPHA, GL_LUMINANCE_ALPHA, GL_UNSIGNED_BYTE);
                YAGL_PIXEL_FORMAT_CASE(gles_gl3, GL_LUMINANCE_ALPHA, GL_LUMINANCE_ALPHA, GL_FLOAT);
                YAGL_PIXEL_FORMAT_CASE(gles_gl3, GL_LUMINANCE_ALPHA, GL_LUMINANCE_ALPHA, GL_HALF_FLOAT_OES);
                }
                break;
            }
        } else {
            switch (format) {
            case GL_ALPHA:
                switch (type) {
                YAGL_PIXEL_FORMAT_CASE(gles_gl2, GL_ALPHA, GL_ALPHA, GL_UNSIGNED_BYTE);
                YAGL_PIXEL_FORMAT_CASE(gles_gl2, GL_ALPHA, GL_ALPHA, GL_FLOAT);
                YAGL_PIXEL_FORMAT_CASE(gles_gl2, GL_ALPHA, GL_ALPHA, GL_HALF_FLOAT_OES);
                }
                break;
            case GL_LUMINANCE:
                switch (type) {
                YAGL_PIXEL_FORMAT_CASE(gles_gl2, GL_LUMINANCE, GL_LUMINANCE, GL_UNSIGNED_BYTE);
                YAGL_PIXEL_FORMAT_CASE(gles_gl2, GL_LUMINANCE, GL_LUMINANCE, GL_FLOAT);
                YAGL_PIXEL_FORMAT_CASE(gles_gl2, GL_LUMINANCE, GL_LUMINANCE, GL_HALF_FLOAT_OES);
                }
                break;
            case GL_LUMINANCE_ALPHA:
                switch (type) {
                YAGL_PIXEL_FORMAT_CASE(gles_gl2, GL_LUMINANCE_ALPHA, GL_LUMINANCE_ALPHA, GL_UNSIGNED_BYTE);
                YAGL_PIXEL_FORMAT_CASE(gles_gl2, GL_LUMINANCE_ALPHA, GL_LUMINANCE_ALPHA, GL_FLOAT);
                YAGL_PIXEL_FORMAT_CASE(gles_gl2, GL_LUMINANCE_ALPHA, GL_LUMINANCE_ALPHA, GL_HALF_FLOAT_OES);
                }
                break;
            }
        }
    }

    pf = ctx->validate_teximage_format(ctx, internalformat, format, type);

    if (!pf) {
        YAGL_LOG_ERROR("validation error: internalformat = 0x%X, format = 0x%X, type = 0x%X",
                       internalformat,
                       format,
                       type);
    }

    return pf;
}

struct yagl_pixel_format
    *yagl_gles_context_validate_getteximage_format(struct yagl_gles_context *ctx,
                                                   GLenum format,
                                                   GLenum type)
{
    GLenum readbuffer_internalformat = 0;
    const struct yagl_gles_format_info *readbuffer_format_info;
    struct yagl_pixel_format *pf = NULL;

    YAGL_LOG_FUNC_SET(yagl_gles_context_validate_getteximage_format);

    if (!yagl_gles_context_validate_readbuffer(ctx,
                                               &readbuffer_internalformat)) {
        goto out;
    }

    readbuffer_format_info =
        yagl_gles_internalformat_info(readbuffer_internalformat);

    if ((readbuffer_format_info->flags & yagl_gles_format_color_renderable) == 0) {
        goto out;
    }

    switch (format) {
    case GL_RGBA:
        if (((readbuffer_format_info->flags & (yagl_gles_format_unsigned_integer)) == 0) &&
            ((readbuffer_format_info->flags & (yagl_gles_format_signed_integer)) == 0)) {
            switch (type) {
            YAGL_PIXEL_FORMAT_CASE(gles, GL_RGBA, GL_RGBA, GL_UNSIGNED_BYTE);
            }
        }
        break;
    }

    pf = ctx->validate_getteximage_format(ctx,
                                          readbuffer_internalformat,
                                          format,
                                          type);

out:
    if (!pf) {
        YAGL_LOG_ERROR("validation error: readbuffer_internalformat = %u, format = 0x%X, type = 0x%X",
                       readbuffer_internalformat,
                       format,
                       type);
    }

    return pf;
}

int yagl_gles_context_validate_copyteximage_format(struct yagl_gles_context *ctx,
                                                   GLenum *internalformat,
                                                   int *is_float)
{
    GLenum readbuffer_internalformat = 0;
    const struct yagl_gles_format_info *readbuffer_format_info;
    const struct yagl_gles_format_info *internalformat_info;
    int res = 0;

    YAGL_LOG_FUNC_SET(yagl_gles_context_validate_copyteximage_format);

    if (!yagl_gles_context_validate_readbuffer(ctx,
                                               &readbuffer_internalformat)) {
        goto out;
    }

    readbuffer_format_info = yagl_gles_internalformat_info(readbuffer_internalformat);
    internalformat_info = yagl_gles_internalformat_info(*internalformat);

    if ((internalformat_info->flags & yagl_gles_format_srgb) !=
        (readbuffer_format_info->flags & yagl_gles_format_srgb)) {
        goto out;
    }

    if (internalformat_info->num_components > readbuffer_format_info->num_components) {
        goto out;
    }

    if (((readbuffer_format_info->flags & yagl_gles_format_unsigned_integer) == 0) &&
        ((readbuffer_format_info->flags & yagl_gles_format_signed_integer) == 0)) {
        switch (*internalformat) {
        case GL_RGB:
            res = 1;
            goto out;
        case GL_RGBA:
            res = 1;
            goto out;
        case GL_BGRA:
            res = 1;
            *internalformat = GL_RGBA;
            goto out;
        }

        if (yagl_gles_context_convert_textures(ctx)) {
            switch (*internalformat) {
            case GL_ALPHA:
            case GL_LUMINANCE_ALPHA:
                if (readbuffer_format_info->num_components == 4) {
                    *internalformat = GL_RGBA;
                    res = 1;
                    goto out;
                }
                break;
            case GL_LUMINANCE:
                res = 1;
                *internalformat = GL_RGBA;
                goto out;
            }
        } else {
            switch (*internalformat) {
            case GL_ALPHA:
            case GL_LUMINANCE_ALPHA:
                if (readbuffer_format_info->num_components == 4) {
                    res = 1;
                    goto out;
                }
                break;
            case GL_LUMINANCE:
                res = 1;
                goto out;
            }
        }
    }

    res = ctx->validate_copyteximage_format(ctx,
                                            readbuffer_internalformat,
                                            internalformat);

out:
    if (res) {
        *is_float = ((readbuffer_format_info->flags & yagl_gles_format_float) != 0);
    } else {
        YAGL_LOG_ERROR("validation error: readbuffer_internalformat = %u, internalformat = 0x%X",
                       readbuffer_internalformat,
                       *internalformat);
    }

    return res;
}

int yagl_gles_context_validate_texstorage_format(struct yagl_gles_context *ctx,
                                                 GLenum *internalformat,
                                                 GLenum *base_internalformat,
                                                 GLenum *any_format,
                                                 GLenum *any_type)
{
    int res = 1;

    YAGL_LOG_FUNC_SET(yagl_gles_context_validate_texstorage_format);

    switch (*internalformat) {
    case GL_RGB:
        *base_internalformat = GL_RGB;
        *any_format = GL_RGB;
        *any_type = GL_UNSIGNED_BYTE;
        goto out;
    case GL_RGBA:
        *base_internalformat = GL_RGBA;
        *any_format = GL_RGBA;
        *any_type = GL_UNSIGNED_BYTE;
        goto out;
    case GL_BGRA:
        *base_internalformat = GL_RGBA;
        *internalformat = GL_RGBA;
        *any_format = GL_BGRA;
        *any_type = GL_UNSIGNED_BYTE;
        goto out;
    case GL_DEPTH_COMPONENT:
        *base_internalformat = GL_DEPTH_COMPONENT;
        *any_format = GL_DEPTH_COMPONENT;
        *any_type = GL_UNSIGNED_INT;
        goto out;
    case GL_DEPTH_STENCIL:
        *base_internalformat = GL_DEPTH_STENCIL;
        *any_format = GL_DEPTH_STENCIL;
        *any_type = GL_UNSIGNED_INT_24_8;
        goto out;
    }

    if (yagl_gles_context_convert_textures(ctx)) {
        switch (*internalformat) {
        case GL_ALPHA:
            *internalformat = GL_RGBA;
            *base_internalformat = GL_ALPHA;
            *any_format = GL_BGRA;
            *any_type = GL_UNSIGNED_BYTE;
            break;
        case GL_LUMINANCE:
            *internalformat = GL_RGBA;
            *base_internalformat = GL_LUMINANCE;
            *any_format = GL_BGRA;
            *any_type = GL_UNSIGNED_BYTE;
            break;
        case GL_LUMINANCE_ALPHA:
            *internalformat = GL_RGBA;
            *base_internalformat = GL_LUMINANCE_ALPHA;
            *any_format = GL_BGRA;
            *any_type = GL_UNSIGNED_BYTE;
            break;
        default:
            res = ctx->validate_texstorage_format(ctx,
                                                  internalformat,
                                                  base_internalformat,
                                                  any_format,
                                                  any_type);
        }
    } else {
        switch (*internalformat) {
        case GL_ALPHA:
            *base_internalformat = GL_ALPHA;
            *any_format = GL_ALPHA;
            *any_type = GL_UNSIGNED_BYTE;
            break;
        case GL_LUMINANCE:
            *base_internalformat = GL_LUMINANCE;
            *any_format = GL_LUMINANCE;
            *any_type = GL_UNSIGNED_BYTE;
            break;
        case GL_LUMINANCE_ALPHA:
            *base_internalformat = GL_LUMINANCE_ALPHA;
            *any_format = GL_LUMINANCE_ALPHA;
            *any_type = GL_UNSIGNED_BYTE;
            break;
        default:
            res = ctx->validate_texstorage_format(ctx,
                                                  internalformat,
                                                  base_internalformat,
                                                  any_format,
                                                  any_type);
        }
    }

    if (!res) {
        YAGL_LOG_ERROR("validation error: internalformat = 0x%X",
                       *internalformat);
    }

out:
    return res;
}

int yagl_gles_context_validate_renderbuffer_format(struct yagl_gles_context *ctx,
                                                   GLenum *internalformat)
{
    int res = 1;

    YAGL_LOG_FUNC_SET(yagl_gles_context_validate_renderbuffer_format);

    switch (*internalformat) {
    case GL_RGBA4:
    case GL_RGB565_OES:
    case GL_RGB5_A1:
    case GL_DEPTH_COMPONENT16:
    case GL_DEPTH_COMPONENT24:
    case GL_DEPTH_COMPONENT32:
    case GL_STENCIL_INDEX8:
    case GL_DEPTH24_STENCIL8:
        break;
    default:
        res = ctx->validate_renderbuffer_format(ctx, internalformat);
    }

    if (!res) {
        YAGL_LOG_ERROR("validation error: internalformat = 0x%X",
                       *internalformat);
    }

    return res;
}

void yagl_gles_context_set_active_texture(struct yagl_gles_context *ctx,
                                          GLenum texture)
{
    YAGL_LOG_FUNC_SET(glActiveTexture);

    if ((texture < GL_TEXTURE0) ||
        (texture >= (GL_TEXTURE0 + ctx->num_texture_units))) {
        YAGL_SET_ERR(GL_INVALID_ENUM);
        return;
    }

    ctx->active_texture_unit = texture - GL_TEXTURE0;

    yagl_host_glActiveTexture(texture);
}

struct yagl_gles_texture_unit
    *yagl_gles_context_get_active_texture_unit(struct yagl_gles_context *ctx)
{
    return &ctx->texture_units[ctx->active_texture_unit];
}

struct yagl_gles_texture_target_state
    *yagl_gles_context_get_active_texture_target_state(struct yagl_gles_context *ctx,
                                                       yagl_gles_texture_target texture_target)
{
    return &yagl_gles_context_get_active_texture_unit(ctx)->target_states[texture_target];
}

void yagl_gles_context_active_texture_set_enabled(struct yagl_gles_context *ctx,
    yagl_gles_texture_target texture_target, int enabled)
{
    struct yagl_gles_texture_target_state *texture_target_state;

    texture_target_state =
            yagl_gles_context_get_active_texture_target_state(ctx,
                                                              texture_target);
    texture_target_state->enabled = enabled;
}

void yagl_gles_context_bind_texture(struct yagl_gles_context *ctx,
                                    GLenum target,
                                    struct yagl_gles_texture *texture)
{
    yagl_gles_texture_target texture_target;
    struct yagl_gles_texture_target_state *texture_target_state;

    YAGL_LOG_FUNC_SET(glBindTexture);

    if (!yagl_gles_context_validate_texture_target(ctx, target, &texture_target)) {
        YAGL_SET_ERR(GL_INVALID_ENUM);
        return;
    }

    texture_target_state =
        yagl_gles_context_get_active_texture_target_state(ctx, texture_target);

    if (!texture) {
        texture = texture_target_state->texture_zero;
    }

    if (!yagl_gles_texture_bind(texture, target)) {
        YAGL_SET_ERR(GL_INVALID_OPERATION);
        return;
    }

    yagl_gles_texture_acquire(texture);
    yagl_gles_texture_release(texture_target_state->texture);
    texture_target_state->texture = texture;
}

void yagl_gles_context_unbind_texture(struct yagl_gles_context *ctx,
                                      yagl_object_name texture_local_name)
{
    int i;
    struct yagl_gles_texture_unit *texture_unit =
        yagl_gles_context_get_active_texture_unit(ctx);

    for (i = 0; i < YAGL_NUM_TEXTURE_TARGETS; ++i) {
        if ((texture_unit->target_states[i].texture != texture_unit->target_states[i].texture_zero) &&
            (texture_unit->target_states[i].texture->base.local_name == texture_local_name)) {
            yagl_gles_texture_acquire(texture_unit->target_states[i].texture_zero);
            yagl_gles_texture_release(texture_unit->target_states[i].texture);
            texture_unit->target_states[i].texture =
                texture_unit->target_states[i].texture_zero;

            /*
             * And bind texture 0, otherwise we'll end up using
             * real texture 0 on host.
             */
            yagl_gles_texture_bind(texture_unit->target_states[i].texture,
                                   texture_unit->target_states[i].texture->target);
        }
    }

    if (ctx->fbo_draw) {
        yagl_gles_framebuffer_unbind_texture(ctx->fbo_draw,
                                             texture_local_name);
    }

    if (ctx->fbo_read) {
        yagl_gles_framebuffer_unbind_texture(ctx->fbo_read,
                                             texture_local_name);
    }
}

void yagl_gles_context_bind_buffer(struct yagl_gles_context *ctx,
                                   GLenum target,
                                   struct yagl_gles_buffer *buffer)
{
    YAGL_LOG_FUNC_SET(glBindBuffer);

    switch (target) {
    case GL_ARRAY_BUFFER:
        yagl_gles_buffer_acquire(buffer);
        yagl_gles_buffer_release(ctx->vbo);
        ctx->vbo = buffer;
        break;
    case GL_ELEMENT_ARRAY_BUFFER:
        yagl_gles_buffer_acquire(buffer);
        yagl_gles_buffer_release(ctx->vao->ebo);
        ctx->vao->ebo = buffer;
        break;
    case GL_PIXEL_PACK_BUFFER:
        yagl_gles_buffer_acquire(buffer);
        yagl_gles_buffer_release(ctx->pack.pbo);
        ctx->pack.pbo = buffer;
        break;
    case GL_PIXEL_UNPACK_BUFFER:
        yagl_gles_buffer_acquire(buffer);
        yagl_gles_buffer_release(ctx->unpack.pbo);
        ctx->unpack.pbo = buffer;
        break;
    default:
        if (!ctx->bind_buffer(ctx, target, buffer)) {
            YAGL_SET_ERR(GL_INVALID_ENUM);
            return;
        }
    }

    if (buffer) {
        yagl_gles_buffer_set_bound(buffer);
    }
}

void yagl_gles_context_unbind_buffer(struct yagl_gles_context *ctx,
                                     yagl_object_name buffer_local_name)
{
    if (ctx->vbo && (ctx->vbo->base.local_name == buffer_local_name)) {
        yagl_gles_buffer_release(ctx->vbo);
        ctx->vbo = NULL;
    }

    if (ctx->vao->ebo && (ctx->vao->ebo->base.local_name == buffer_local_name)) {
        yagl_gles_buffer_release(ctx->vao->ebo);
        ctx->vao->ebo = NULL;
    }

    if (ctx->pack.pbo && (ctx->pack.pbo->base.local_name == buffer_local_name)) {
        yagl_gles_buffer_release(ctx->pack.pbo);
        ctx->pack.pbo = NULL;
    }

    if (ctx->unpack.pbo && (ctx->unpack.pbo->base.local_name == buffer_local_name)) {
        yagl_gles_buffer_release(ctx->unpack.pbo);
        ctx->unpack.pbo = NULL;
    }

    ctx->unbind_buffer(ctx, buffer_local_name);
}

void yagl_gles_context_bind_framebuffer(struct yagl_gles_context *ctx,
                                        GLenum target,
                                        struct yagl_gles_framebuffer *fbo)
{
    YAGL_LOG_FUNC_SET(glBindFramebuffer);

    switch (target) {
    case GL_FRAMEBUFFER:
        yagl_gles_framebuffer_acquire(fbo);
        yagl_gles_framebuffer_acquire(fbo);
        yagl_gles_framebuffer_release(ctx->fbo_draw);
        yagl_gles_framebuffer_release(ctx->fbo_read);
        ctx->fbo_draw = ctx->fbo_read = fbo;
        break;
    case GL_DRAW_FRAMEBUFFER:
        yagl_gles_framebuffer_acquire(fbo);
        yagl_gles_framebuffer_release(ctx->fbo_draw);
        ctx->fbo_draw = fbo;
        break;
    case GL_READ_FRAMEBUFFER:
        yagl_gles_framebuffer_acquire(fbo);
        yagl_gles_framebuffer_release(ctx->fbo_read);
        ctx->fbo_read = fbo;
        break;
    default:
        YAGL_SET_ERR(GL_INVALID_ENUM);
        return;
    }

    yagl_gles_framebuffer_bind(fbo, target);
}

void yagl_gles_context_unbind_framebuffer(struct yagl_gles_context *ctx,
                                          yagl_object_name fbo_local_name)
{
    if (ctx->fbo_draw && (ctx->fbo_draw->base.local_name == fbo_local_name)) {
        yagl_gles_framebuffer_release(ctx->fbo_draw);
        ctx->fbo_draw = NULL;
    }

    if (ctx->fbo_read && (ctx->fbo_read->base.local_name == fbo_local_name)) {
        yagl_gles_framebuffer_release(ctx->fbo_read);
        ctx->fbo_read = NULL;
    }
}

GLenum yagl_gles_context_check_framebuffer_status(struct yagl_gles_context *ctx,
                                                  struct yagl_gles_framebuffer *fb)
{
    GLenum res = 0;
    struct yagl_gles_texture *texture;
    struct yagl_gles_renderbuffer *rb;
    const struct yagl_gles_format_info *format_info;
    int i, missing = 1;

    if (!fb) {
        res = GL_FRAMEBUFFER_COMPLETE;
        goto out;
    }

    switch (fb->attachment_states[yagl_gles_framebuffer_attachment_depth].type) {
    case GL_TEXTURE:
        missing = 0;

        texture = fb->attachment_states[yagl_gles_framebuffer_attachment_depth].texture;

        if (!texture) {
            res = GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT;
            goto out;
        }

        format_info = yagl_gles_internalformat_info(texture->internalformat);

        if ((format_info->flags & yagl_gles_format_depth_renderable) == 0) {
            res = GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT;
            goto out;
        }

        break;
    case GL_RENDERBUFFER:
        missing = 0;

        rb = fb->attachment_states[yagl_gles_framebuffer_attachment_depth].rb;

        if (!rb) {
            res = GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT;
            goto out;
        }

        format_info = yagl_gles_internalformat_info(rb->internalformat);

        if ((format_info->flags & yagl_gles_format_depth_renderable) == 0) {
            res = GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT;
            goto out;
        }

        break;
    }

    switch (fb->attachment_states[yagl_gles_framebuffer_attachment_stencil].type) {
    case GL_TEXTURE:
        missing = 0;

        texture = fb->attachment_states[yagl_gles_framebuffer_attachment_stencil].texture;

        if (!texture) {
            res = GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT;
            goto out;
        }

        format_info = yagl_gles_internalformat_info(texture->internalformat);

        if ((format_info->flags & yagl_gles_format_stencil_renderable) == 0) {
            res = GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT;
            goto out;
        }

        break;
    case GL_RENDERBUFFER:
        missing = 0;

        rb = fb->attachment_states[yagl_gles_framebuffer_attachment_stencil].rb;

        if (!rb) {
            res = GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT;
            goto out;
        }

        format_info = yagl_gles_internalformat_info(rb->internalformat);

        if ((format_info->flags & yagl_gles_format_stencil_renderable) == 0) {
            res = GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT;
            goto out;
        }

        break;
    }

    for (i = yagl_gles_framebuffer_attachment_color0;
         i < (yagl_gles_framebuffer_attachment_color0 +
              ctx->max_color_attachments); ++i) {
        switch (fb->attachment_states[i].type) {
        case GL_TEXTURE:
            missing = 0;

            texture = fb->attachment_states[i].texture;

            if (!texture) {
                res = GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT;
                goto out;
            }

            format_info = yagl_gles_internalformat_info(texture->internalformat);

            if ((format_info->flags & yagl_gles_format_color_renderable) == 0) {
                res = GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT;
                goto out;
            }

            if (!yagl_gles_texture_color_renderable(texture)) {
                res = GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT;
                goto out;
            }

            break;
        case GL_RENDERBUFFER:
            missing = 0;

            rb = fb->attachment_states[i].rb;

            if (!rb) {
                res = GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT;
                goto out;
            }

            format_info = yagl_gles_internalformat_info(rb->internalformat);

            if ((format_info->flags & yagl_gles_format_color_renderable) == 0) {
                res = GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT;
                goto out;
            }

            break;
        }
    }

    if (missing) {
        res = GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT;
        goto out;
    }

    /*
     * According to OpenGL standard, 9.4.3 Required Framebuffer Formats:
     * "However, when both depth and stencil attachments are
     * present, implementations are only required to support
     * framebuffer objects where both attachments refer to
     * the same image."
     */

    if ((fb->attachment_states[yagl_gles_framebuffer_attachment_depth].type != GL_NONE) &&
        (fb->attachment_states[yagl_gles_framebuffer_attachment_stencil].type != GL_NONE) &&
        (memcmp(&fb->attachment_states[yagl_gles_framebuffer_attachment_depth],
                &fb->attachment_states[yagl_gles_framebuffer_attachment_stencil],
                sizeof(struct yagl_gles_framebuffer_attachment_state)) != 0)) {
        res = GL_FRAMEBUFFER_UNSUPPORTED;
        goto out;
    }

    res = GL_FRAMEBUFFER_COMPLETE;

out:
    return res;
}

void yagl_gles_context_bind_renderbuffer(struct yagl_gles_context *ctx,
                                         GLenum target,
                                         struct yagl_gles_renderbuffer *rbo)
{
    YAGL_LOG_FUNC_SET(glBindRenderbuffer);

    switch (target) {
    case GL_RENDERBUFFER:
        yagl_gles_renderbuffer_acquire(rbo);
        yagl_gles_renderbuffer_release(ctx->rbo);
        ctx->rbo = rbo;
        break;
    default:
        YAGL_SET_ERR(GL_INVALID_ENUM);
        return;
    }

    yagl_gles_renderbuffer_bind(rbo, target);
}

void yagl_gles_context_unbind_renderbuffer(struct yagl_gles_context *ctx,
                                           yagl_object_name rbo_local_name)
{
    if (ctx->rbo && (ctx->rbo->base.local_name == rbo_local_name)) {
        yagl_gles_renderbuffer_release(ctx->rbo);
        ctx->rbo = NULL;
    }

    if (ctx->fbo_draw) {
        yagl_gles_framebuffer_unbind_renderbuffer(ctx->fbo_draw,
                                                  rbo_local_name);
    }

    if (ctx->fbo_read) {
        yagl_gles_framebuffer_unbind_renderbuffer(ctx->fbo_read,
                                                  rbo_local_name);
    }
}

int yagl_gles_context_acquire_binded_buffer(struct yagl_gles_context *ctx,
                                            GLenum target,
                                            struct yagl_gles_buffer **buffer)
{
    switch (target) {
    case GL_ARRAY_BUFFER:
        yagl_gles_buffer_acquire(ctx->vbo);
        *buffer = ctx->vbo;
        break;
    case GL_ELEMENT_ARRAY_BUFFER:
        yagl_gles_buffer_acquire(ctx->vao->ebo);
        *buffer = ctx->vao->ebo;
        break;
    case GL_PIXEL_PACK_BUFFER:
        yagl_gles_buffer_acquire(ctx->pack.pbo);
        *buffer = ctx->pack.pbo;
        break;
    case GL_PIXEL_UNPACK_BUFFER:
        yagl_gles_buffer_acquire(ctx->unpack.pbo);
        *buffer = ctx->unpack.pbo;
        break;
    default:
        return ctx->acquire_binded_buffer(ctx, target, buffer);
    }

    return 1;
}

int yagl_gles_context_acquire_binded_framebuffer(struct yagl_gles_context *ctx,
                                                 GLenum target,
                                                 struct yagl_gles_framebuffer **fb)
{
    switch (target) {
    case GL_FRAMEBUFFER:
    case GL_DRAW_FRAMEBUFFER:
        yagl_gles_framebuffer_acquire(ctx->fbo_draw);
        *fb = ctx->fbo_draw;
        break;
    case GL_READ_FRAMEBUFFER:
        yagl_gles_framebuffer_acquire(ctx->fbo_read);
        *fb = ctx->fbo_read;
        break;
    default:
        return 0;
    }

    return 1;
}

int yagl_gles_context_acquire_binded_renderbuffer(struct yagl_gles_context *ctx,
                                                  GLenum target,
                                                  struct yagl_gles_renderbuffer **rb)
{
    switch (target) {
    case GL_RENDERBUFFER:
        yagl_gles_renderbuffer_acquire(ctx->rbo);
        *rb = ctx->rbo;
        break;
    default:
        return 0;
    }

    return 1;
}

void yagl_gles_context_enable(struct yagl_gles_context *ctx,
                              GLenum cap,
                              GLboolean enable)
{
    YAGL_LOG_FUNC_SET(yagl_gles_context_enable);

    switch (cap) {
    case GL_BLEND:
        ctx->blend_enabled = enable;
        break;
    case GL_CULL_FACE:
        ctx->cull_face_enabled = enable;
        break;
    case GL_DEPTH_TEST:
        ctx->depth_test_enabled = enable;
        break;
    case GL_DITHER:
        ctx->dither_enabled = enable;
        break;
    case GL_POLYGON_OFFSET_FILL:
        ctx->polygon_offset_fill_enabled = enable;
        break;
    case GL_SAMPLE_ALPHA_TO_COVERAGE:
        ctx->sample_alpha_to_coverage_enabled = enable;
        break;
    case GL_SAMPLE_COVERAGE:
        ctx->sample_coverage_enabled = enable;
        break;
    case GL_SCISSOR_TEST:
        ctx->scissor_test_enabled = enable;
        break;
    case GL_STENCIL_TEST:
        ctx->stencil_test_enabled = enable;
        break;
    default:
        if (!ctx->enable(ctx, cap, enable)) {
            YAGL_SET_ERR(GL_INVALID_ENUM);
            return;
        }
        break;
    }

    if (enable) {
        yagl_host_glEnable(cap);
    } else {
        yagl_host_glDisable(cap);
    }
}

GLboolean yagl_gles_context_is_enabled(struct yagl_gles_context *ctx,
                                       GLenum cap)
{
    GLboolean res = GL_FALSE;

    YAGL_LOG_FUNC_SET(glIsEnabled);

    switch (cap) {
    case GL_BLEND:
        res = ctx->blend_enabled;
        break;
    case GL_CULL_FACE:
        res = ctx->cull_face_enabled;
        break;
    case GL_DEPTH_TEST:
        res = ctx->depth_test_enabled;
        break;
    case GL_DITHER:
        res = ctx->dither_enabled;
        break;
    case GL_POLYGON_OFFSET_FILL:
        res = ctx->polygon_offset_fill_enabled;
        break;
    case GL_SAMPLE_ALPHA_TO_COVERAGE:
        res = ctx->sample_alpha_to_coverage_enabled;
        break;
    case GL_SAMPLE_COVERAGE:
        res = ctx->sample_coverage_enabled;
        break;
    case GL_SCISSOR_TEST:
        res = ctx->scissor_test_enabled;
        break;
    case GL_STENCIL_TEST:
        res = ctx->stencil_test_enabled;
        break;
    default:
        if (!ctx->is_enabled(ctx, cap, &res)) {
            YAGL_SET_ERR(GL_INVALID_ENUM);
            return GL_FALSE;
        }
        break;
    }

    return res;
}

int yagl_gles_context_get_integerv(struct yagl_gles_context *ctx,
                                   GLenum pname,
                                   GLint *params,
                                   uint32_t *num_params)
{
    int processed = 1;
    struct yagl_gles_texture_target_state *tts;
    GLenum internalformat;

    switch (pname) {
    case GL_ACTIVE_TEXTURE:
        *params = GL_TEXTURE0 + ctx->active_texture_unit;
        *num_params = 1;
        break;
    case GL_TEXTURE_BINDING_2D:
        tts = yagl_gles_context_get_active_texture_target_state(ctx,
            yagl_gles_texture_target_2d);
        *params = tts->texture->base.local_name;
        *num_params = 1;
        break;
    case GL_ARRAY_BUFFER_BINDING:
        *params = ctx->vbo ? ctx->vbo->base.local_name : 0;
        *num_params = 1;
        break;
    case GL_ELEMENT_ARRAY_BUFFER_BINDING:
        *params = ctx->vao->ebo ? ctx->vao->ebo->base.local_name : 0;
        *num_params = 1;
        break;
    case GL_FRAMEBUFFER_BINDING:
        *params = ctx->fbo_draw ? ctx->fbo_draw->base.local_name : 0;
        *num_params = 1;
        break;
    case GL_READ_FRAMEBUFFER_BINDING:
        *params = ctx->fbo_read ? ctx->fbo_read->base.local_name : 0;
        *num_params = 1;
        break;
    case GL_RENDERBUFFER_BINDING:
        *params = ctx->rbo ? ctx->rbo->base.local_name : 0;
        *num_params = 1;
        break;
    case GL_BLEND_EQUATION_RGB:
        *params = ctx->blend_equation_rgb;
        *num_params = 1;
        break;
    case GL_BLEND_EQUATION_ALPHA:
        *params = ctx->blend_equation_alpha;
        *num_params = 1;
        break;
    case GL_BLEND_SRC_RGB:
        *params = ctx->blend_src_rgb;
        *num_params = 1;
        break;
    case GL_BLEND_DST_RGB:
        *params = ctx->blend_dst_rgb;
        *num_params = 1;
        break;
    case GL_BLEND_SRC_ALPHA:
        *params = ctx->blend_src_alpha;
        *num_params = 1;
        break;
    case GL_BLEND_DST_ALPHA:
        *params = ctx->blend_dst_alpha;
        *num_params = 1;
        break;
    case GL_COLOR_WRITEMASK:
        params[0] = ctx->color_writemask[0];
        params[1] = ctx->color_writemask[1];
        params[2] = ctx->color_writemask[2];
        params[3] = ctx->color_writemask[3];
        *num_params = 4;
        break;
    case GL_CULL_FACE_MODE:
        *params = ctx->cull_face_mode;
        *num_params = 1;
        break;
    case GL_DEPTH_FUNC:
        *params = ctx->depth_func;
        *num_params = 1;
        break;
    case GL_DEPTH_WRITEMASK:
        *params = ctx->depth_writemask;
        *num_params = 1;
        break;
    case GL_FRONT_FACE:
        *params = ctx->front_face_mode;
        *num_params = 1;
        break;
    case GL_PACK_ALIGNMENT:
        *params = ctx->pack.alignment;
        *num_params = 1;
        break;
    case GL_PACK_IMAGE_HEIGHT:
        *params = ctx->pack.image_height;
        *num_params = 1;
        break;
    case GL_PACK_ROW_LENGTH:
        *params = ctx->pack.row_length;
        *num_params = 1;
        break;
    case GL_PACK_SKIP_IMAGES:
        *params = ctx->pack.skip_images;
        *num_params = 1;
        break;
    case GL_PACK_SKIP_PIXELS:
        *params = ctx->pack.skip_pixels;
        *num_params = 1;
        break;
    case GL_PACK_SKIP_ROWS:
        *params = ctx->pack.skip_rows;
        *num_params = 1;
        break;
    case GL_UNPACK_ALIGNMENT:
        *params = ctx->unpack.alignment;
        *num_params = 1;
        break;
    case GL_UNPACK_IMAGE_HEIGHT:
        *params = ctx->unpack.image_height;
        *num_params = 1;
        break;
    case GL_UNPACK_ROW_LENGTH:
        *params = ctx->unpack.row_length;
        *num_params = 1;
        break;
    case GL_UNPACK_SKIP_IMAGES:
        *params = ctx->unpack.skip_images;
        *num_params = 1;
        break;
    case GL_UNPACK_SKIP_PIXELS:
        *params = ctx->unpack.skip_pixels;
        *num_params = 1;
        break;
    case GL_UNPACK_SKIP_ROWS:
        *params = ctx->unpack.skip_rows;
        *num_params = 1;
        break;
    case GL_IMPLEMENTATION_COLOR_READ_FORMAT:
        if (yagl_gles_context_validate_readbuffer(ctx, &internalformat)) {
            const struct yagl_gles_format_info *format_info = yagl_gles_internalformat_info(internalformat);

            if (((format_info->flags & yagl_gles_format_unsigned_integer) != 0) ||
                ((format_info->flags & yagl_gles_format_signed_integer) != 0)) {
                *params = GL_RGBA_INTEGER;
                *num_params = 1;
                break;
            }
        }
        *params = GL_RGBA;
        *num_params = 1;
        break;
    case GL_IMPLEMENTATION_COLOR_READ_TYPE:
        if (yagl_gles_context_validate_readbuffer(ctx, &internalformat)) {
            const struct yagl_gles_format_info *format_info = yagl_gles_internalformat_info(internalformat);

            if ((format_info->flags & yagl_gles_format_unsigned_integer) != 0) {
                *params = GL_UNSIGNED_INT;
                *num_params = 1;
                break;
            }

            if ((format_info->flags & yagl_gles_format_signed_integer) != 0) {
                *params = GL_INT;
                *num_params = 1;
                break;
            }
        }
        *params = GL_UNSIGNED_BYTE;
        *num_params = 1;
        break;
    case GL_VIEWPORT:
        if (ctx->have_viewport) {
            params[0] = ctx->viewport[0];
            params[1] = ctx->viewport[1];
            params[2] = ctx->viewport[2];
            params[3] = ctx->viewport[3];
            *num_params = 4;
        } else {
            processed = 0;
        }
        break;
    case GL_VERTEX_ARRAY_BINDING:
        if (ctx->vertex_arrays_supported) {
            *params = (ctx->vao != ctx->va_zero) ? ctx->vao->base.local_name : 0;
            *num_params = 1;
        } else {
            return 0;
        }
        break;
    case GL_MAX_COLOR_ATTACHMENTS:
        *params = ctx->max_color_attachments;
        *num_params = 1;
        break;
    case GL_MAX_DRAW_BUFFERS:
        *params = ctx->max_draw_buffers;
        *num_params = 1;
        break;
    case GL_PIXEL_PACK_BUFFER_BINDING:
        *params = ctx->pack.pbo ? ctx->pack.pbo->base.local_name : 0;
        *num_params = 1;
        break;
    case GL_PIXEL_UNPACK_BUFFER_BINDING:
        *params = ctx->unpack.pbo ? ctx->unpack.pbo->base.local_name : 0;
        *num_params = 1;
        break;
    case GL_READ_BUFFER:
        *params = ctx->fbo_read ? ctx->fbo_read->read_buffer : ctx->fb0_read_buffer;
        *num_params = 1;
        break;
    case GL_ALPHA_BITS:
        if (yagl_gles_context_validate_drawbuffer(ctx, &internalformat)) {
            const struct yagl_gles_format_info *format_info = yagl_gles_internalformat_info(internalformat);
            *params = format_info->alpha_size;
        } else {
            *params = 0;
        }
        *num_params = 1;
        break;
    case GL_BLUE_BITS:
        if (yagl_gles_context_validate_drawbuffer(ctx, &internalformat)) {
            const struct yagl_gles_format_info *format_info = yagl_gles_internalformat_info(internalformat);
            *params = format_info->blue_size;
        } else {
            *params = 0;
        }
        *num_params = 1;
        break;
    case GL_GREEN_BITS:
        if (yagl_gles_context_validate_drawbuffer(ctx, &internalformat)) {
            const struct yagl_gles_format_info *format_info = yagl_gles_internalformat_info(internalformat);
            *params = format_info->green_size;
        } else {
            *params = 0;
        }
        *num_params = 1;
        break;
    case GL_RED_BITS:
        if (yagl_gles_context_validate_drawbuffer(ctx, &internalformat)) {
            const struct yagl_gles_format_info *format_info = yagl_gles_internalformat_info(internalformat);
            *params = format_info->red_size;
        } else {
            *params = 0;
        }
        *num_params = 1;
        break;
    case GL_STENCIL_BITS:
        if (ctx->fbo_draw) {
            struct yagl_gles_framebuffer_attachment_state *attachment_state;

            attachment_state = &ctx->fbo_draw->attachment_states[yagl_gles_framebuffer_attachment_stencil];

            if (yagl_gles_framebuffer_attachment_internalformat(attachment_state,
                                                                &internalformat)) {
                const struct yagl_gles_format_info *format_info = yagl_gles_internalformat_info(internalformat);
                *params = format_info->stencil_size;
            } else {
                *params = 0;
            }
        } else {
            *params = 8;
        }
        *num_params = 1;
        break;
    case GL_DEPTH_BITS:
        if (ctx->fbo_draw) {
            struct yagl_gles_framebuffer_attachment_state *attachment_state;

            attachment_state = &ctx->fbo_draw->attachment_states[yagl_gles_framebuffer_attachment_depth];

            if (yagl_gles_framebuffer_attachment_internalformat(attachment_state,
                                                                &internalformat)) {
                const struct yagl_gles_format_info *format_info = yagl_gles_internalformat_info(internalformat);
                *params = format_info->depth_size;
            } else {
                *params = 0;
            }
        } else {
            *params = 24;
        }
        *num_params = 1;
        break;
    case GL_BLEND:
        *params = ctx->blend_enabled;
        *num_params = 1;
        break;
    case GL_CULL_FACE:
        *params = ctx->cull_face_enabled;
        *num_params = 1;
        break;
    case GL_DEPTH_TEST:
        *params = ctx->depth_test_enabled;
        *num_params = 1;
        break;
    case GL_POLYGON_OFFSET_FILL:
        *params = ctx->polygon_offset_fill_enabled;
        *num_params = 1;
        break;
    case GL_SAMPLE_ALPHA_TO_COVERAGE:
        *params = ctx->sample_alpha_to_coverage_enabled;
        *num_params = 1;
        break;
    case GL_SAMPLE_COVERAGE:
        *params = ctx->sample_coverage_enabled;
        *num_params = 1;
        break;
    case GL_SCISSOR_TEST:
        *params = ctx->scissor_test_enabled;
        *num_params = 1;
        break;
    case GL_STENCIL_TEST:
        *params = ctx->stencil_test_enabled;
        *num_params = 1;
        break;
    case GL_GENERATE_MIPMAP_HINT:
        *params = ctx->generate_mipmap_hint;
        *num_params = 1;
        break;
    case GL_SAMPLE_COVERAGE_INVERT:
        *params = ctx->sample_coverage_invert;
        *num_params = 1;
        break;
    case GL_STENCIL_CLEAR_VALUE:
        *params = ctx->clear_stencil;
        *num_params = 1;
        break;
    case GL_STENCIL_FAIL:
        *params = ctx->stencil_front.fail;
        *num_params = 1;
        break;
    case GL_STENCIL_FUNC:
        *params = ctx->stencil_front.func;
        *num_params = 1;
        break;
    case GL_STENCIL_PASS_DEPTH_FAIL:
        *params = ctx->stencil_front.zfail;
        *num_params = 1;
        break;
    case GL_STENCIL_PASS_DEPTH_PASS:
        *params = ctx->stencil_front.zpass;
        *num_params = 1;
        break;
    case GL_STENCIL_REF:
        *params = ctx->stencil_front.ref;
        *num_params = 1;
        break;
    case GL_STENCIL_VALUE_MASK:
        *params = ctx->stencil_front.mask;
        *num_params = 1;
        break;
    case GL_STENCIL_WRITEMASK:
        *params = ctx->stencil_front.writemask;
        *num_params = 1;
        break;
    case GL_MAX_RENDERBUFFER_SIZE:
        if (ctx->have_max_renderbuffer_size) {
            *params = ctx->max_renderbuffer_size;
            *num_params = 1;
        } else {
            processed = 0;
        }
        break;
    case GL_MAX_VIEWPORT_DIMS:
        if (ctx->have_max_viewport_dims) {
            params[0] = ctx->max_viewport_dims[0];
            params[1] = ctx->max_viewport_dims[1];
            *num_params = 2;
        } else {
            processed = 0;
        }
        break;
    case GL_SAMPLE_BUFFERS:
        if (ctx->have_sample_buffers) {
            *params = ctx->sample_buffers;
            *num_params = 1;
        } else {
            processed = 0;
        }
        break;
    case GL_SAMPLES:
        if (ctx->have_samples) {
            *params = ctx->samples;
            *num_params = 1;
        } else {
            processed = 0;
        }
        break;
    case GL_SCISSOR_BOX:
        if (ctx->have_scissor_box) {
            params[0] = ctx->scissor_box[0];
            params[1] = ctx->scissor_box[1];
            params[2] = ctx->scissor_box[2];
            params[3] = ctx->scissor_box[3];
            *num_params = 4;
        } else {
            processed = 0;
        }
        break;
    case GL_SUBPIXEL_BITS:
        if (ctx->have_subpixel_bits) {
            *params = ctx->subpixel_bits;
            *num_params = 1;
        } else {
            processed = 0;
        }
        break;
    default:
        if ((pname >= GL_DRAW_BUFFER0) &&
            (pname <= (GL_DRAW_BUFFER0 + ctx->max_draw_buffers - 1))) {
            if (ctx->fbo_draw) {
                *params = ctx->fbo_draw->draw_buffers[pname - GL_DRAW_BUFFER0];
                *num_params = 1;
            } else if (pname == GL_DRAW_BUFFER0) {
                *params = ctx->fb0_draw_buffer;
                *num_params = 1;
            } else {
                *params = GL_NONE;
                *num_params = 1;
            }
            break;
        }

        processed = 0;
        break;
    }

    if (processed) {
        return 1;
    }

    switch (pname) {
    case GL_MAX_RENDERBUFFER_SIZE:
        *num_params = 1;
        yagl_host_glGetIntegerv(pname, params, *num_params, NULL);
        ctx->max_renderbuffer_size = *params;
        ctx->have_max_renderbuffer_size = 1;
        break;
    case GL_MAX_VIEWPORT_DIMS:
        *num_params = 2;
        yagl_host_glGetIntegerv(pname, params, *num_params, NULL);
        ctx->max_viewport_dims[0] = params[0];
        ctx->max_viewport_dims[1] = params[1];
        ctx->have_max_viewport_dims = 1;
        break;
    case GL_SAMPLE_BUFFERS:
        *num_params = 1;
        yagl_host_glGetIntegerv(pname, params, *num_params, NULL);
        ctx->sample_buffers = *params;
        ctx->have_sample_buffers = 1;
        break;
    case GL_SAMPLES:
        *num_params = 1;
        yagl_host_glGetIntegerv(pname, params, *num_params, NULL);
        ctx->samples = *params;
        ctx->have_samples = 1;
        break;
    case GL_SCISSOR_BOX:
        *num_params = 4;
        yagl_host_glGetIntegerv(pname, params, *num_params, NULL);
        ctx->scissor_box[0] = params[0];
        ctx->scissor_box[1] = params[1];
        ctx->scissor_box[2] = params[2];
        ctx->scissor_box[3] = params[3];
        ctx->have_scissor_box = 1;
        break;
    case GL_SUBPIXEL_BITS:
        *num_params = 1;
        yagl_host_glGetIntegerv(pname, params, *num_params, NULL);
        ctx->subpixel_bits = *params;
        ctx->have_subpixel_bits = 1;
        break;
    case GL_VIEWPORT:
        *num_params = 4;
        yagl_host_glGetIntegerv(pname, params, *num_params, NULL);
        ctx->viewport[0] = params[0];
        ctx->viewport[1] = params[1];
        ctx->viewport[2] = params[2];
        ctx->viewport[3] = params[3];
        ctx->have_viewport = 1;
        break;
    default:
        return ctx->get_integerv(ctx, pname, params, num_params);
    }

    return 1;
}

int yagl_gles_context_get_floatv(struct yagl_gles_context *ctx,
                                 GLenum pname,
                                 GLfloat *params,
                                 uint32_t *num_params,
                                 int *needs_map)
{
    int processed = 1;

    switch (pname) {
    case GL_COLOR_CLEAR_VALUE:
        params[0] = ctx->clear_color[0];
        params[1] = ctx->clear_color[1];
        params[2] = ctx->clear_color[2];
        params[3] = ctx->clear_color[3];
        *num_params = 4;
        *needs_map = 1;
        break;
    case GL_DEPTH_CLEAR_VALUE:
        *params = ctx->clear_depth;
        *num_params = 1;
        *needs_map = 1;
        break;
    case GL_DEPTH_RANGE:
        params[0] = ctx->depth_range[0];
        params[1] = ctx->depth_range[1];
        *num_params = 2;
        *needs_map = 1;
        break;
    case GL_SAMPLE_COVERAGE_VALUE:
        *params = ctx->sample_coverage_value;
        *num_params = 1;
        break;
    case GL_LINE_WIDTH:
        *params = ctx->line_width;
        *num_params = 1;
        break;
    case GL_POLYGON_OFFSET_FACTOR:
        *params = ctx->polygon_offset_factor;
        *num_params = 1;
        break;
    case GL_POLYGON_OFFSET_UNITS:
        *params = ctx->polygon_offset_units;
        *num_params = 1;
        break;
    case GL_ALIASED_LINE_WIDTH_RANGE:
        if (ctx->have_line_width_range) {
            params[0] = ctx->line_width_range[0];
            params[1] = ctx->line_width_range[1];
            *num_params = 2;
        } else {
            processed = 0;
        }
        break;
    case GL_ALIASED_POINT_SIZE_RANGE:
        if (ctx->have_point_size_range) {
            params[0] = ctx->point_size_range[0];
            params[1] = ctx->point_size_range[1];
            *num_params = 2;
        } else {
            processed = 0;
        }
        break;
    default:
        processed = 0;
        break;
    }

    if (processed) {
        return 1;
    }

    switch (pname) {
    case GL_ALIASED_LINE_WIDTH_RANGE:
        *num_params = 2;
        yagl_host_glGetFloatv(pname, params, *num_params, NULL);
        ctx->line_width_range[0] = params[0];
        ctx->line_width_range[1] = params[1];
        ctx->have_line_width_range = 1;
        break;
    case GL_ALIASED_POINT_SIZE_RANGE:
        *num_params = 2;
        yagl_host_glGetFloatv(pname, params, *num_params, NULL);
        ctx->point_size_range[0] = params[0];
        ctx->point_size_range[1] = params[1];
        ctx->have_point_size_range = 1;
        break;
    default:
        return ctx->get_floatv(ctx, pname, params, num_params, needs_map);
    }

    return 1;
}

void yagl_gles_context_draw_arrays(struct yagl_gles_context *ctx,
                                   GLenum mode, GLint first, GLsizei count,
                                   GLsizei primcount)
{
    GLuint i;

    YAGL_LOG_FUNC_SET(yagl_gles_context_draw_arrays);

    if (!yagl_gles_is_draw_mode_valid(mode)) {
        YAGL_SET_ERR(GL_INVALID_ENUM);
        return;
    }

    if ((first < 0) || (count < 0)) {
        YAGL_SET_ERR(GL_INVALID_VALUE);
        return;
    }

    if ((count == 0) || (primcount == 0)) {
        return;
    }

    yagl_render_invalidate(0);

    for (i = 0; i < ctx->vao->num_arrays; ++i) {
        yagl_gles_array_transfer(&ctx->vao->arrays[i],
                                 first,
                                 count,
                                 primcount);
    }

    ctx->draw_arrays(ctx, mode, first, count, primcount);
}

void yagl_gles_context_draw_elements(struct yagl_gles_context *ctx,
                                     GLenum mode, GLsizei count,
                                     GLenum type, const GLvoid *indices,
                                     GLsizei primcount)
{
    int index_size = 0;
    int have_range = 0;
    uint32_t min_idx = 0, max_idx = 0;
    GLuint i;

    YAGL_LOG_FUNC_SET(yagl_gles_context_draw_elements);

    if (!yagl_gles_is_draw_mode_valid(mode)) {
        YAGL_SET_ERR(GL_INVALID_ENUM);
        return;
    }

    if (count < 0) {
        YAGL_SET_ERR(GL_INVALID_VALUE);
        return;
    }

    if (!yagl_gles_get_index_size(type, &index_size)) {
        YAGL_SET_ERR(GL_INVALID_ENUM);
        return;
    }

    if (!ctx->vao->ebo && !indices) {
        YAGL_SET_ERR(GL_INVALID_VALUE);
        return;
    }

    if ((count == 0) || (primcount == 0)) {
        return;
    }

    yagl_render_invalidate(0);

    for (i = 0; i < ctx->vao->num_arrays; ++i) {
        if (!ctx->vao->arrays[i].enabled) {
            continue;
        }

        if (!have_range) {
            if (ctx->vao->ebo) {
                if (!yagl_gles_buffer_get_minmax_index(ctx->vao->ebo,
                                                       type,
                                                       (GLint)indices,
                                                       count,
                                                       &min_idx,
                                                       &max_idx)) {
                    YAGL_LOG_WARN("unable to get min/max index from ebo");
                    return;
                }
            } else {
                yagl_get_minmax_index(indices, count, type,
                                      &min_idx, &max_idx);
            }
            have_range = 1;
        }

        yagl_gles_array_transfer(&ctx->vao->arrays[i],
                                 min_idx,
                                 max_idx + 1 - min_idx,
                                 primcount);
    }

    if (!have_range) {
        return;
    }

    if (ctx->vao->ebo) {
        yagl_gles_buffer_bind(ctx->vao->ebo,
                              type,
                              0,
                              GL_ELEMENT_ARRAY_BUFFER);
        yagl_gles_buffer_transfer(ctx->vao->ebo,
                                  type,
                                  GL_ELEMENT_ARRAY_BUFFER,
                                  0);
        ctx->draw_elements(ctx, mode, count, type, NULL, (int32_t)indices, primcount, max_idx);
        yagl_host_glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    } else {
        ctx->draw_elements(ctx, mode, count, type, indices, count * index_size, primcount, max_idx);
    }
}

int yagl_gles_context_pre_unpack(struct yagl_gles_context *ctx,
                                 const GLvoid **pixels,
                                 int need_convert,
                                 int *using_pbo)
{
    *using_pbo = 0;

    if (!ctx->unpack.pbo) {
        return 1;
    }

    if (!need_convert) {
        yagl_gles_buffer_bind(ctx->unpack.pbo,
                              0,
                              0,
                              GL_PIXEL_UNPACK_BUFFER);
        yagl_gles_buffer_transfer(ctx->unpack.pbo,
                                  0,
                                  GL_PIXEL_UNPACK_BUFFER,
                                  0);

        *using_pbo = 1;

        return 1;
    }

    if (!yagl_gles_buffer_map(ctx->unpack.pbo,
                              0, ctx->unpack.pbo->size,
                              GL_MAP_READ_BIT)) {
        return 0;
    }

    *pixels = ctx->unpack.pbo->map_pointer + (uint32_t)*pixels;

    return 1;
}

void yagl_gles_context_post_unpack(struct yagl_gles_context *ctx,
                                   int need_convert)
{
    if (!ctx->unpack.pbo) {
        return;
    }

    if (need_convert) {
        yagl_gles_buffer_unmap(ctx->unpack.pbo);
    } else {
        yagl_host_glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
    }
}

int yagl_gles_context_pre_pack(struct yagl_gles_context *ctx,
                               GLvoid **pixels,
                               int need_convert,
                               int *using_pbo)
{
    *using_pbo = 0;

    if (!ctx->pack.pbo) {
        return 1;
    }

    if (!need_convert) {
        yagl_gles_buffer_bind(ctx->pack.pbo,
                              0,
                              0,
                              GL_PIXEL_PACK_BUFFER);
        yagl_gles_buffer_transfer(ctx->pack.pbo,
                                  0,
                                  GL_PIXEL_PACK_BUFFER,
                                  0);

        *using_pbo = 1;

        return 1;
    }

    if (!yagl_gles_buffer_map(ctx->pack.pbo,
                              0, ctx->pack.pbo->size,
                              GL_MAP_WRITE_BIT)) {
        return 0;
    }

    *pixels = ctx->pack.pbo->map_pointer + (uint32_t)*pixels;

    return 1;
}

void yagl_gles_context_post_pack(struct yagl_gles_context *ctx,
                                 GLvoid *pixels,
                                 GLsizei size,
                                 int need_convert)
{
    if (!ctx->pack.pbo) {
        return;
    }

    if (need_convert) {
        yagl_gles_buffer_unmap(ctx->pack.pbo);
    } else {
        yagl_host_glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
        yagl_gles_buffer_set_gpu_dirty(ctx->pack.pbo,
                                       (GLint)pixels,
                                       size);
    }
}

void yagl_gles_context_line_width(struct yagl_gles_context *ctx,
                                  GLfloat width)
{
    ctx->line_width = width;

    yagl_host_glLineWidth(width);
}

void yagl_gles_context_tex_parameterf(struct yagl_gles_context *ctx,
                                      GLenum target,
                                      GLenum pname,
                                      GLfloat param)
{
    yagl_gles_texture_target texture_target;
    struct yagl_gles_texture_target_state *tex_target_state;

    YAGL_LOG_FUNC_SET(glTexParameterf);

    if (!yagl_gles_context_validate_texture_target(ctx,
                                                   target,
                                                   &texture_target)) {
        YAGL_SET_ERR(GL_INVALID_ENUM);
        return;
    }

    tex_target_state =
        yagl_gles_context_get_active_texture_target_state(ctx, texture_target);

    if (pname == GL_TEXTURE_MIN_FILTER) {
        tex_target_state->texture->min_filter = param;
    } else if (pname == GL_TEXTURE_MAG_FILTER) {
        tex_target_state->texture->mag_filter = param;
    }

    yagl_host_glTexParameterf(target, pname, param);
}

void yagl_gles_context_tex_parameterfv(struct yagl_gles_context *ctx,
                                       GLenum target,
                                       GLenum pname,
                                       const GLfloat *params)
{
    yagl_gles_texture_target texture_target;
    struct yagl_gles_texture_target_state *tex_target_state;

    YAGL_LOG_FUNC_SET(glTexParameterfv);

    if (!yagl_gles_context_validate_texture_target(ctx,
                                                   target,
                                                   &texture_target)) {
        YAGL_SET_ERR(GL_INVALID_ENUM);
        return;
    }

    tex_target_state =
        yagl_gles_context_get_active_texture_target_state(ctx, texture_target);

    if (pname == GL_TEXTURE_MIN_FILTER) {
        tex_target_state->texture->min_filter = params[0];
    } else if (pname == GL_TEXTURE_MAG_FILTER) {
        tex_target_state->texture->mag_filter = params[0];
    }

    yagl_host_glTexParameterfv(target, pname, params, 1);
}

int yagl_gles_context_get_tex_parameterfv(struct yagl_gles_context *ctx,
                                          GLenum target,
                                          GLenum pname,
                                          GLfloat *params)
{
    yagl_gles_texture_target texture_target;
    struct yagl_gles_texture *texture_obj;

    if (!yagl_gles_context_validate_texture_target(ctx, target, &texture_target)) {
        return 0;
    }

    texture_obj = yagl_gles_context_get_active_texture_target_state(ctx,
                                                                    texture_target)->texture;

    switch (pname) {
    case GL_TEXTURE_IMMUTABLE_FORMAT:
        params[0] = texture_obj->immutable;
        return 1;
    default:
        break;
    }

    yagl_host_glGetTexParameterfv(target, pname, params);

    return 1;
}

int yagl_gles_context_get_tex_parameteriv(struct yagl_gles_context *ctx,
                                          GLenum target,
                                          GLenum pname,
                                          GLint *params)
{
    yagl_gles_texture_target texture_target;
    struct yagl_gles_texture *texture_obj;

    if (!yagl_gles_context_validate_texture_target(ctx, target, &texture_target)) {
        return 0;
    }

    texture_obj = yagl_gles_context_get_active_texture_target_state(ctx,
                                                                    texture_target)->texture;

    switch (pname) {
    case GL_TEXTURE_IMMUTABLE_FORMAT:
        params[0] = texture_obj->immutable;
        return 1;
    default:
        break;
    }

    yagl_host_glGetTexParameteriv(target, pname, params);

    return 1;
}

void yagl_gles_context_clear_color(struct yagl_gles_context *ctx,
                                   GLclampf red,
                                   GLclampf green,
                                   GLclampf blue,
                                   GLclampf alpha)
{
    ctx->clear_color[0] = yagl_clampf(red);
    ctx->clear_color[1] = yagl_clampf(green);
    ctx->clear_color[2] = yagl_clampf(blue);
    ctx->clear_color[3] = yagl_clampf(alpha);

    yagl_host_glClearColor(red, green, blue, alpha);
}

void yagl_gles_context_clear_depthf(struct yagl_gles_context *ctx,
                                    GLclampf depth)
{
    ctx->clear_depth = yagl_clampf(depth);

    yagl_host_glClearDepthf(depth);
}

void yagl_gles_context_sample_coverage(struct yagl_gles_context *ctx,
                                       GLclampf value,
                                       GLboolean invert)
{
    ctx->sample_coverage_value = yagl_clampf(value);
    ctx->sample_coverage_invert = invert;

    yagl_host_glSampleCoverage(value, invert);
}

void yagl_gles_context_depth_rangef(struct yagl_gles_context *ctx,
                                    GLclampf zNear,
                                    GLclampf zFar)
{
    ctx->depth_range[0] = yagl_clampf(zNear);
    ctx->depth_range[1] = yagl_clampf(zFar);

    yagl_host_glDepthRangef(zNear, zFar);
}

void yagl_gles_context_polygon_offset(struct yagl_gles_context *ctx,
                                      GLfloat factor,
                                      GLfloat units)
{
    ctx->polygon_offset_factor = factor;
    ctx->polygon_offset_units = units;

    yagl_host_glPolygonOffset(factor, units);
}

void yagl_gles_context_hint(struct yagl_gles_context *ctx,
                            GLenum target,
                            GLenum mode)
{
    switch (target) {
    case GL_GENERATE_MIPMAP_HINT:
        ctx->generate_mipmap_hint = mode;
        break;
    default:
        ctx->hint(ctx, target, mode);
        break;
    }

    yagl_host_glHint(target, mode);
}
