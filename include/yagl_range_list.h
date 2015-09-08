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

#ifndef _YAGL_RANGE_LIST_H_
#define _YAGL_RANGE_LIST_H_

#include "yagl_export.h"
#include "yagl_types.h"
#include "yagl_vector.h"

struct yagl_range_list
{
    struct yagl_vector ranges;
};

YAGL_API void yagl_range_list_init(struct yagl_range_list *range_list);

YAGL_API void yagl_range_list_cleanup(struct yagl_range_list *range_list);

YAGL_API void yagl_range_list_add(struct yagl_range_list *range_list,
                                  int start,
                                  int size);

YAGL_API int yagl_range_list_size(struct yagl_range_list *range_list);

YAGL_API void yagl_range_list_get(struct yagl_range_list *range_list,
                                  int i,
                                  int *start,
                                  int *size);

YAGL_API void yagl_range_list_clear(struct yagl_range_list *range_list);

#endif
