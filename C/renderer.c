/*
 * renderer.c - EGL/GLES2 renderer for J~NET Video Player
 * Handles video frame rendering and UI overlay drawing
 */

#include "main.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES2/gl2.h>
#include <libavutil/rational.h>

/* Shader sources */
static const char *vert_src =
    "attribute vec2 a_pos;\n"
    "attribute vec2 a_tex;\n"
    "varying vec2 v_tex;\n"
    "void main() {\n"
    "    gl_Position = vec4(a_pos, 0.0, 1.0);\n"
    "    v_tex = a_tex;\n"
    "}\n";

static const char *frag_src =
    "precision mediump float;\n"
    "varying vec2 v_tex;\n"
    "uniform sampler2D u_tex;\n"
    "void main() {\n"
    "    gl_FragColor = texture2D(u_tex, v_tex);\n"
    "}\n";

static GLuint gl_program = 0;
static GLuint gl_tex_id = 0;
static GLint gl_a_pos = -1, gl_a_tex = -1, gl_u_tex = -1;

static GLuint compile_shader(GLenum type, const char *src) {
    GLuint s = glCreateShader(type);
    glShaderSource(s, 1, &src, NULL);
    glCompileShader(s);
    GLint ok;
    glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[512];
        glGetShaderInfoLog(s, 512, NULL, log);
        fprintf(stderr, "Shader error: %s\n", log);
        glDeleteShader(s);
        return 0;
    }
    return s;
}

void renderer_init(void) {
    GLuint vs = compile_shader(GL_VERTEX_SHADER, vert_src);
    GLuint fs = compile_shader(GL_FRAGMENT_SHADER, frag_src);
    gl_program = glCreateProgram();
    glAttachShader(gl_program, vs);
    glAttachShader(gl_program, fs);
    glLinkProgram(gl_program);
    GLint ok;
    glGetProgramiv(gl_program, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[512];
        glGetProgramInfoLog(gl_program, 512, NULL, log);
        fprintf(stderr, "Link error: %s\n", log);
    }
    glDeleteShader(vs);
    glDeleteShader(fs);
    gl_a_pos = glGetAttribLocation(gl_program, "a_pos");
    gl_a_tex = glGetAttribLocation(gl_program, "a_tex");
    gl_u_tex = glGetUniformLocation(gl_program, "u_tex");
    glGenTextures(1, &gl_tex_id);
    glBindTexture(GL_TEXTURE_2D, gl_tex_id);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}

void renderer_shutdown(void) {
    if (gl_tex_id) glDeleteTextures(1, &gl_tex_id);
    if (gl_program) glDeleteProgram(gl_program);
}

void renderer_render_frame(uint8_t *rgb_data, int width, int height,
                           int win_width, int win_height) {
    if (!rgb_data || width <= 0 || height <= 0) return;
    if (!gl_program) return;
    
    glViewport(0, 0, win_width, win_height);
    glClearColor(0.02f, 0.02f, 0.02f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    glUseProgram(gl_program);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, gl_tex_id);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0,
                 GL_RGB, GL_UNSIGNED_BYTE, rgb_data);
    glUniform1i(gl_u_tex, 0);

    /* Aspect-correct scaling */
    float aspect = (float)width / height;
    float win_aspect = (float)win_width / win_height;
    float sx, sy;
    if (aspect > win_aspect) { sx = 1.0f; sy = win_aspect / aspect; }
    else { sx = aspect / win_aspect; sy = 1.0f; }

    GLfloat verts[] = {
        -sx, -sy,  sx, -sy,  sx, sy,
        -sx, -sy,  sx, sy,  -sx, sy
    };
    GLfloat texs[] = { 0, 1, 1, 1, 1, 0, 0, 1, 0, 0 };
    glEnableVertexAttribArray(gl_a_pos);
    glVertexAttribPointer(gl_a_pos, 2, GL_FLOAT, GL_FALSE, 0, verts);
    glEnableVertexAttribArray(gl_a_tex);
    glVertexAttribPointer(gl_a_tex, 2, GL_FLOAT, GL_FALSE, 0, texs);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glDisableVertexAttribArray(gl_a_pos);
    glDisableVertexAttribArray(gl_a_tex);
}
