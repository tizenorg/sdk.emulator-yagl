#include "GLES3/gl3.h"
#include "yagl_gles3_context.h"
#include "yagl_gles3_buffer_binding.h"
#include "yagl_gles3_transform_feedback.h"
#include "yagl_gles3_query.h"
#include "yagl_gles3_pixel_formats.h"
#include "yagl_gles2_program.h"
#include "yagl_gles2_utils.h"
#include "yagl_gles_buffer.h"
#include "yagl_gles_texture_unit.h"
#include "yagl_gles_texture.h"
#include "yagl_gles_sampler.h"
#include "yagl_gles_utils.h"
#include "yagl_log.h"
#include "yagl_malloc.h"
#include "yagl_state.h"
#include "yagl_egl_fence.h"
#include "yagl_host_gles_calls.h"
#include <string.h>
#include <stdlib.h>
#include <assert.h>

/*
 * We can't include GLES2/gl2ext.h here
 */
#define GL_HALF_FLOAT_OES 0x8D61

/*
 * We can't include GL/glext.h here
 */
#define GL_FRAMEBUFFER_SRGB 0x8DB9

#define YAGL_SET_ERR(err) \
    yagl_gles_context_set_error(&ctx->base.base, err); \
    YAGL_LOG_ERROR("error = 0x%X", err)

static inline void yagl_gles3_context_pre_draw(struct yagl_gles3_context *ctx)
{
    struct yagl_gles3_buffer_binding *buffer_binding;

    yagl_list_for_each(struct yagl_gles3_buffer_binding,
                       buffer_binding,
                       &ctx->active_uniform_buffer_bindings, list) {
        yagl_gles3_buffer_binding_transfer_begin(buffer_binding);
    }
}

static inline void yagl_gles3_context_post_draw(struct yagl_gles3_context *ctx)
{
    struct yagl_gles3_buffer_binding *buffer_binding;

    yagl_list_for_each(struct yagl_gles3_buffer_binding,
                       buffer_binding,
                       &ctx->active_uniform_buffer_bindings, list) {
        yagl_gles3_buffer_binding_transfer_end(buffer_binding);
    }

    if (ctx->tfo->active && !ctx->tfo->paused) {
        yagl_gles3_transform_feedback_post_draw(ctx->tfo);
    }
}

static void yagl_gles3_context_prepare(struct yagl_client_context *ctx)
{
    struct yagl_gles3_context *gles3_ctx = (struct yagl_gles3_context*)ctx;
    GLuint i;
    const GLchar **extensions;
    int num_extensions;

    yagl_host_glEnable(GL_FRAMEBUFFER_SRGB);

    yagl_gles2_context_prepare(&gles3_ctx->base);

    yagl_host_glGetIntegerv(GL_MAX_UNIFORM_BUFFER_BINDINGS,
                            (GLint*)&gles3_ctx->num_uniform_buffer_bindings,
                            1,
                            NULL);

    gles3_ctx->uniform_buffer_bindings =
        yagl_malloc0(sizeof(gles3_ctx->uniform_buffer_bindings[0]) * gles3_ctx->num_uniform_buffer_bindings);

    for (i = 0; i < gles3_ctx->num_uniform_buffer_bindings; ++i) {
        yagl_gles3_buffer_binding_init(&gles3_ctx->uniform_buffer_bindings[i],
                                       GL_UNIFORM_BUFFER,
                                       i);
    }

    yagl_host_glGetIntegerv(GL_UNIFORM_BUFFER_OFFSET_ALIGNMENT,
                            &gles3_ctx->uniform_buffer_offset_alignment,
                            1,
                            NULL);

    yagl_host_glGetIntegerv(GL_MAX_TRANSFORM_FEEDBACK_SEPARATE_ATTRIBS,
                            &gles3_ctx->max_transform_feedback_separate_attribs,
                            1,
                            NULL);

    gles3_ctx->tf_zero =
        yagl_gles3_transform_feedback_create(1,
                                             gles3_ctx->max_transform_feedback_separate_attribs);

    yagl_gles3_transform_feedback_acquire(gles3_ctx->tf_zero);
    gles3_ctx->tfo = gles3_ctx->tf_zero;

    extensions = yagl_gles2_context_get_extensions(&gles3_ctx->base,
                                                   &num_extensions);

    yagl_gles_context_prepare_end(&gles3_ctx->base.base,
                                  extensions, num_extensions);
}

static void yagl_gles3_context_destroy(struct yagl_client_context *ctx)
{
    struct yagl_gles3_context *gles3_ctx = (struct yagl_gles3_context*)ctx;
    GLuint i;

    YAGL_LOG_FUNC_ENTER(yagl_gles3_context_destroy, "%p", ctx);

    yagl_gles_buffer_release(gles3_ctx->cwbo);
    yagl_gles_buffer_release(gles3_ctx->crbo);

    yagl_gles3_query_release(gles3_ctx->occlusion_query);
    yagl_gles3_query_release(gles3_ctx->tf_primitives_written_query);

    yagl_gles3_transform_feedback_release(gles3_ctx->tfo);
    yagl_gles3_transform_feedback_release(gles3_ctx->tf_zero);
    yagl_gles_buffer_release(gles3_ctx->tfbo);

    yagl_gles_buffer_release(gles3_ctx->ubo);

    for (i = 0; i < gles3_ctx->num_uniform_buffer_bindings; ++i) {
        yagl_gles3_buffer_binding_reset(&gles3_ctx->uniform_buffer_bindings[i]);
    }

    yagl_free(gles3_ctx->uniform_buffer_bindings);

    yagl_namespace_cleanup(&gles3_ctx->queries);
    yagl_namespace_cleanup(&gles3_ctx->transform_feedbacks);

    yagl_gles2_context_cleanup(&gles3_ctx->base);

    yagl_free(gles3_ctx);

    YAGL_LOG_FUNC_EXIT(NULL);
}

static const GLchar
    *yagl_gles3_context_get_string(struct yagl_gles_context *ctx,
                                   GLenum name)
{
    const char *str = NULL;

    switch (name) {
    case GL_VERSION:
        str = "OpenGL ES 3.0";
        break;
    case GL_RENDERER:
        str = "YaGL GLESv3";
        break;
    case GL_SHADING_LANGUAGE_VERSION:
        str = "OpenGL ES GLSL ES 3.0";
        break;
    default:
        str = "";
    }

    return str;
}

static int yagl_gles3_context_enable(struct yagl_gles_context *ctx,
                                     GLenum cap,
                                     GLboolean enable)
{
    struct yagl_gles3_context *gles3_ctx = (struct yagl_gles3_context*)ctx;

    switch (cap) {
    case GL_PRIMITIVE_RESTART_FIXED_INDEX:
        gles3_ctx->primitive_restart_fixed_index = enable;
        break;
    case GL_RASTERIZER_DISCARD:
        gles3_ctx->rasterizer_discard = enable;
        break;
    default:
        return 0;
    }

    if (enable) {
        yagl_host_glEnable(cap);
    } else {
        yagl_host_glDisable(cap);
    }

    return 1;
}

static int yagl_gles3_context_is_enabled(struct yagl_gles_context *ctx,
                                         GLenum cap,
                                         GLboolean *enabled)
{
    struct yagl_gles3_context *gles3_ctx = (struct yagl_gles3_context*)ctx;

    switch (cap) {
    case GL_PRIMITIVE_RESTART_FIXED_INDEX:
        *enabled = gles3_ctx->primitive_restart_fixed_index;
        break;
    case GL_RASTERIZER_DISCARD:
        *enabled = gles3_ctx->rasterizer_discard;
        break;
    default:
        return 0;
    }

    return 1;
}

static int yagl_gles3_context_get_integerv(struct yagl_gles_context *ctx,
                                           GLenum pname,
                                           GLint *params,
                                           uint32_t *num_params)
{
    int processed = 1;
    struct yagl_gles3_context *gles3_ctx = (struct yagl_gles3_context*)ctx;
    struct yagl_gles_sampler *sampler;
    struct yagl_gles_texture_target_state *tts;

    switch (pname) {
    case GL_MAX_UNIFORM_BUFFER_BINDINGS:
        *params = gles3_ctx->num_uniform_buffer_bindings;
        *num_params = 1;
        break;
    case GL_NUM_PROGRAM_BINARY_FORMATS:
        *params = 0;
        *num_params = 1;
        break;
    case GL_UNIFORM_BUFFER_BINDING:
        *params = gles3_ctx->ubo ? gles3_ctx->ubo->base.local_name : 0;
        *num_params = 1;
        break;
    case GL_UNIFORM_BUFFER_OFFSET_ALIGNMENT:
        *params = gles3_ctx->uniform_buffer_offset_alignment;
        *num_params = 1;
        break;
    case GL_MAX_TRANSFORM_FEEDBACK_SEPARATE_ATTRIBS:
        *params = gles3_ctx->max_transform_feedback_separate_attribs;
        *num_params = 1;
        break;
    case GL_TRANSFORM_FEEDBACK_BINDING:
        *params = gles3_ctx->tfo->base.local_name;
        *num_params = 1;
        break;
    case GL_TRANSFORM_FEEDBACK_ACTIVE:
        *params = gles3_ctx->tfo->active;
        *num_params = 1;
        break;
    case GL_TRANSFORM_FEEDBACK_PAUSED:
        *params = gles3_ctx->tfo->paused;
        *num_params = 1;
        break;
    case GL_TRANSFORM_FEEDBACK_BUFFER_BINDING:
        *params = gles3_ctx->tfbo ? gles3_ctx->tfbo->base.local_name : 0;
        *num_params = 1;
        break;
    case GL_SAMPLER_BINDING:
        sampler = yagl_gles_context_get_active_texture_unit(ctx)->sampler;
        *params = sampler ? sampler->base.local_name : 0;
        *num_params = 1;
        break;
    case GL_TEXTURE_BINDING_2D_ARRAY:
        tts = yagl_gles_context_get_active_texture_target_state(ctx,
            yagl_gles_texture_target_2d_array);
        *params = tts->texture->base.local_name;
        *num_params = 1;
        break;
    case GL_COPY_READ_BUFFER_BINDING:
        *params = gles3_ctx->crbo ? gles3_ctx->crbo->base.local_name : 0;
        *num_params = 1;
        break;
    case GL_COPY_WRITE_BUFFER_BINDING:
        *params = gles3_ctx->cwbo ? gles3_ctx->cwbo->base.local_name : 0;
        *num_params = 1;
        break;
    case GL_MAX_SERVER_WAIT_TIMEOUT:
        *params = 0x7FFFFFFE;
        *num_params = 1;
        break;
    case GL_NUM_EXTENSIONS:
        *params = ctx->num_extensions;
        *num_params = 1;
        break;
    case GL_MAJOR_VERSION:
        *params = 3;
        *num_params = 1;
        break;
    case GL_MINOR_VERSION:
        *params = 0;
        *num_params = 1;
        break;
    case GL_MAX_VARYING_COMPONENTS:
        *num_params = 1;
        if (yagl_get_host_gl_version() >= yagl_gl_3_2) {
            /*
             * GL_MAX_VARYING_COMPONENTS is deprecated in
             * OpenGL 3.2, thus, we use a constant.
             */
            *params = 64;
        } else if (gles3_ctx->base.have_max_varying_floats) {
            *params = gles3_ctx->base.max_varying_floats;
        } else {
            yagl_host_glGetIntegerv(GL_MAX_VARYING_COMPONENTS, params, *num_params, NULL);
            gles3_ctx->base.max_varying_floats = *params;
            gles3_ctx->base.have_max_varying_floats = 1;
        }
        break;
    case GL_FRAGMENT_SHADER_DERIVATIVE_HINT:
        *params = gles3_ctx->fragment_shader_derivative_hint;
        *num_params = 1;
        break;
    case GL_MAX_ARRAY_TEXTURE_LAYERS:
        if (gles3_ctx->have_max_array_texture_layers) {
            *params = gles3_ctx->max_array_texture_layers;
            *num_params = 1;
        } else {
            processed = 0;
        }
        break;
    case GL_MAX_COMBINED_FRAGMENT_UNIFORM_COMPONENTS:
        if (gles3_ctx->have_max_combined_fragment_uniform_components) {
            *params = gles3_ctx->max_combined_fragment_uniform_components;
            *num_params = 1;
        } else {
            processed = 0;
        }
        break;
    case GL_MAX_COMBINED_UNIFORM_BLOCKS:
        if (gles3_ctx->have_max_combined_uniform_blocks) {
            *params = gles3_ctx->max_combined_uniform_blocks;
            *num_params = 1;
        } else {
            processed = 0;
        }
        break;
    case GL_MAX_COMBINED_VERTEX_UNIFORM_COMPONENTS:
        if (gles3_ctx->have_max_combined_vertex_uniform_components) {
            *params = gles3_ctx->max_combined_vertex_uniform_components;
            *num_params = 1;
        } else {
            processed = 0;
        }
        break;
    case GL_MAX_ELEMENT_INDEX:
        if (gles3_ctx->have_max_element_index) {
            *params = gles3_ctx->max_element_index;
            *num_params = 1;
        } else {
            processed = 0;
        }
        break;
    case GL_MAX_ELEMENTS_INDICES:
        if (gles3_ctx->have_max_elements_indices) {
            *params = gles3_ctx->max_elements_indices;
            *num_params = 1;
        } else {
            processed = 0;
        }
        break;
    case GL_MAX_ELEMENTS_VERTICES:
        if (gles3_ctx->have_max_elements_vertices) {
            *params = gles3_ctx->max_elements_vertices;
            *num_params = 1;
        } else {
            processed = 0;
        }
        break;
    case GL_MAX_FRAGMENT_INPUT_COMPONENTS:
        if (gles3_ctx->have_max_fragment_input_components) {
            *params = gles3_ctx->max_fragment_input_components;
            *num_params = 1;
        } else {
            processed = 0;
        }
        break;
    case GL_MAX_FRAGMENT_UNIFORM_BLOCKS:
        if (gles3_ctx->have_max_fragment_uniform_blocks) {
            *params = gles3_ctx->max_fragment_uniform_blocks;
            *num_params = 1;
        } else {
            processed = 0;
        }
        break;
    case GL_MAX_FRAGMENT_UNIFORM_COMPONENTS:
        if (gles3_ctx->have_max_fragment_uniform_components) {
            *params = gles3_ctx->max_fragment_uniform_components;
            *num_params = 1;
        } else {
            processed = 0;
        }
        break;
    case GL_MAX_PROGRAM_TEXEL_OFFSET:
        if (gles3_ctx->have_max_program_texel_offset) {
            *params = gles3_ctx->max_program_texel_offset;
            *num_params = 1;
        } else {
            processed = 0;
        }
        break;
    case GL_MAX_SAMPLES:
        if (gles3_ctx->have_max_samples) {
            *params = gles3_ctx->max_samples;
            *num_params = 1;
        } else {
            processed = 0;
        }
        break;
    case GL_MAX_TEXTURE_LOD_BIAS:
        if (gles3_ctx->have_max_texture_lod_bias) {
            *params = gles3_ctx->max_texture_lod_bias;
            *num_params = 1;
        } else {
            processed = 0;
        }
        break;
    case GL_MAX_TRANSFORM_FEEDBACK_INTERLEAVED_COMPONENTS:
        if (gles3_ctx->have_max_transform_feedback_interleaved_components) {
            *params = gles3_ctx->max_transform_feedback_interleaved_components;
            *num_params = 1;
        } else {
            processed = 0;
        }
        break;
    case GL_MAX_TRANSFORM_FEEDBACK_SEPARATE_COMPONENTS:
        if (gles3_ctx->have_max_transform_feedback_separate_components) {
            *params = gles3_ctx->max_transform_feedback_separate_components;
            *num_params = 1;
        } else {
            processed = 0;
        }
        break;
    case GL_MAX_UNIFORM_BLOCK_SIZE:
        if (gles3_ctx->have_max_uniform_block_size) {
            *params = gles3_ctx->max_uniform_block_size;
            *num_params = 1;
        } else {
            processed = 0;
        }
        break;
    case GL_MAX_VERTEX_OUTPUT_COMPONENTS:
        if (gles3_ctx->have_max_vertex_output_components) {
            *params = gles3_ctx->max_vertex_output_components;
            *num_params = 1;
        } else {
            processed = 0;
        }
        break;
    case GL_MAX_VERTEX_UNIFORM_BLOCKS:
        if (gles3_ctx->have_max_vertex_uniform_blocks) {
            *params = gles3_ctx->max_vertex_uniform_blocks;
            *num_params = 1;
        } else {
            processed = 0;
        }
        break;
    case GL_MAX_VERTEX_UNIFORM_COMPONENTS:
        if (gles3_ctx->have_max_vertex_uniform_components) {
            *params = gles3_ctx->max_vertex_uniform_components;
            *num_params = 1;
        } else {
            processed = 0;
        }
        break;
    case GL_MIN_PROGRAM_TEXEL_OFFSET:
        if (gles3_ctx->have_min_program_texel_offset) {
            *params = gles3_ctx->min_program_texel_offset;
            *num_params = 1;
        } else {
            processed = 0;
        }
        break;
    case GL_PRIMITIVE_RESTART_FIXED_INDEX:
        *params = gles3_ctx->primitive_restart_fixed_index;
        *num_params = 1;
        break;
    default:
        processed = 0;
        break;
    }

    if (processed) {
        return 1;
    }

    switch (pname) {
    case GL_MAX_ARRAY_TEXTURE_LAYERS:
        *num_params = 1;
        yagl_host_glGetIntegerv(pname, params, *num_params, NULL);
        gles3_ctx->max_array_texture_layers = *params;
        gles3_ctx->have_max_array_texture_layers = 1;
        break;
    case GL_MAX_COMBINED_FRAGMENT_UNIFORM_COMPONENTS:
        *num_params = 1;
        yagl_host_glGetIntegerv(pname, params, *num_params, NULL);
        gles3_ctx->max_combined_fragment_uniform_components = *params;
        gles3_ctx->have_max_combined_fragment_uniform_components = 1;
        break;
    case GL_MAX_COMBINED_UNIFORM_BLOCKS:
        *num_params = 1;
        yagl_host_glGetIntegerv(pname, params, *num_params, NULL);
        gles3_ctx->max_combined_uniform_blocks = *params;
        gles3_ctx->have_max_combined_uniform_blocks = 1;
        break;
    case GL_MAX_COMBINED_VERTEX_UNIFORM_COMPONENTS:
        *num_params = 1;
        yagl_host_glGetIntegerv(pname, params, *num_params, NULL);
        gles3_ctx->max_combined_vertex_uniform_components = *params;
        gles3_ctx->have_max_combined_vertex_uniform_components = 1;
        break;
    case GL_MAX_ELEMENT_INDEX:
        *num_params = 1;
        yagl_host_glGetIntegerv(pname, params, *num_params, NULL);
        gles3_ctx->max_element_index = *params;
        gles3_ctx->have_max_element_index = 1;
        break;
    case GL_MAX_ELEMENTS_INDICES:
        *num_params = 1;
        yagl_host_glGetIntegerv(pname, params, *num_params, NULL);
        gles3_ctx->max_elements_indices = *params;
        gles3_ctx->have_max_elements_indices = 1;
        break;
    case GL_MAX_ELEMENTS_VERTICES:
        *num_params = 1;
        yagl_host_glGetIntegerv(pname, params, *num_params, NULL);
        gles3_ctx->max_elements_vertices = *params;
        gles3_ctx->have_max_elements_vertices = 1;
        break;
    case GL_MAX_FRAGMENT_INPUT_COMPONENTS:
        *num_params = 1;
        yagl_host_glGetIntegerv(pname, params, *num_params, NULL);
        gles3_ctx->max_fragment_input_components = *params;
        gles3_ctx->have_max_fragment_input_components = 1;
        break;
    case GL_MAX_FRAGMENT_UNIFORM_BLOCKS:
        *num_params = 1;
        yagl_host_glGetIntegerv(pname, params, *num_params, NULL);
        gles3_ctx->max_fragment_uniform_blocks = *params;
        gles3_ctx->have_max_fragment_uniform_blocks = 1;
        break;
    case GL_MAX_FRAGMENT_UNIFORM_COMPONENTS:
        *num_params = 1;
        yagl_host_glGetIntegerv(pname, params, *num_params, NULL);
        gles3_ctx->max_fragment_uniform_components = *params;
        gles3_ctx->have_max_fragment_uniform_components = 1;
        break;
    case GL_MAX_PROGRAM_TEXEL_OFFSET:
        *num_params = 1;
        yagl_host_glGetIntegerv(pname, params, *num_params, NULL);
        gles3_ctx->max_program_texel_offset = *params;
        gles3_ctx->have_max_program_texel_offset = 1;
        break;
    case GL_MAX_SAMPLES:
        *num_params = 1;
        yagl_host_glGetIntegerv(pname, params, *num_params, NULL);
        gles3_ctx->max_samples = *params;
        gles3_ctx->have_max_samples = 1;
        break;
    case GL_MAX_TEXTURE_LOD_BIAS:
        *num_params = 1;
        yagl_host_glGetIntegerv(pname, params, *num_params, NULL);
        gles3_ctx->max_texture_lod_bias = *params;
        gles3_ctx->have_max_texture_lod_bias = 1;
        break;
    case GL_MAX_TRANSFORM_FEEDBACK_INTERLEAVED_COMPONENTS:
        *num_params = 1;
        yagl_host_glGetIntegerv(pname, params, *num_params, NULL);
        gles3_ctx->max_transform_feedback_interleaved_components = *params;
        gles3_ctx->have_max_transform_feedback_interleaved_components = 1;
        break;
    case GL_MAX_TRANSFORM_FEEDBACK_SEPARATE_COMPONENTS:
        *num_params = 1;
        yagl_host_glGetIntegerv(pname, params, *num_params, NULL);
        gles3_ctx->max_transform_feedback_separate_components = *params;
        gles3_ctx->have_max_transform_feedback_separate_components = 1;
        break;
    case GL_MAX_UNIFORM_BLOCK_SIZE:
        *num_params = 1;
        yagl_host_glGetIntegerv(pname, params, *num_params, NULL);
        gles3_ctx->max_uniform_block_size = *params;
        gles3_ctx->have_max_uniform_block_size = 1;
        break;
    case GL_MAX_VERTEX_OUTPUT_COMPONENTS:
        *num_params = 1;
        yagl_host_glGetIntegerv(pname, params, *num_params, NULL);
        gles3_ctx->max_vertex_output_components = *params;
        gles3_ctx->have_max_vertex_output_components = 1;
        break;
    case GL_MAX_VERTEX_UNIFORM_BLOCKS:
        *num_params = 1;
        yagl_host_glGetIntegerv(pname, params, *num_params, NULL);
        gles3_ctx->max_vertex_uniform_blocks = *params;
        gles3_ctx->have_max_vertex_uniform_blocks = 1;
        break;
    case GL_MAX_VERTEX_UNIFORM_COMPONENTS:
        *num_params = 1;
        yagl_host_glGetIntegerv(pname, params, *num_params, NULL);
        gles3_ctx->max_vertex_uniform_components = *params;
        gles3_ctx->have_max_vertex_uniform_components = 1;
        break;
    case GL_MIN_PROGRAM_TEXEL_OFFSET:
        *num_params = 1;
        yagl_host_glGetIntegerv(pname, params, *num_params, NULL);
        gles3_ctx->min_program_texel_offset = *params;
        gles3_ctx->have_min_program_texel_offset = 1;
        break;
    case GL_PROGRAM_BINARY_FORMATS:
        *num_params = 0;
        break;
    default:
        return yagl_gles2_context_get_integerv(ctx, pname, params, num_params);
    }

    return 1;
}

static void yagl_gles3_context_draw_arrays(struct yagl_gles_context *gles_ctx,
                                           GLenum mode,
                                           GLint first,
                                           GLsizei count,
                                           GLsizei primcount)
{
    struct yagl_gles3_context *ctx = (struct yagl_gles3_context*)gles_ctx;

    YAGL_LOG_FUNC_SET(yagl_gles3_context_draw_arrays);

    if (ctx->tfo->active &&
        !ctx->tfo->paused &&
        (mode != ctx->tf_primitive_mode)) {
        YAGL_SET_ERR(GL_INVALID_OPERATION);
        return;
    }

    yagl_gles3_context_pre_draw(ctx);

    yagl_gles2_context_draw_arrays(gles_ctx, mode, first, count, primcount);

    yagl_gles3_context_post_draw(ctx);
}

static void yagl_gles3_context_draw_elements(struct yagl_gles_context *gles_ctx,
                                             GLenum mode,
                                             GLsizei count,
                                             GLenum type,
                                             const GLvoid *indices,
                                             int32_t indices_count,
                                             GLsizei primcount,
                                             uint32_t max_idx)
{
    struct yagl_gles3_context *ctx = (struct yagl_gles3_context*)gles_ctx;

    YAGL_LOG_FUNC_SET(yagl_gles3_context_draw_elements);

    if (ctx->tfo->active && !ctx->tfo->paused) {
        YAGL_SET_ERR(GL_INVALID_OPERATION);
        return;
    }

    yagl_gles3_context_pre_draw(ctx);

    yagl_gles2_context_draw_elements(gles_ctx,
                                     mode,
                                     count,
                                     type,
                                     indices,
                                     indices_count,
                                     primcount,
                                     max_idx);

    yagl_gles3_context_post_draw(ctx);
}

static int yagl_gles3_context_bind_buffer(struct yagl_gles_context *ctx,
                                          GLenum target,
                                          struct yagl_gles_buffer *buffer)
{
    struct yagl_gles3_context *gles3_ctx = (struct yagl_gles3_context*)ctx;

    switch (target) {
    case GL_UNIFORM_BUFFER:
        yagl_gles_buffer_acquire(buffer);
        yagl_gles_buffer_release(gles3_ctx->ubo);
        gles3_ctx->ubo = buffer;
        break;
    case GL_TRANSFORM_FEEDBACK_BUFFER:
        yagl_gles_buffer_acquire(buffer);
        yagl_gles_buffer_release(gles3_ctx->tfbo);
        gles3_ctx->tfbo = buffer;
        break;
    case GL_COPY_READ_BUFFER:
        yagl_gles_buffer_acquire(buffer);
        yagl_gles_buffer_release(gles3_ctx->crbo);
        gles3_ctx->crbo = buffer;
        break;
    case GL_COPY_WRITE_BUFFER:
        yagl_gles_buffer_acquire(buffer);
        yagl_gles_buffer_release(gles3_ctx->cwbo);
        gles3_ctx->cwbo = buffer;
        break;
    default:
        return 0;
    }

    return 1;
}

static void yagl_gles3_context_unbind_buffer(struct yagl_gles_context *ctx,
                                             yagl_object_name buffer_local_name)
{
    struct yagl_gles3_context *gles3_ctx = (struct yagl_gles3_context*)ctx;
    GLuint i;

    if (gles3_ctx->ubo && (gles3_ctx->ubo->base.local_name == buffer_local_name)) {
        yagl_gles_buffer_release(gles3_ctx->ubo);
        gles3_ctx->ubo = NULL;
    }

    if (gles3_ctx->tfbo && (gles3_ctx->tfbo->base.local_name == buffer_local_name)) {
        yagl_gles_buffer_release(gles3_ctx->tfbo);
        gles3_ctx->tfbo = NULL;
    }

    for (i = 0; i < gles3_ctx->num_uniform_buffer_bindings; ++i) {
        struct yagl_gles_buffer *buffer = gles3_ctx->uniform_buffer_bindings[i].buffer;

        if (buffer && (buffer->base.local_name == buffer_local_name)) {
            yagl_gles3_buffer_binding_reset(&gles3_ctx->uniform_buffer_bindings[i]);
        }
    }

    yagl_gles3_transform_feedback_unbind_buffer(gles3_ctx->tfo,
                                                buffer_local_name);

    if (gles3_ctx->crbo && (gles3_ctx->crbo->base.local_name == buffer_local_name)) {
        yagl_gles_buffer_release(gles3_ctx->crbo);
        gles3_ctx->crbo = NULL;
    }

    if (gles3_ctx->cwbo && (gles3_ctx->cwbo->base.local_name == buffer_local_name)) {
        yagl_gles_buffer_release(gles3_ctx->cwbo);
        gles3_ctx->cwbo = NULL;
    }
}

static int yagl_gles3_context_acquire_binded_buffer(struct yagl_gles_context *ctx,
                                                    GLenum target,
                                                    struct yagl_gles_buffer **buffer)
{
    struct yagl_gles3_context *gles3_ctx = (struct yagl_gles3_context*)ctx;

    switch (target) {
    case GL_UNIFORM_BUFFER:
        yagl_gles_buffer_acquire(gles3_ctx->ubo);
        *buffer = gles3_ctx->ubo;
        break;
    case GL_TRANSFORM_FEEDBACK_BUFFER:
        yagl_gles_buffer_acquire(gles3_ctx->tfbo);
        *buffer = gles3_ctx->tfbo;
        break;
    case GL_COPY_READ_BUFFER:
        yagl_gles_buffer_acquire(gles3_ctx->crbo);
        *buffer = gles3_ctx->crbo;
        break;
    case GL_COPY_WRITE_BUFFER:
        yagl_gles_buffer_acquire(gles3_ctx->cwbo);
        *buffer = gles3_ctx->cwbo;
        break;
    default:
        return 0;
    }

    return 1;
}

static int yagl_gles3_context_validate_texture_target(struct yagl_gles_context *ctx,
                                                      GLenum target,
                                                      yagl_gles_texture_target *texture_target)
{
    if (yagl_gles2_context_validate_texture_target(ctx,
                                                   target,
                                                   texture_target)) {
        return 1;
    }

    switch (target) {
    case GL_TEXTURE_2D_ARRAY:
        *texture_target = yagl_gles_texture_target_2d_array;
        break;
    default:
        return 0;
    }

    return 1;
}

static struct yagl_pixel_format
    *yagl_gles3_context_validate_teximage_format(struct yagl_gles_context *ctx,
                                                 GLenum internalformat,
                                                 GLenum format,
                                                 GLenum type)
{
    struct yagl_pixel_format *pf =
        yagl_gles2_context_validate_teximage_format(ctx,
                                                    internalformat,
                                                    format,
                                                    type);

    if (pf) {
        return pf;
    }

    switch (format) {
    case GL_RED:
        switch (internalformat) {
        case GL_R8:
            switch (type) {
            YAGL_PIXEL_FORMAT_CASE(gles3, GL_R8, GL_RED, GL_UNSIGNED_BYTE);
            }
            break;
        case GL_R8_SNORM:
            switch (type) {
            YAGL_PIXEL_FORMAT_CASE(gles3, GL_R8_SNORM, GL_RED, GL_BYTE);
            }
            break;
        case GL_R16F:
            switch (type) {
            YAGL_PIXEL_FORMAT_CASE(gles3, GL_R16F, GL_RED, GL_FLOAT);
            YAGL_PIXEL_FORMAT_CASE(gles3, GL_R16F, GL_RED, GL_HALF_FLOAT);
            YAGL_PIXEL_FORMAT_CASE(gles3, GL_R16F, GL_RED, GL_HALF_FLOAT_OES);
            }
            break;
        case GL_R32F:
            switch (type) {
            YAGL_PIXEL_FORMAT_CASE(gles3, GL_R32F, GL_RED, GL_FLOAT);
            }
            break;
        }
        break;
    case GL_RED_INTEGER:
        switch (internalformat) {
        case GL_R8UI:
            switch (type) {
            YAGL_PIXEL_FORMAT_CASE(gles3, GL_R8UI, GL_RED_INTEGER, GL_UNSIGNED_BYTE);
            }
            break;
        case GL_R8I:
            switch (type) {
            YAGL_PIXEL_FORMAT_CASE(gles3, GL_R8I, GL_RED_INTEGER, GL_BYTE);
            }
            break;
        case GL_R16UI:
            switch (type) {
            YAGL_PIXEL_FORMAT_CASE(gles3, GL_R16UI, GL_RED_INTEGER, GL_UNSIGNED_SHORT);
            }
            break;
        case GL_R16I:
            switch (type) {
            YAGL_PIXEL_FORMAT_CASE(gles3, GL_R16I, GL_RED_INTEGER, GL_SHORT);
            }
            break;
        case GL_R32UI:
            switch (type) {
            YAGL_PIXEL_FORMAT_CASE(gles3, GL_R32UI, GL_RED_INTEGER, GL_UNSIGNED_INT);
            }
            break;
        case GL_R32I:
            switch (type) {
            YAGL_PIXEL_FORMAT_CASE(gles3, GL_R32I, GL_RED_INTEGER, GL_INT);
            }
            break;
        }
        break;
    case GL_RG:
        switch (internalformat) {
        case GL_RG8:
            switch (type) {
            YAGL_PIXEL_FORMAT_CASE(gles3, GL_RG8, GL_RG, GL_UNSIGNED_BYTE);
            }
            break;
        case GL_RG8_SNORM:
            switch (type) {
            YAGL_PIXEL_FORMAT_CASE(gles3, GL_RG8_SNORM, GL_RG, GL_BYTE);
            }
            break;
        case GL_RG16F:
            switch (type) {
            YAGL_PIXEL_FORMAT_CASE(gles3, GL_RG16F, GL_RG, GL_FLOAT);
            YAGL_PIXEL_FORMAT_CASE(gles3, GL_RG16F, GL_RG, GL_HALF_FLOAT);
            YAGL_PIXEL_FORMAT_CASE(gles3, GL_RG16F, GL_RG, GL_HALF_FLOAT_OES);
            }
            break;
        case GL_RG32F:
            switch (type) {
            YAGL_PIXEL_FORMAT_CASE(gles3, GL_RG32F, GL_RG, GL_FLOAT);
            }
            break;
        }
        break;
    case GL_RG_INTEGER:
        switch (internalformat) {
        case GL_RG8UI:
            switch (type) {
            YAGL_PIXEL_FORMAT_CASE(gles3, GL_RG8UI, GL_RG_INTEGER, GL_UNSIGNED_BYTE);
            }
            break;
        case GL_RG8I:
            switch (type) {
            YAGL_PIXEL_FORMAT_CASE(gles3, GL_RG8I, GL_RG_INTEGER, GL_BYTE);
            }
            break;
        case GL_RG16UI:
            switch (type) {
            YAGL_PIXEL_FORMAT_CASE(gles3, GL_RG16UI, GL_RG_INTEGER, GL_UNSIGNED_SHORT);
            }
            break;
        case GL_RG16I:
            switch (type) {
            YAGL_PIXEL_FORMAT_CASE(gles3, GL_RG16I, GL_RG_INTEGER, GL_SHORT);
            }
            break;
        case GL_RG32UI:
            switch (type) {
            YAGL_PIXEL_FORMAT_CASE(gles3, GL_RG32UI, GL_RG_INTEGER, GL_UNSIGNED_INT);
            }
            break;
        case GL_RG32I:
            switch (type) {
            YAGL_PIXEL_FORMAT_CASE(gles3, GL_RG32I, GL_RG_INTEGER, GL_INT);
            }
            break;
        }
        break;
    case GL_RGB:
        switch (internalformat) {
        case GL_RGB8:
            switch (type) {
            YAGL_PIXEL_FORMAT_CASE(gles3, GL_RGB8, GL_RGB, GL_UNSIGNED_BYTE);
            }
            break;
        case GL_SRGB8:
            switch (type) {
            YAGL_PIXEL_FORMAT_CASE(gles3, GL_SRGB8, GL_RGB, GL_UNSIGNED_BYTE);
            }
            break;
        case GL_RGB565:
            switch (type) {
            YAGL_PIXEL_FORMAT_CASE(gles3, GL_RGB565, GL_RGB, GL_UNSIGNED_BYTE);
            YAGL_PIXEL_FORMAT_CASE(gles3, GL_RGB565, GL_RGB, GL_UNSIGNED_SHORT_5_6_5);
            }
            break;
        case GL_RGB8_SNORM:
            switch (type) {
            YAGL_PIXEL_FORMAT_CASE(gles3, GL_RGB8_SNORM, GL_RGB, GL_BYTE);
            }
            break;
        case GL_R11F_G11F_B10F:
            switch (type) {
            YAGL_PIXEL_FORMAT_CASE(gles3, GL_R11F_G11F_B10F, GL_RGB, GL_FLOAT);
            YAGL_PIXEL_FORMAT_CASE(gles3, GL_R11F_G11F_B10F, GL_RGB, GL_HALF_FLOAT);
            YAGL_PIXEL_FORMAT_CASE(gles3, GL_R11F_G11F_B10F, GL_RGB, GL_HALF_FLOAT_OES);
            YAGL_PIXEL_FORMAT_CASE(gles3, GL_R11F_G11F_B10F, GL_RGB, GL_UNSIGNED_INT_10F_11F_11F_REV);
            }
            break;
        case GL_RGB9_E5:
            switch (type) {
            YAGL_PIXEL_FORMAT_CASE(gles3, GL_RGB9_E5, GL_RGB, GL_FLOAT);
            YAGL_PIXEL_FORMAT_CASE(gles3, GL_RGB9_E5, GL_RGB, GL_HALF_FLOAT);
            YAGL_PIXEL_FORMAT_CASE(gles3, GL_RGB9_E5, GL_RGB, GL_HALF_FLOAT_OES);
            YAGL_PIXEL_FORMAT_CASE(gles3, GL_RGB9_E5, GL_RGB, GL_UNSIGNED_INT_5_9_9_9_REV);
            }
            break;
        case GL_RGB16F:
            switch (type) {
            YAGL_PIXEL_FORMAT_CASE(gles3, GL_RGB16F, GL_RGB, GL_FLOAT);
            YAGL_PIXEL_FORMAT_CASE(gles3, GL_RGB16F, GL_RGB, GL_HALF_FLOAT);
            YAGL_PIXEL_FORMAT_CASE(gles3, GL_RGB16F, GL_RGB, GL_HALF_FLOAT_OES);
            }
            break;
        case GL_RGB32F:
            switch (type) {
            YAGL_PIXEL_FORMAT_CASE(gles3, GL_RGB32F, GL_RGB, GL_FLOAT);
            }
            break;
        }
        break;
    case GL_RGB_INTEGER:
        switch (internalformat) {
        case GL_RGB8UI:
            switch (type) {
            YAGL_PIXEL_FORMAT_CASE(gles3, GL_RGB8UI, GL_RGB_INTEGER, GL_UNSIGNED_BYTE);
            }
            break;
        case GL_RGB8I:
            switch (type) {
            YAGL_PIXEL_FORMAT_CASE(gles3, GL_RGB8I, GL_RGB_INTEGER, GL_BYTE);
            }
            break;
        case GL_RGB16UI:
            switch (type) {
            YAGL_PIXEL_FORMAT_CASE(gles3, GL_RGB16UI, GL_RGB_INTEGER, GL_UNSIGNED_SHORT);
            }
            break;
        case GL_RGB16I:
            switch (type) {
            YAGL_PIXEL_FORMAT_CASE(gles3, GL_RGB16I, GL_RGB_INTEGER, GL_SHORT);
            }
            break;
        case GL_RGB32UI:
            switch (type) {
            YAGL_PIXEL_FORMAT_CASE(gles3, GL_RGB32UI, GL_RGB_INTEGER, GL_UNSIGNED_INT);
            }
            break;
        case GL_RGB32I:
            switch (type) {
            YAGL_PIXEL_FORMAT_CASE(gles3, GL_RGB32I, GL_RGB_INTEGER, GL_INT);
            }
            break;
        }
        break;
    case GL_RGBA:
        switch (internalformat) {
        case GL_RGBA8:
            switch (type) {
            YAGL_PIXEL_FORMAT_CASE(gles3, GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE);
            }
            break;
        case GL_SRGB8_ALPHA8:
            switch (type) {
            YAGL_PIXEL_FORMAT_CASE(gles3, GL_SRGB8_ALPHA8, GL_RGBA, GL_UNSIGNED_BYTE);
            }
            break;
        case GL_RGBA8_SNORM:
            switch (type) {
            YAGL_PIXEL_FORMAT_CASE(gles3, GL_RGBA8_SNORM, GL_RGBA, GL_BYTE);
            }
            break;
        case GL_RGB5_A1:
            switch (type) {
            YAGL_PIXEL_FORMAT_CASE(gles3, GL_RGB5_A1, GL_RGBA, GL_UNSIGNED_BYTE);
            YAGL_PIXEL_FORMAT_CASE(gles3, GL_RGB5_A1, GL_RGBA, GL_UNSIGNED_SHORT_5_5_5_1);
            YAGL_PIXEL_FORMAT_CASE(gles3, GL_RGB5_A1, GL_RGBA, GL_UNSIGNED_INT_2_10_10_10_REV);
            }
            break;
        case GL_RGBA4:
            switch (type) {
            YAGL_PIXEL_FORMAT_CASE(gles3, GL_RGBA4, GL_RGBA, GL_UNSIGNED_BYTE);
            YAGL_PIXEL_FORMAT_CASE(gles3, GL_RGBA4, GL_RGBA, GL_UNSIGNED_SHORT_4_4_4_4);
            }
            break;
        case GL_RGB10_A2:
            switch (type) {
            YAGL_PIXEL_FORMAT_CASE(gles3, GL_RGB10_A2, GL_RGBA, GL_UNSIGNED_INT_2_10_10_10_REV);
            }
            break;
        case GL_RGBA16F:
            switch (type) {
            YAGL_PIXEL_FORMAT_CASE(gles3, GL_RGBA16F, GL_RGBA, GL_FLOAT);
            YAGL_PIXEL_FORMAT_CASE(gles3, GL_RGBA16F, GL_RGBA, GL_HALF_FLOAT);
            YAGL_PIXEL_FORMAT_CASE(gles3, GL_RGBA16F, GL_RGBA, GL_HALF_FLOAT_OES);
            }
            break;
        case GL_RGBA32F:
            switch (type) {
            YAGL_PIXEL_FORMAT_CASE(gles3, GL_RGBA32F, GL_RGBA, GL_FLOAT);
            }
            break;
        }
        break;
    case GL_RGBA_INTEGER:
        switch (internalformat) {
        case GL_RGBA8UI:
            switch (type) {
            YAGL_PIXEL_FORMAT_CASE(gles3, GL_RGBA8UI, GL_RGBA_INTEGER, GL_UNSIGNED_BYTE);
            }
            break;
        case GL_RGBA8I:
            switch (type) {
            YAGL_PIXEL_FORMAT_CASE(gles3, GL_RGBA8I, GL_RGBA_INTEGER, GL_BYTE);
            }
            break;
        case GL_RGB10_A2UI:
            switch (type) {
            YAGL_PIXEL_FORMAT_CASE(gles3, GL_RGB10_A2UI, GL_RGBA_INTEGER, GL_UNSIGNED_INT_2_10_10_10_REV);
            }
            break;
        case GL_RGBA16UI:
            switch (type) {
            YAGL_PIXEL_FORMAT_CASE(gles3, GL_RGBA16UI, GL_RGBA_INTEGER, GL_UNSIGNED_SHORT);
            }
            break;
        case GL_RGBA16I:
            switch (type) {
            YAGL_PIXEL_FORMAT_CASE(gles3, GL_RGBA16I, GL_RGBA_INTEGER, GL_SHORT);
            }
            break;
        case GL_RGBA32UI:
            switch (type) {
            YAGL_PIXEL_FORMAT_CASE(gles3, GL_RGBA32UI, GL_RGBA_INTEGER, GL_UNSIGNED_INT);
            }
            break;
        case GL_RGBA32I:
            switch (type) {
            YAGL_PIXEL_FORMAT_CASE(gles3, GL_RGBA32I, GL_RGBA_INTEGER, GL_INT);
            }
            break;
        }
        break;
    case GL_DEPTH_COMPONENT:
        switch (internalformat) {
        case GL_DEPTH_COMPONENT16:
            switch (type) {
            YAGL_PIXEL_FORMAT_CASE(gles3, GL_DEPTH_COMPONENT16, GL_DEPTH_COMPONENT, GL_UNSIGNED_SHORT);
            YAGL_PIXEL_FORMAT_CASE(gles3, GL_DEPTH_COMPONENT16, GL_DEPTH_COMPONENT, GL_UNSIGNED_INT);
            }
            break;
        case GL_DEPTH_COMPONENT24:
            switch (type) {
            YAGL_PIXEL_FORMAT_CASE(gles3, GL_DEPTH_COMPONENT24, GL_DEPTH_COMPONENT, GL_UNSIGNED_INT);
            }
            break;
        case GL_DEPTH_COMPONENT32F:
            switch (type) {
            YAGL_PIXEL_FORMAT_CASE(gles3, GL_DEPTH_COMPONENT32F, GL_DEPTH_COMPONENT, GL_FLOAT);
            }
            break;
        }
        break;
    case GL_DEPTH_STENCIL:
        switch (internalformat) {
        case GL_DEPTH24_STENCIL8:
            switch (type) {
            YAGL_PIXEL_FORMAT_CASE(gles3, GL_DEPTH24_STENCIL8, GL_DEPTH_STENCIL, GL_UNSIGNED_INT_24_8);
            }
            break;
        case GL_DEPTH32F_STENCIL8:
            switch (type) {
            YAGL_PIXEL_FORMAT_CASE(gles3, GL_DEPTH32F_STENCIL8, GL_DEPTH_STENCIL, GL_FLOAT_32_UNSIGNED_INT_24_8_REV);
            }
            break;
        }
        break;
    }

    return NULL;
}

static struct yagl_pixel_format
    *yagl_gles3_context_validate_getteximage_format(struct yagl_gles_context *ctx,
                                                    GLenum readbuffer_internalformat,
                                                    GLenum format,
                                                    GLenum type)
{
    const struct yagl_gles_format_info *readbuffer_format_info;
    struct yagl_pixel_format *pf =
        yagl_gles2_context_validate_getteximage_format(ctx,
                                                       readbuffer_internalformat,
                                                       format,
                                                       type);

    if (pf) {
        return pf;
    }

    readbuffer_format_info = yagl_gles_internalformat_info(readbuffer_internalformat);

    switch (format) {
    case GL_RGBA:
        if (readbuffer_internalformat == GL_RGB10_A2) {
            switch (type) {
            YAGL_PIXEL_FORMAT_CASE(gles3, GL_RGB10_A2, GL_RGBA, GL_UNSIGNED_INT_2_10_10_10_REV);
            }
        }
        break;
    case GL_RGBA_INTEGER:
        if ((readbuffer_format_info->flags & (yagl_gles_format_unsigned_integer)) != 0) {
            switch (type) {
            YAGL_PIXEL_FORMAT_CASE(gles3, GL_RGBA32UI, GL_RGBA_INTEGER, GL_UNSIGNED_INT);
            }
        }
        if ((readbuffer_format_info->flags & (yagl_gles_format_signed_integer)) != 0) {
            switch (type) {
            YAGL_PIXEL_FORMAT_CASE(gles3, GL_RGBA32I, GL_RGBA_INTEGER, GL_INT);
            }
        }
        break;
    }

    return NULL;
}

static int yagl_gles3_context_validate_copyteximage_format(struct yagl_gles_context *ctx,
                                                           GLenum readbuffer_internalformat,
                                                           GLenum *internalformat)
{
    const struct yagl_gles_format_info *readbuffer_format_info;

    if (yagl_gles2_context_validate_copyteximage_format(ctx,
                                                        readbuffer_internalformat,
                                                        internalformat)) {
        return 1;
    }

    readbuffer_format_info = yagl_gles_internalformat_info(readbuffer_internalformat);

    if ((readbuffer_format_info->flags & yagl_gles_format_unsigned_integer) != 0) {
        switch (*internalformat) {
        case GL_R8UI:
        case GL_R16UI:
        case GL_R32UI:
        case GL_RG8UI:
        case GL_RG16UI:
        case GL_RG32UI:
        case GL_RGBA8UI:
        case GL_RGB10_A2UI:
        case GL_RGBA16UI:
        case GL_RGBA32UI:
        case GL_RGB8UI:
        case GL_RGB16UI:
        case GL_RGB32UI:
            return 1;
        }
    } else if ((readbuffer_format_info->flags & yagl_gles_format_signed_integer) != 0) {
        switch (*internalformat) {
        case GL_R8I:
        case GL_R16I:
        case GL_R32I:
        case GL_RG8I:
        case GL_RG16I:
        case GL_RG32I:
        case GL_RGBA8I:
        case GL_RGBA16I:
        case GL_RGBA32I:
        case GL_RGB8I:
        case GL_RGB16I:
        case GL_RGB32I:
            return 1;
        }
    } else {
        switch (*internalformat) {
        case GL_R16F:
        case GL_R32F:
        case GL_RG16F:
        case GL_RG32F:
        case GL_R11F_G11F_B10F:
        case GL_RGB16F:
        case GL_RGB32F:
        case GL_RGBA16F:
        case GL_RGBA32F:
        case GL_R8:
        case GL_RG8:
        case GL_RGB565:
        case GL_RGB8:
        case GL_RGBA4:
        case GL_RGB5_A1:
        case GL_RGBA8:
        case GL_RGB10_A2:
        case GL_SRGB8_ALPHA8:
        case GL_R8_SNORM:
        case GL_RG8_SNORM:
        case GL_RGB8_SNORM:
        case GL_RGBA8_SNORM:
        case GL_SRGB8:
            return 1;
        }
    }

    return 0;
}

static int yagl_gles3_context_validate_texstorage_format(struct yagl_gles_context *ctx,
                                                         GLenum *internalformat,
                                                         GLenum *base_internalformat,
                                                         GLenum *any_format,
                                                         GLenum *any_type)
{
    if (yagl_gles2_context_validate_texstorage_format(ctx,
                                                      internalformat,
                                                      base_internalformat,
                                                      any_format,
                                                      any_type)) {
        return 1;
    }

    switch (*internalformat) {
    case GL_R8:
        *any_format = GL_RED;
        *any_type = GL_UNSIGNED_BYTE;
        break;
    case GL_R8_SNORM:
        *any_format = GL_RED;
        *any_type = GL_BYTE;
        break;
    case GL_R16F:
    case GL_R32F:
        *any_format = GL_RED;
        *any_type = GL_FLOAT;
        break;
    case GL_R8UI:
        *any_format = GL_RED_INTEGER;
        *any_type = GL_UNSIGNED_BYTE;
        break;
    case GL_R8I:
        *any_format = GL_RED_INTEGER;
        *any_type = GL_BYTE;
        break;
    case GL_R16UI:
        *any_format = GL_RED_INTEGER;
        *any_type = GL_UNSIGNED_SHORT;
        break;
    case GL_R16I:
        *any_format = GL_RED_INTEGER;
        *any_type = GL_SHORT;
        break;
    case GL_R32UI:
        *any_format = GL_RED_INTEGER;
        *any_type = GL_UNSIGNED_INT;
        break;
    case GL_R32I:
        *any_format = GL_RED_INTEGER;
        *any_type = GL_INT;
        break;
    case GL_RG8:
        *any_format = GL_RG;
        *any_type = GL_UNSIGNED_BYTE;
        break;
    case GL_RG8_SNORM:
        *any_format = GL_RG;
        *any_type = GL_BYTE;
        break;
    case GL_RG16F:
    case GL_RG32F:
        *any_format = GL_RG;
        *any_type = GL_FLOAT;
        break;
    case GL_RG8UI:
        *any_format = GL_RG_INTEGER;
        *any_type = GL_UNSIGNED_BYTE;
        break;
    case GL_RG8I:
        *any_format = GL_RG_INTEGER;
        *any_type = GL_BYTE;
        break;
    case GL_RG16UI:
        *any_format = GL_RG_INTEGER;
        *any_type = GL_UNSIGNED_SHORT;
        break;
    case GL_RG16I:
        *any_format = GL_RG_INTEGER;
        *any_type = GL_SHORT;
        break;
    case GL_RG32UI:
        *any_format = GL_RG_INTEGER;
        *any_type = GL_UNSIGNED_INT;
        break;
    case GL_RG32I:
        *any_format = GL_RG_INTEGER;
        *any_type = GL_INT;
        break;
    case GL_RGB8:
    case GL_SRGB8:
    case GL_RGB565:
        *any_format = GL_RGB;
        *any_type = GL_UNSIGNED_BYTE;
        break;
    case GL_RGB8_SNORM:
        *any_format = GL_RGB;
        *any_type = GL_BYTE;
        break;
    case GL_R11F_G11F_B10F:
    case GL_RGB9_E5:
    case GL_RGB16F:
    case GL_RGB32F:
        *any_format = GL_RGB;
        *any_type = GL_FLOAT;
        break;
    case GL_RGB8UI:
        *any_format = GL_RGB_INTEGER;
        *any_type = GL_UNSIGNED_BYTE;
        break;
    case GL_RGB8I:
        *any_format = GL_RGB_INTEGER;
        *any_type = GL_BYTE;
        break;
    case GL_RGB16UI:
        *any_format = GL_RGB_INTEGER;
        *any_type = GL_UNSIGNED_SHORT;
        break;
    case GL_RGB16I:
        *any_format = GL_RGB_INTEGER;
        *any_type = GL_SHORT;
        break;
    case GL_RGB32UI:
        *any_format = GL_RGB_INTEGER;
        *any_type = GL_UNSIGNED_INT;
        break;
    case GL_RGB32I:
        *any_format = GL_RGB_INTEGER;
        *any_type = GL_INT;
        break;
    case GL_RGBA8:
    case GL_SRGB8_ALPHA8:
    case GL_RGB5_A1:
    case GL_RGBA4:
        *any_format = GL_RGBA;
        *any_type = GL_UNSIGNED_BYTE;
        break;
    case GL_RGBA8_SNORM:
        *any_format = GL_RGBA;
        *any_type = GL_BYTE;
        break;
    case GL_RGB10_A2:
        *any_format = GL_RGBA;
        *any_type = GL_UNSIGNED_INT_2_10_10_10_REV;
        break;
    case GL_RGBA16F:
    case GL_RGBA32F:
        *any_format = GL_RGBA;
        *any_type = GL_FLOAT;
        break;
    case GL_RGBA8UI:
        *any_format = GL_RGBA_INTEGER;
        *any_type = GL_UNSIGNED_BYTE;
        break;
    case GL_RGBA8I:
        *any_format = GL_RGBA_INTEGER;
        *any_type = GL_BYTE;
        break;
    case GL_RGB10_A2UI:
        *any_format = GL_RGBA_INTEGER;
        *any_type = GL_UNSIGNED_INT_2_10_10_10_REV;
        break;
    case GL_RGBA16UI:
        *any_format = GL_RGBA_INTEGER;
        *any_type = GL_UNSIGNED_SHORT;
        break;
    case GL_RGBA16I:
        *any_format = GL_RGBA_INTEGER;
        *any_type = GL_SHORT;
        break;
    case GL_RGBA32I:
        *any_format = GL_RGBA_INTEGER;
        *any_type = GL_INT;
        break;
    case GL_RGBA32UI:
        *any_format = GL_RGBA_INTEGER;
        *any_type = GL_UNSIGNED_INT;
        break;
    case GL_DEPTH_COMPONENT16:
    case GL_DEPTH_COMPONENT24:
        *any_format = GL_DEPTH_COMPONENT;
        *any_type = GL_UNSIGNED_INT;
        break;
    case GL_DEPTH_COMPONENT32F:
        *any_format = GL_DEPTH_COMPONENT;
        *any_type = GL_FLOAT;
        break;
    case GL_DEPTH24_STENCIL8:
        *any_format = GL_DEPTH_STENCIL;
        *any_type = GL_UNSIGNED_INT_24_8;
        break;
    case GL_DEPTH32F_STENCIL8:
        *any_format = GL_DEPTH_STENCIL;
        *any_type = GL_FLOAT_32_UNSIGNED_INT_24_8_REV;
        break;
    default:
        return 0;
    }

    *base_internalformat = *internalformat;

    return 1;
}

static int yagl_gles3_context_validate_renderbuffer_format(struct yagl_gles_context *ctx,
                                                           GLenum *internalformat)
{
    if (yagl_gles2_context_validate_renderbuffer_format(ctx,
                                                        internalformat)) {
        return 1;
    }

    switch (*internalformat) {
    case GL_R8:
    case GL_R8UI:
    case GL_R8I:
    case GL_R16UI:
    case GL_R16I:
    case GL_R32UI:
    case GL_R32I:
    case GL_RG8:
    case GL_RG8UI:
    case GL_RG8I:
    case GL_RG16UI:
    case GL_RG16I:
    case GL_RG32UI:
    case GL_RG32I:
    case GL_RGB8:
    case GL_RGBA8:
    case GL_SRGB8_ALPHA8:
    case GL_RGB10_A2:
    case GL_RGBA8UI:
    case GL_RGBA8I:
    case GL_RGB10_A2UI:
    case GL_RGBA16UI:
    case GL_RGBA16I:
    case GL_RGBA32I:
    case GL_RGBA32UI:
    case GL_DEPTH_COMPONENT32F:
    case GL_DEPTH32F_STENCIL8:
        break;
    default:
        return 0;
    }

    return 1;
}

static void yagl_gles3_context_hint(struct yagl_gles_context *gles_ctx,
                                    GLenum target,
                                    GLenum mode)
{
    struct yagl_gles3_context *ctx = (struct yagl_gles3_context*)gles_ctx;

    switch (target) {
    case GL_FRAGMENT_SHADER_DERIVATIVE_HINT:
        ctx->fragment_shader_derivative_hint = mode;
        break;
    default:
        yagl_gles2_context_hint(gles_ctx, target, mode);
        break;
    }
}

static int yagl_gles3_context_get_programiv(struct yagl_gles2_context *ctx,
                                            struct yagl_gles2_program *program,
                                            GLenum pname,
                                            GLint *params)
{
    if (yagl_gles2_context_get_programiv(ctx, program, pname, params)) {
        return 1;
    }

    switch (pname) {
    case GL_ACTIVE_UNIFORM_BLOCKS:
        *params = program->num_active_uniform_blocks;
        break;
    case GL_ACTIVE_UNIFORM_BLOCK_MAX_NAME_LENGTH:
        *params = program->max_active_uniform_block_bufsize;
        break;
    case GL_PROGRAM_BINARY_RETRIEVABLE_HINT:
        *params = GL_FALSE;
        break;
    case GL_TRANSFORM_FEEDBACK_BUFFER_MODE:
        *params = program->transform_feedback_info.buffer_mode;
        break;
    case GL_TRANSFORM_FEEDBACK_VARYINGS:
        *params = program->transform_feedback_info.num_varyings;
        break;
    case GL_TRANSFORM_FEEDBACK_VARYING_MAX_LENGTH:
        *params = program->transform_feedback_info.max_varying_bufsize;
        break;
    default:
        return 0;
    }

    return 1;
}

static int yagl_gles3_context_pre_use_program(struct yagl_gles2_context *gles_ctx,
                                              struct yagl_gles2_program *program)
{
    struct yagl_gles3_context *ctx = (struct yagl_gles3_context*)gles_ctx;

    YAGL_LOG_FUNC_SET(glUseProgram);

    if (ctx->tfo->active && !ctx->tfo->paused) {
        YAGL_SET_ERR(GL_INVALID_OPERATION);
        return 0;
    } else {
        return 1;
    }
}

static int yagl_gles3_context_pre_link_program(struct yagl_gles2_context *gles_ctx,
                                               struct yagl_gles2_program *program)
{
    struct yagl_gles3_context *ctx = (struct yagl_gles3_context*)gles_ctx;

    YAGL_LOG_FUNC_SET(glLinkProgram);

    if (ctx->tfo->active) {
        YAGL_SET_ERR(GL_INVALID_OPERATION);
        return 0;
    } else {
        return 1;
    }
}

struct yagl_client_context *yagl_gles3_context_create(struct yagl_sharegroup *sg)
{
    struct yagl_gles3_context *gles3_ctx;

    YAGL_LOG_FUNC_ENTER(yagl_gles3_context_create, NULL);

    gles3_ctx = yagl_malloc0(sizeof(*gles3_ctx));

    yagl_gles2_context_init(&gles3_ctx->base, yagl_client_api_gles3, sg);

    yagl_namespace_init(&gles3_ctx->transform_feedbacks);
    yagl_namespace_init(&gles3_ctx->queries);

    yagl_list_init(&gles3_ctx->active_uniform_buffer_bindings);

    gles3_ctx->fragment_shader_derivative_hint = GL_DONT_CARE;

    gles3_ctx->base.base.base.prepare = &yagl_gles3_context_prepare;
    gles3_ctx->base.base.base.destroy = &yagl_gles3_context_destroy;
    gles3_ctx->base.base.create_arrays = &yagl_gles2_context_create_arrays;
    gles3_ctx->base.base.get_string = &yagl_gles3_context_get_string;
    gles3_ctx->base.base.compressed_tex_image_2d = &yagl_gles2_context_compressed_tex_image_2d;
    gles3_ctx->base.base.compressed_tex_sub_image_2d = &yagl_gles2_context_compressed_tex_sub_image_2d;
    gles3_ctx->base.base.enable = &yagl_gles3_context_enable;
    gles3_ctx->base.base.is_enabled = &yagl_gles3_context_is_enabled;
    gles3_ctx->base.base.get_integerv = &yagl_gles3_context_get_integerv;
    gles3_ctx->base.base.get_floatv = &yagl_gles2_context_get_floatv;
    gles3_ctx->base.base.draw_arrays = &yagl_gles3_context_draw_arrays;
    gles3_ctx->base.base.draw_elements = &yagl_gles3_context_draw_elements;
    gles3_ctx->base.base.bind_buffer = &yagl_gles3_context_bind_buffer;
    gles3_ctx->base.base.unbind_buffer = &yagl_gles3_context_unbind_buffer;
    gles3_ctx->base.base.acquire_binded_buffer = &yagl_gles3_context_acquire_binded_buffer;
    gles3_ctx->base.base.validate_texture_target = &yagl_gles3_context_validate_texture_target;
    gles3_ctx->base.base.validate_teximage_format = &yagl_gles3_context_validate_teximage_format;
    gles3_ctx->base.base.validate_getteximage_format = &yagl_gles3_context_validate_getteximage_format;
    gles3_ctx->base.base.validate_copyteximage_format = &yagl_gles3_context_validate_copyteximage_format;
    gles3_ctx->base.base.validate_texstorage_format = &yagl_gles3_context_validate_texstorage_format;
    gles3_ctx->base.base.validate_renderbuffer_format = &yagl_gles3_context_validate_renderbuffer_format;
    gles3_ctx->base.base.hint = &yagl_gles3_context_hint;
    gles3_ctx->base.get_programiv = &yagl_gles3_context_get_programiv;
    gles3_ctx->base.pre_use_program = &yagl_gles3_context_pre_use_program;
    gles3_ctx->base.pre_link_program = &yagl_gles3_context_pre_link_program;

    YAGL_LOG_FUNC_EXIT("%p", gles3_ctx);

    return &gles3_ctx->base.base.base;
}

void yagl_gles3_context_bind_buffer_base(struct yagl_gles3_context *ctx,
                                         GLenum target,
                                         GLuint index,
                                         struct yagl_gles_buffer *buffer)
{
    YAGL_LOG_FUNC_SET(glBindBufferBase);

    switch (target) {
    case GL_UNIFORM_BUFFER:
        if (index >= ctx->num_uniform_buffer_bindings) {
            YAGL_SET_ERR(GL_INVALID_VALUE);
            return;
        }

        /*
         * Set indexed binding point.
         */

        yagl_gles3_buffer_binding_set_base(&ctx->uniform_buffer_bindings[index],
                                           buffer);

        if (buffer) {
            yagl_list_add_tail(&ctx->active_uniform_buffer_bindings,
                               &ctx->uniform_buffer_bindings[index].list);
        }

        /*
         * Set generic binding point.
         */

        yagl_gles_buffer_acquire(buffer);
        yagl_gles_buffer_release(ctx->ubo);
        ctx->ubo = buffer;

        break;
    case GL_TRANSFORM_FEEDBACK_BUFFER:
        /*
         * Set indexed binding point.
         */

        if (ctx->tfo->active) {
            YAGL_SET_ERR(GL_INVALID_OPERATION);
            return;
        }

        if (!yagl_gles3_transform_feedback_bind_buffer_base(ctx->tfo,
                                                            index,
                                                            buffer)) {
            YAGL_SET_ERR(GL_INVALID_VALUE);
            return;
        }

        /*
         * Set generic binding point.
         */

        yagl_gles_buffer_acquire(buffer);
        yagl_gles_buffer_release(ctx->tfbo);
        ctx->tfbo = buffer;

        break;
    default:
        YAGL_SET_ERR(GL_INVALID_ENUM);
        return;
    }

    if (buffer) {
        yagl_gles_buffer_set_bound(buffer);
    }
}

void yagl_gles3_context_bind_buffer_range(struct yagl_gles3_context *ctx,
                                          GLenum target,
                                          GLuint index,
                                          GLintptr offset,
                                          GLsizeiptr size,
                                          struct yagl_gles_buffer *buffer)
{
    YAGL_LOG_FUNC_SET(glBindBufferRange);

    switch (target) {
    case GL_UNIFORM_BUFFER:
        if (index >= ctx->num_uniform_buffer_bindings) {
            YAGL_SET_ERR(GL_INVALID_VALUE);
            return;
        }

        if (buffer) {
            if (size <= 0) {
                YAGL_SET_ERR(GL_INVALID_VALUE);
                return;
            }

            if ((offset % ctx->uniform_buffer_offset_alignment) != 0) {
                YAGL_SET_ERR(GL_INVALID_VALUE);
                return;
            }
        }

        /*
         * Set indexed binding point.
         */

        yagl_gles3_buffer_binding_set_range(&ctx->uniform_buffer_bindings[index],
                                            buffer, offset, size);

        if (buffer) {
            yagl_list_add_tail(&ctx->active_uniform_buffer_bindings,
                               &ctx->uniform_buffer_bindings[index].list);
        }

        /*
         * Set generic binding point.
         */

        yagl_gles_buffer_acquire(buffer);
        yagl_gles_buffer_release(ctx->ubo);
        ctx->ubo = buffer;

        break;
    case GL_TRANSFORM_FEEDBACK_BUFFER:
        /*
         * Set indexed binding point.
         */

        if (ctx->tfo->active) {
            YAGL_SET_ERR(GL_INVALID_OPERATION);
            return;
        }

        if (buffer) {
            if (size <= 0) {
                YAGL_SET_ERR(GL_INVALID_VALUE);
                return;
            }

            if (((offset % 4) != 0) || ((size % 4) != 0)) {
                YAGL_SET_ERR(GL_INVALID_VALUE);
                return;
            }
        }

        if (!yagl_gles3_transform_feedback_bind_buffer_range(ctx->tfo,
                                                             index,
                                                             offset,
                                                             size,
                                                             buffer)) {
            YAGL_SET_ERR(GL_INVALID_VALUE);
            return;
        }

        /*
         * Set generic binding point.
         */

        yagl_gles_buffer_acquire(buffer);
        yagl_gles_buffer_release(ctx->tfbo);
        ctx->tfbo = buffer;

        break;
    default:
        YAGL_SET_ERR(GL_INVALID_ENUM);
        return;
    }

    if (buffer) {
        yagl_gles_buffer_set_bound(buffer);
    }
}

void yagl_gles3_context_bind_transform_feedback(struct yagl_gles3_context *ctx,
                                                GLenum target,
                                                struct yagl_gles3_transform_feedback *tfo)
{
    YAGL_LOG_FUNC_SET(glBindTransformFeedback);

    if (!tfo) {
        tfo = ctx->tf_zero;
    }

    switch (target) {
    case GL_TRANSFORM_FEEDBACK:
        if (ctx->tfo->active && !ctx->tfo->paused) {
            YAGL_SET_ERR(GL_INVALID_OPERATION);
            return;
        }

        yagl_gles3_transform_feedback_acquire(tfo);
        yagl_gles3_transform_feedback_release(ctx->tfo);
        ctx->tfo = tfo;
        break;
    default:
        YAGL_SET_ERR(GL_INVALID_ENUM);
        return;
    }

    yagl_gles3_transform_feedback_bind(tfo, target);
}

void yagl_gles3_context_begin_query(struct yagl_gles3_context *ctx,
                                    GLenum target,
                                    struct yagl_gles3_query *query)
{
    YAGL_LOG_FUNC_SET(glBeginQuery);

    if (query->active) {
        YAGL_SET_ERR(GL_INVALID_OPERATION);
        return;
    }

    switch (target) {
    case GL_TRANSFORM_FEEDBACK_PRIMITIVES_WRITTEN:
        if (ctx->tf_primitives_written_query) {
            YAGL_SET_ERR(GL_INVALID_OPERATION);
            return;
        }

        yagl_gles3_query_acquire(query);
        yagl_gles3_query_release(ctx->tf_primitives_written_query);
        ctx->tf_primitives_written_query = query;
        break;
    case GL_ANY_SAMPLES_PASSED:
    case GL_ANY_SAMPLES_PASSED_CONSERVATIVE:
        if (ctx->occlusion_query) {
            YAGL_SET_ERR(GL_INVALID_OPERATION);
            return;
        }

        yagl_gles3_query_acquire(query);
        yagl_gles3_query_release(ctx->occlusion_query);
        ctx->occlusion_query = query;
        break;
    default:
        YAGL_SET_ERR(GL_INVALID_ENUM);
        return;
    }

    yagl_gles3_query_begin(query, target);
}

void yagl_gles3_context_end_query(struct yagl_gles3_context *ctx,
                                  GLenum target)
{
    struct yagl_gles3_query *query = NULL;

    YAGL_LOG_FUNC_SET(glEndQuery);

    switch (target) {
    case GL_TRANSFORM_FEEDBACK_PRIMITIVES_WRITTEN:
        if (!ctx->tf_primitives_written_query) {
            YAGL_SET_ERR(GL_INVALID_OPERATION);
            return;
        }

        query = ctx->tf_primitives_written_query;
        ctx->tf_primitives_written_query = NULL;
        break;
    case GL_ANY_SAMPLES_PASSED:
    case GL_ANY_SAMPLES_PASSED_CONSERVATIVE:
        if (!ctx->occlusion_query) {
            YAGL_SET_ERR(GL_INVALID_OPERATION);
            return;
        }

        query = ctx->occlusion_query;
        ctx->occlusion_query = NULL;
        break;
    default:
        YAGL_SET_ERR(GL_INVALID_ENUM);
        return;
    }

    yagl_gles3_query_end(query, target);
    yagl_gles3_query_release(query);
}

int yagl_gles3_context_acquire_active_query(struct yagl_gles3_context *ctx,
                                            GLenum target,
                                            struct yagl_gles3_query **query)
{
    switch (target) {
    case GL_TRANSFORM_FEEDBACK_PRIMITIVES_WRITTEN:
        yagl_gles3_query_acquire(ctx->tf_primitives_written_query);
        *query = ctx->tf_primitives_written_query;
        break;
    case GL_ANY_SAMPLES_PASSED:
    case GL_ANY_SAMPLES_PASSED_CONSERVATIVE:
        yagl_gles3_query_acquire(ctx->occlusion_query);
        *query = ctx->occlusion_query;
        break;
    default:
        return 0;
    }

    return 1;
}

int yagl_gles3_context_bind_sampler(struct yagl_gles3_context *ctx,
                                    GLuint unit,
                                    struct yagl_gles_sampler *sampler)
{
    if (unit > ctx->base.base.num_texture_units) {
        return 0;
    }

    yagl_gles_sampler_acquire(sampler);
    yagl_gles_sampler_release(ctx->base.base.texture_units[unit].sampler);
    ctx->base.base.texture_units[unit].sampler = sampler;

    yagl_gles_sampler_bind(sampler, unit);

    return 1;
}

void yagl_gles3_context_unbind_sampler(struct yagl_gles3_context *ctx,
                                       yagl_object_name sampler_local_name)
{
    int i;

    for (i = 0; i < ctx->base.base.num_texture_units; ++i) {
        if (ctx->base.base.texture_units[i].sampler &&
            (ctx->base.base.texture_units[i].sampler->base.local_name == sampler_local_name)) {
            yagl_gles_sampler_release(ctx->base.base.texture_units[i].sampler);
            ctx->base.base.texture_units[i].sampler = NULL;
        }
    }
}

int yagl_gles3_context_get_integerv_indexed(struct yagl_gles3_context *ctx,
                                            GLenum target,
                                            GLuint index,
                                            GLint *params,
                                            uint32_t *num_params)
{
    YAGL_LOG_FUNC_SET(glGetIntegeri_v);

    switch (target) {
    case GL_TRANSFORM_FEEDBACK_BUFFER_SIZE:
        if (index >= ctx->tfo->num_buffer_bindings) {
            YAGL_SET_ERR(GL_INVALID_VALUE);
            return 0;
        }

        if (ctx->tfo->buffer_bindings[index].entire) {
            *params = 0;
        } else {
            *params = ctx->tfo->buffer_bindings[index].size;
        }

        *num_params = 1;
        break;
    case GL_TRANSFORM_FEEDBACK_BUFFER_START:
        if (index >= ctx->tfo->num_buffer_bindings) {
            YAGL_SET_ERR(GL_INVALID_VALUE);
            return 0;
        }

        if (ctx->tfo->buffer_bindings[index].entire) {
            *params = 0;
        } else {
            *params = ctx->tfo->buffer_bindings[index].offset;
        }

        *num_params = 1;
        break;
    case GL_UNIFORM_BUFFER_SIZE:
        if (index >= ctx->num_uniform_buffer_bindings) {
            YAGL_SET_ERR(GL_INVALID_VALUE);
            return 0;
        }

        if (ctx->uniform_buffer_bindings[index].entire) {
            *params = 0;
        } else {
            *params = ctx->uniform_buffer_bindings[index].size;
        }

        *num_params = 1;
        break;
    case GL_UNIFORM_BUFFER_START:
        if (index >= ctx->num_uniform_buffer_bindings) {
            YAGL_SET_ERR(GL_INVALID_VALUE);
            return 0;
        }

        if (ctx->uniform_buffer_bindings[index].entire) {
            *params = 0;
        } else {
            *params = ctx->uniform_buffer_bindings[index].offset;
        }

        *num_params = 1;
        break;
    case GL_TRANSFORM_FEEDBACK_BUFFER_BINDING:
        if (index >= ctx->tfo->num_buffer_bindings) {
            YAGL_SET_ERR(GL_INVALID_VALUE);
            return 0;
        }
        *params = ctx->tfo->buffer_bindings[index].buffer ? ctx->tfo->buffer_bindings[index].buffer->base.local_name : 0;
        *num_params = 1;
        break;
    case GL_UNIFORM_BUFFER_BINDING:
        if (index >= ctx->num_uniform_buffer_bindings) {
            YAGL_SET_ERR(GL_INVALID_VALUE);
            return 0;
        }
        *params = ctx->uniform_buffer_bindings[index].buffer ? ctx->uniform_buffer_bindings[index].buffer->base.local_name : 0;
        *num_params = 1;
        break;
    default:
        YAGL_SET_ERR(GL_INVALID_ENUM);
        return 0;
    }

    return 1;
}

void yagl_gles3_context_draw_range_elements(struct yagl_gles3_context *ctx,
                                            GLenum mode,
                                            GLuint start,
                                            GLuint end,
                                            GLsizei count,
                                            GLenum type,
                                            const GLvoid *indices,
                                            int32_t indices_count)
{
    YAGL_LOG_FUNC_SET(yagl_gles3_context_draw_range_elements);

    if (ctx->tfo->active && !ctx->tfo->paused) {
        YAGL_SET_ERR(GL_INVALID_OPERATION);
        return;
    }

    yagl_gles3_context_pre_draw(ctx);
    yagl_gles2_context_pre_draw(&ctx->base, mode, end + 1);

    yagl_host_glDrawRangeElements(mode, start, end, count, type, indices, indices_count);

    yagl_gles2_context_post_draw(&ctx->base, mode, end + 1);
    yagl_gles3_context_post_draw(ctx);
}
