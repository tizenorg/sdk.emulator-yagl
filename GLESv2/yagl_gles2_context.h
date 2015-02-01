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

#ifndef _YAGL_GLES2_CONTEXT_H_
#define _YAGL_GLES2_CONTEXT_H_

#include "yagl_gles_context.h"

struct yagl_gles2_program;

struct yagl_gles2_context
{
    struct yagl_gles_context base;

    int (*get_programiv)(struct yagl_gles2_context */*ctx*/,
                         struct yagl_gles2_program */*program*/,
                         GLenum /*pname*/,
                         GLint */*params*/);

    int (*pre_use_program)(struct yagl_gles2_context */*ctx*/,
                           struct yagl_gles2_program */*program*/);

    int (*pre_link_program)(struct yagl_gles2_context */*ctx*/,
                            struct yagl_gles2_program */*program*/);

    /*
     * From 'base.base.sg'.
     */
    struct yagl_sharegroup *sg;

    /*
     * Host OpenGL (version < 3.1) vertex attribute 0 is "magic", it has to be
     * always enabled as a vertex attribute array. This means we have to
     * keep some buffer around and on each drawing call:
     * + Check if vertex attribute array 0 is enabled, if not then
     * + Resize this buffer to fit the number of elements to draw and
     *   stuff it with vertex attribute 0 value
     * + Call glEnableVertexAttribArray + glVertexAttribPointer
     * + Draw
     * + Restore everything back to normal.
     * This is really heavy, but luckily almost everyone uses vertex attribute
     * array 0, so this code will be triggered on very rare occasions, like
     * khronos conformance test.
     * Also, Host OpenGL 3.1+ says that vertex attribute 0
     * "is not magic anymore", but this is
     * currently broken in many drivers, so we have to workaround even
     * when we're running with OpenGL 3.1+ core...
     */
    struct
    {
        union
        {
            /*
             * We assume that all these arrays
             * are equal in size.
             */
            GLfloat f[4];
            GLint i[4];
            GLuint ui[4];
        } value;
        GLenum type;
        GLint count;
        struct yagl_gles_buffer *vbo;
        int warned;
    } vertex_attrib0;

    /*
     * Generate program uniform locations ourselves or vmexit
     * and ask host.
     */
    int gen_locations;

    int texture_half_float;

    int vertex_half_float;

    int standard_derivatives;

    int instanced_arrays;

    struct yagl_gles2_program *program;

    int have_max_texture_size;
    GLint max_texture_size;

    int have_max_cubemap_texture_size;
    GLint max_cubemap_texture_size;

    int have_max_samples_img;
    GLint max_samples_img;

    int have_max_texture_image_units;
    GLint max_texture_image_units;

    int have_max_texture_max_anisotropy;
    GLint max_texture_max_anisotropy;

    int have_max_vertex_texture_image_units;
    GLint max_vertex_texture_image_units;

    int have_max_3d_texture_size;
    GLint max_3d_texture_size;

    int have_max_fragment_uniform_components;
    GLint max_fragment_uniform_components;

    int have_max_varying_floats;
    GLint max_varying_floats;

    int have_max_vertex_uniform_components;
    GLint max_vertex_uniform_components;

    GLclampf blend_color[4];
};

/*
 * GLESv2/v3 common.
 * @{
 */

void yagl_gles2_context_init(struct yagl_gles2_context *ctx,
                             yagl_client_api client_api,
                             struct yagl_sharegroup *sg);

void yagl_gles2_context_cleanup(struct yagl_gles2_context *ctx);

void yagl_gles2_context_prepare(struct yagl_gles2_context *ctx);

const GLchar **yagl_gles2_context_get_extensions(struct yagl_gles2_context *ctx,
                                                 int *num_extensions);

struct yagl_gles_array
    *yagl_gles2_context_create_arrays(struct yagl_gles_context *ctx);

void yagl_gles2_context_pre_draw(struct yagl_gles2_context *ctx,
                                 GLenum mode,
                                 GLint count);

void yagl_gles2_context_post_draw(struct yagl_gles2_context *ctx,
                                  GLenum mode,
                                  GLint count);

void yagl_gles2_context_compressed_tex_image_2d(struct yagl_gles_context *ctx,
                                                GLenum target,
                                                struct yagl_gles_texture *texture,
                                                GLint level,
                                                GLenum internalformat,
                                                GLsizei width,
                                                GLsizei height,
                                                GLint border,
                                                GLsizei imageSize,
                                                const GLvoid *data);

void yagl_gles2_context_compressed_tex_sub_image_2d(struct yagl_gles_context *ctx,
                                                    GLenum target,
                                                    GLint level,
                                                    GLint xoffset,
                                                    GLint yoffset,
                                                    GLsizei width,
                                                    GLsizei height,
                                                    GLenum format,
                                                    GLsizei imageSize,
                                                    const GLvoid *data);

void yagl_gles2_context_compressed_tex_image_3d(struct yagl_gles2_context *ctx,
                                                GLenum target,
                                                struct yagl_gles_texture *texture,
                                                GLint level,
                                                GLenum internalformat,
                                                GLsizei width,
                                                GLsizei height,
                                                GLsizei depth,
                                                GLint border,
                                                GLsizei imageSize,
                                                const GLvoid *data);

void yagl_gles2_context_compressed_tex_sub_image_3d(struct yagl_gles2_context *ctx,
                                                    GLenum target,
                                                    GLint level,
                                                    GLint xoffset,
                                                    GLint yoffset,
                                                    GLint zoffset,
                                                    GLsizei width,
                                                    GLsizei height,
                                                    GLsizei depth,
                                                    GLenum format,
                                                    GLsizei imageSize,
                                                    const GLvoid *data);

int yagl_gles2_context_get_integerv(struct yagl_gles_context *ctx,
                                    GLenum pname,
                                    GLint *params,
                                    uint32_t *num_params);

int yagl_gles2_context_get_floatv(struct yagl_gles_context *ctx,
                                  GLenum pname,
                                  GLfloat *params,
                                  uint32_t *num_params,
                                  int *needs_map);

void yagl_gles2_context_draw_arrays(struct yagl_gles_context *ctx,
                                    GLenum mode,
                                    GLint first,
                                    GLsizei count,
                                    GLsizei primcount);

void yagl_gles2_context_draw_elements(struct yagl_gles_context *ctx,
                                      GLenum mode,
                                      GLsizei count,
                                      GLenum type,
                                      const GLvoid *indices,
                                      int32_t indices_count,
                                      GLsizei primcount,
                                      uint32_t max_idx);

int yagl_gles2_context_validate_texture_target(struct yagl_gles_context *ctx,
                                               GLenum target,
                                               yagl_gles_texture_target *texture_target);

struct yagl_pixel_format
    *yagl_gles2_context_validate_teximage_format(struct yagl_gles_context *ctx,
                                                 GLenum internalformat,
                                                 GLenum format,
                                                 GLenum type);

struct yagl_pixel_format
    *yagl_gles2_context_validate_getteximage_format(struct yagl_gles_context *ctx,
                                                    GLenum readbuffer_internalformat,
                                                    GLenum format,
                                                    GLenum type);

int yagl_gles2_context_validate_copyteximage_format(struct yagl_gles_context *ctx,
                                                    GLenum readbuffer_internalformat,
                                                    GLenum *internalformat);

int yagl_gles2_context_validate_texstorage_format(struct yagl_gles_context *ctx,
                                                  GLenum *internalformat,
                                                  GLenum *base_internalformat,
                                                  GLenum *any_format,
                                                  GLenum *any_type);

int yagl_gles2_context_validate_renderbuffer_format(struct yagl_gles_context *ctx,
                                                    GLenum *internalformat);

void yagl_gles2_context_hint(struct yagl_gles_context *ctx,
                             GLenum target,
                             GLenum mode);

int yagl_gles2_context_get_programiv(struct yagl_gles2_context *ctx,
                                     struct yagl_gles2_program *program,
                                     GLenum pname,
                                     GLint *params);

/*
 * @}
 */

struct yagl_client_context *yagl_gles2_context_create(struct yagl_sharegroup *sg);

void yagl_gles2_context_use_program(struct yagl_gles2_context *ctx,
                                    struct yagl_gles2_program *program);

void yagl_gles2_context_unuse_program(struct yagl_gles2_context *ctx,
                                      struct yagl_gles2_program *program);

int yagl_gles2_context_get_array_param(struct yagl_gles2_context *ctx,
                                       GLuint index,
                                       GLenum pname,
                                       GLint *param);

#endif
