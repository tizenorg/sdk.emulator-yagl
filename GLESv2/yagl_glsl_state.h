#ifndef _YAGL_GLSL_STATE_H_
#define _YAGL_GLSL_STATE_H_

#include "yagl_types.h"
#include "yagl_list.h"
#include "yagl_vector.h"

struct yagl_glsl_state;

#include "yagl_glsl_parser.h"

struct yagl_glsl_pending
{
    struct yagl_list list;

    int token_index;
};

typedef enum
{
    yagl_glsl_asis,
    yagl_glsl_120,
    yagl_glsl_140,
    yagl_glsl_150,
} yagl_glsl_version;

struct yagl_glsl_state
{
    GLenum shader_type;

    const char *source;
    int source_len;
    int source_pos;

    int es3_supported;

    void *scanner;

    /*
     * These are modified during GLSL parsing.
     * @{
     */

    yagl_glsl_version patch_version;
    int patch_precision;
    int patch_builtins;
    int patch_gl2;
    int patch_max_varying_floats;

    int have_error;

    int have_version;

    int have_extensions;

    int frag_color_declared;
    int texture1d_declared;
    int texture1dproj_declared;
    int texture1dlod_declared;
    int texture1dprojlod_declared;
    int texture2d_declared;
    int texture2dproj_declared;
    int texture2dlod_declared;
    int texture2dprojlod_declared;
    int texture3d_declared;
    int texture3dproj_declared;
    int texture3dlod_declared;
    int texture3dprojlod_declared;
    int texturecube_declared;
    int texturecubelod_declared;

    // Each token is assigned an index.
    int token_index;

    // Output shader source header.
    struct yagl_vector header;

    // Output shader source.
    struct yagl_vector output;

    // Strings that are waiting to be inserted into 'output'
    // or to be discarded.
    struct yagl_list pending_list;

    // String that's waiting to be inserted into 'pending_list'.
    struct yagl_vector pending;

    // strdup'ed strings.
    struct yagl_vector strings;

    /*
     * @}
     */
};

int yagl_glsl_state_init(struct yagl_glsl_state *state,
                         GLenum shader_type,
                         const char *source,
                         int source_len,
                         int es3_supported);

void yagl_glsl_state_cleanup(struct yagl_glsl_state *state);

void yagl_glsl_state_set_version(struct yagl_glsl_state *state,
                                 int version,
                                 int is_es);

void yagl_glsl_state_set_error(struct yagl_glsl_state *state,
                               const char *msg);

void yagl_glsl_state_new_pending(struct yagl_glsl_state *state,
                                 const char *str);

void yagl_glsl_state_new_comment(struct yagl_glsl_state *state,
                                 int len);

void yagl_glsl_state_new_str_token(struct yagl_glsl_state *state,
                                   YYSTYPE *yylval,
                                   const char *value);

void yagl_glsl_state_new_integer_token(struct yagl_glsl_state *state,
                                       YYSTYPE *yylval,
                                       int value);

void yagl_glsl_state_flush_pending(struct yagl_glsl_state *state,
                                   int token_index);

void yagl_glsl_state_append_header(struct yagl_glsl_state *state,
                                   const char *str);

void yagl_glsl_state_append_output(struct yagl_glsl_state *state,
                                   const char *str);

void yagl_glsl_state_output_to_header(struct yagl_glsl_state *state);

/*
 * The caller owns the data, it must be freed with 'yagl_free'.
 */
char *yagl_glsl_state_get_output(struct yagl_glsl_state *state, int *output_len);

int yagl_glsl_parse(struct yagl_glsl_state *state);

#endif
