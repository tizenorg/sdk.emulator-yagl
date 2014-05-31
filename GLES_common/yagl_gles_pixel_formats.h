#ifndef _YAGL_GLES_PIXEL_FORMATS_H_
#define _YAGL_GLES_PIXEL_FORMATS_H_

#include "yagl_pixel_format.h"

YAGL_PIXEL_FORMAT_DECL(gles_gl2, GL_ALPHA, GL_ALPHA, GL_UNSIGNED_BYTE);
YAGL_PIXEL_FORMAT_DECL(gles_gl2, GL_ALPHA, GL_ALPHA, GL_FLOAT);
YAGL_PIXEL_FORMAT_DECL(gles_gl2, GL_ALPHA, GL_ALPHA, GL_HALF_FLOAT);
YAGL_PIXEL_FORMAT_DECL(gles_gl2, GL_ALPHA, GL_ALPHA, GL_HALF_FLOAT_OES);
YAGL_PIXEL_FORMAT_DECL(gles_gl3, GL_ALPHA, GL_ALPHA, GL_UNSIGNED_BYTE);
YAGL_PIXEL_FORMAT_DECL(gles_gl3, GL_ALPHA, GL_ALPHA, GL_FLOAT);
YAGL_PIXEL_FORMAT_DECL(gles_gl3, GL_ALPHA, GL_ALPHA, GL_HALF_FLOAT);
YAGL_PIXEL_FORMAT_DECL(gles_gl3, GL_ALPHA, GL_ALPHA, GL_HALF_FLOAT_OES);
YAGL_PIXEL_FORMAT_DECL(gles, GL_RGB, GL_RGB, GL_UNSIGNED_BYTE);
YAGL_PIXEL_FORMAT_DECL(gles, GL_RGB, GL_RGB, GL_UNSIGNED_SHORT_5_6_5);
YAGL_PIXEL_FORMAT_DECL(gles, GL_RGB, GL_RGB, GL_FLOAT);
YAGL_PIXEL_FORMAT_DECL(gles, GL_RGB, GL_RGB, GL_HALF_FLOAT);
YAGL_PIXEL_FORMAT_DECL(gles, GL_RGB, GL_RGB, GL_HALF_FLOAT_OES);
YAGL_PIXEL_FORMAT_DECL(gles, GL_RGBA, GL_RGBA, GL_UNSIGNED_BYTE);
YAGL_PIXEL_FORMAT_DECL(gles, GL_RGBA, GL_RGBA, GL_UNSIGNED_SHORT_4_4_4_4);
YAGL_PIXEL_FORMAT_DECL(gles, GL_RGBA, GL_RGBA, GL_UNSIGNED_SHORT_5_5_5_1);
YAGL_PIXEL_FORMAT_DECL(gles, GL_RGBA, GL_RGBA, GL_FLOAT);
YAGL_PIXEL_FORMAT_DECL(gles, GL_RGBA, GL_RGBA, GL_HALF_FLOAT);
YAGL_PIXEL_FORMAT_DECL(gles, GL_RGBA, GL_RGBA, GL_HALF_FLOAT_OES);
YAGL_PIXEL_FORMAT_DECL(gles, GL_BGRA, GL_BGRA, GL_UNSIGNED_BYTE);
YAGL_PIXEL_FORMAT_DECL(gles, GL_BGRA, GL_BGRA, GL_FLOAT);
YAGL_PIXEL_FORMAT_DECL(gles, GL_BGRA, GL_BGRA, GL_HALF_FLOAT);
YAGL_PIXEL_FORMAT_DECL(gles, GL_BGRA, GL_BGRA, GL_HALF_FLOAT_OES);
YAGL_PIXEL_FORMAT_DECL(gles_gl2, GL_LUMINANCE, GL_LUMINANCE, GL_UNSIGNED_BYTE);
YAGL_PIXEL_FORMAT_DECL(gles_gl2, GL_LUMINANCE, GL_LUMINANCE, GL_FLOAT);
YAGL_PIXEL_FORMAT_DECL(gles_gl2, GL_LUMINANCE, GL_LUMINANCE, GL_HALF_FLOAT);
YAGL_PIXEL_FORMAT_DECL(gles_gl2, GL_LUMINANCE, GL_LUMINANCE, GL_HALF_FLOAT_OES);
YAGL_PIXEL_FORMAT_DECL(gles_gl2, GL_LUMINANCE_ALPHA, GL_LUMINANCE_ALPHA, GL_UNSIGNED_BYTE);
YAGL_PIXEL_FORMAT_DECL(gles_gl2, GL_LUMINANCE_ALPHA, GL_LUMINANCE_ALPHA, GL_FLOAT);
YAGL_PIXEL_FORMAT_DECL(gles_gl2, GL_LUMINANCE_ALPHA, GL_LUMINANCE_ALPHA, GL_HALF_FLOAT);
YAGL_PIXEL_FORMAT_DECL(gles_gl2, GL_LUMINANCE_ALPHA, GL_LUMINANCE_ALPHA, GL_HALF_FLOAT_OES);
YAGL_PIXEL_FORMAT_DECL(gles_gl3, GL_LUMINANCE, GL_LUMINANCE, GL_UNSIGNED_BYTE);
YAGL_PIXEL_FORMAT_DECL(gles_gl3, GL_LUMINANCE, GL_LUMINANCE, GL_FLOAT);
YAGL_PIXEL_FORMAT_DECL(gles_gl3, GL_LUMINANCE, GL_LUMINANCE, GL_HALF_FLOAT);
YAGL_PIXEL_FORMAT_DECL(gles_gl3, GL_LUMINANCE, GL_LUMINANCE, GL_HALF_FLOAT_OES);
YAGL_PIXEL_FORMAT_DECL(gles_gl3, GL_LUMINANCE_ALPHA, GL_LUMINANCE_ALPHA, GL_UNSIGNED_BYTE);
YAGL_PIXEL_FORMAT_DECL(gles_gl3, GL_LUMINANCE_ALPHA, GL_LUMINANCE_ALPHA, GL_FLOAT);
YAGL_PIXEL_FORMAT_DECL(gles_gl3, GL_LUMINANCE_ALPHA, GL_LUMINANCE_ALPHA, GL_HALF_FLOAT);
YAGL_PIXEL_FORMAT_DECL(gles_gl3, GL_LUMINANCE_ALPHA, GL_LUMINANCE_ALPHA, GL_HALF_FLOAT_OES);
YAGL_PIXEL_FORMAT_DECL(gles, GL_DEPTH_COMPONENT, GL_DEPTH_COMPONENT, GL_UNSIGNED_SHORT);
YAGL_PIXEL_FORMAT_DECL(gles, GL_DEPTH_COMPONENT, GL_DEPTH_COMPONENT, GL_UNSIGNED_INT);
YAGL_PIXEL_FORMAT_DECL(gles, GL_DEPTH_STENCIL, GL_DEPTH_STENCIL, GL_UNSIGNED_INT_24_8);

#endif