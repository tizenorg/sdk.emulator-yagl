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

#ifndef _YAGL_IOCTL_H_
#define _YAGL_IOCTL_H_

#include <linux/ioctl.h>

/*
 * Version number.
 */
#define YAGL_VERSION 23

/*
 * Device control codes magic.
 */
#define YAGL_IOC_MAGIC 'Y'

/*
 * Get version number.
 */
#define YAGL_IOC_GET_VERSION _IOR(YAGL_IOC_MAGIC, 0, unsigned int)

/*
 * Get user info.
 */
struct yagl_user_info
{
    unsigned int index;
    unsigned int render_type;
    unsigned int gl_version;
};

#define YAGL_IOC_GET_USER_INFO _IOR(YAGL_IOC_MAGIC, 1, struct yagl_user_info)

/*
 * Locks/unlocks memory. Exists solely
 * for offscreen backend's backing images.
 * @{
 */

struct yagl_mlock_arg
{
    unsigned long address;
    unsigned int size;
};

#define YAGL_IOC_MLOCK _IOW(YAGL_IOC_MAGIC, 2, struct yagl_mlock_arg)

#define YAGL_IOC_MUNLOCK _IOW(YAGL_IOC_MAGIC, 3, unsigned long)

/*
 * @}
 */

#endif
