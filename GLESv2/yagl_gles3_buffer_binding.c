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

#include "GLES3/gl3.h"
#include "yagl_gles3_buffer_binding.h"
#include "yagl_gles_buffer.h"
#include "yagl_host_gles_calls.h"
#include <assert.h>

static int yagl_gles3_buffer_binding_fixup(struct yagl_gles3_buffer_binding *buffer_binding,
                                           GLint *offset,
                                           GLsizei *size)
{
    *offset = buffer_binding->offset;
    *size = buffer_binding->size;

    if (*offset < 0) {
        *size += *offset;
        *offset = 0;
    }

    if ((*offset >= buffer_binding->buffer->size) || (*size <= 0)) {
        return 0;
    }

    if ((*offset + *size) > buffer_binding->buffer->size) {
        *size = buffer_binding->buffer->size - *offset;
    }

    return 1;
}

void yagl_gles3_buffer_binding_init(struct yagl_gles3_buffer_binding *buffer_binding,
                                    GLenum target,
                                    GLuint index)
{
    buffer_binding->target = target;
    buffer_binding->index = index;
    yagl_list_init(&buffer_binding->list);
}

void yagl_gles3_buffer_binding_reset(struct yagl_gles3_buffer_binding *buffer_binding)
{
    yagl_gles_buffer_release(buffer_binding->buffer);
    yagl_list_remove(&buffer_binding->list);
    buffer_binding->buffer = NULL;
    buffer_binding->entire = 0;
    buffer_binding->offset = 0;
    buffer_binding->size = 0;
}

void yagl_gles3_buffer_binding_set_base(struct yagl_gles3_buffer_binding *buffer_binding,
                                        struct yagl_gles_buffer *buffer)
{
    yagl_gles_buffer_acquire(buffer);
    yagl_gles3_buffer_binding_reset(buffer_binding);
    if (buffer) {
        buffer_binding->buffer = buffer;
        buffer_binding->entire = 1;
    }
}

void yagl_gles3_buffer_binding_set_range(struct yagl_gles3_buffer_binding *buffer_binding,
                                         struct yagl_gles_buffer *buffer,
                                         GLintptr offset,
                                         GLsizeiptr size)
{
    yagl_gles_buffer_acquire(buffer);
    yagl_gles3_buffer_binding_reset(buffer_binding);
    if (buffer) {
        buffer_binding->buffer = buffer;
        buffer_binding->offset = offset;
        buffer_binding->size = size;
    }
}

void yagl_gles3_buffer_binding_transfer_begin(struct yagl_gles3_buffer_binding *buffer_binding)
{
    GLint offset;
    GLsizei size;

    assert(buffer_binding->buffer);
    if (!buffer_binding->buffer) {
        return;
    }

    if (buffer_binding->buffer->size <= 0) {
        return;
    }

    if (yagl_gles_buffer_is_cpu_dirty(buffer_binding->buffer, 0, 0)) {
        yagl_gles_buffer_bind(buffer_binding->buffer, 0, 0,
                              buffer_binding->target);
        yagl_gles_buffer_transfer(buffer_binding->buffer, 0,
                                  buffer_binding->target, 0);
    }

    if (buffer_binding->entire) {
        yagl_host_glBindBufferBase(buffer_binding->target,
                                   buffer_binding->index,
                                   buffer_binding->buffer->default_part.global_name);
    } else {
        if (!yagl_gles3_buffer_binding_fixup(buffer_binding,
                                             &offset,
                                             &size)) {
            return;
        }

        yagl_host_glBindBufferRange(buffer_binding->target,
                                    buffer_binding->index,
                                    buffer_binding->buffer->default_part.global_name,
                                    offset, size);
    }
}

void yagl_gles3_buffer_binding_transfer_end(struct yagl_gles3_buffer_binding *buffer_binding)
{
    yagl_host_glBindBufferBase(buffer_binding->target,
                               buffer_binding->index,
                               0);
}

void yagl_gles3_buffer_binding_set_gpu_dirty(struct yagl_gles3_buffer_binding *buffer_binding)
{
    GLint offset;
    GLsizei size;

    assert(buffer_binding->buffer);
    if (!buffer_binding->buffer) {
        return;
    }

    if (buffer_binding->entire) {
        yagl_gles_buffer_set_gpu_dirty(buffer_binding->buffer,
                                       0,
                                       buffer_binding->buffer->size);
    } else if (yagl_gles3_buffer_binding_fixup(buffer_binding,
                                               &offset,
                                               &size)) {
        yagl_gles_buffer_set_gpu_dirty(buffer_binding->buffer,
                                       offset,
                                       size);
    }
}
