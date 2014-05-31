#include "GLES3/gl3.h"
#include "yagl_gles2_validate.h"

int yagl_gles2_is_shader_type_valid(GLenum type)
{
    switch (type) {
    case GL_VERTEX_SHADER:
    case GL_FRAGMENT_SHADER:
        return 1;
    default:
        return 0;
    }
}

int yagl_gles2_is_texture_target_layered(GLenum target)
{
    switch (target) {
    case GL_TEXTURE_2D_ARRAY:
    case GL_TEXTURE_3D:
        return 1;
    default:
        return 0;
    }
}
