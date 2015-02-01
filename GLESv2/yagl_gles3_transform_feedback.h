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

#ifndef _YAGL_GLES3_TRANSFORM_FEEDBACK_H_
#define _YAGL_GLES3_TRANSFORM_FEEDBACK_H_

#include "yagl_types.h"
#include "yagl_object.h"

struct yagl_gles3_buffer_binding;
struct yagl_gles_buffer;

struct yagl_gles3_transform_feedback
{
    struct yagl_object base;

    yagl_object_name global_name;

    struct yagl_gles3_buffer_binding *buffer_bindings;
    GLuint num_buffer_bindings;

    int active;
    int paused;

    GLuint num_active_buffer_bindings;

    int was_bound;
};

struct yagl_gles3_transform_feedback
    *yagl_gles3_transform_feedback_create(int fake,
                                          GLuint num_buffer_bindings);

/*
 * Passing NULL won't hurt, this is for convenience.
 */
void yagl_gles3_transform_feedback_acquire(struct yagl_gles3_transform_feedback *tf);

/*
 * Passing NULL won't hurt, this is for convenience.
 */
void yagl_gles3_transform_feedback_release(struct yagl_gles3_transform_feedback *tf);

int yagl_gles3_transform_feedback_bind_buffer_base(struct yagl_gles3_transform_feedback *tf,
                                                   GLuint index,
                                                   struct yagl_gles_buffer *buffer);

int yagl_gles3_transform_feedback_bind_buffer_range(struct yagl_gles3_transform_feedback *tf,
                                                    GLuint index,
                                                    GLintptr offset,
                                                    GLsizeiptr size,
                                                    struct yagl_gles_buffer *buffer);

void yagl_gles3_transform_feedback_unbind_buffer(struct yagl_gles3_transform_feedback *tf,
                                                 yagl_object_name buffer_local_name);

void yagl_gles3_transform_feedback_bind(struct yagl_gles3_transform_feedback *tf,
                                        GLenum target);

int yagl_gles3_transform_feedback_was_bound(struct yagl_gles3_transform_feedback *tf);

void yagl_gles3_transform_feedback_begin(struct yagl_gles3_transform_feedback *tf,
                                         GLenum primitive_mode,
                                         GLuint num_active_buffer_bindings);

void yagl_gles3_transform_feedback_pause(struct yagl_gles3_transform_feedback *tf);

void yagl_gles3_transform_feedback_resume(struct yagl_gles3_transform_feedback *tf);

void yagl_gles3_transform_feedback_post_draw(struct yagl_gles3_transform_feedback *tf);

void yagl_gles3_transform_feedback_end(struct yagl_gles3_transform_feedback *tf);

#endif
