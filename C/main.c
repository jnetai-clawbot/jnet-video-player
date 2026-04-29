#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include <unistd.h>
#include <errno.h>
#include <math.h>
#include <time.h>
#include <poll.h>
#include <sys/mman.h>
#include <sys/timerfd.h>
#include <linux/input.h>

#include <wayland-client.h>
#include <wayland-egl.h>
#include <wayland-cursor.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES2/gl2.h>
#include <xkbcommon/xkbcommon.h>

#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
#include <libavutil/imgutils.h>
#include <libavutil/time.h>
#include <libswscale/swscale.h>
#include <libswresample/swresample.h>
#include <libavfilter/avfilter.h>

/* Generated protocol headers */
#include "xdg-shell.h"
#include "xdg-activation-v1.h"

#include "main.h"
#define APP_TITLE "J~NET Video Player"
static void app_cleanup(App *app);
static int  app_run(App *app);
static void app_create_window(App *app, int w, int h);
static void app_destroy_window(App *app);

/* ============================================================
 * EGL / Wayland helpers
 * ============================================================ */
static const EGLint egl_config_attribs[] = {
    EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
    EGL_RED_SIZE, 8,
    EGL_GREEN_SIZE, 8,
    EGL_BLUE_SIZE, 8,
    EGL_ALPHA_SIZE, 8,
    EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
    EGL_NONE
};

static const EGLint egl_context_attribs[] = {
    EGL_CONTEXT_CLIENT_VERSION, 2,
    EGL_NONE
};

/* Shader sources */
static const char *vert_shader_src =
    "attribute vec2 a_pos;\n"
    "attribute vec2 a_tex;\n"
    "varying vec2 v_tex;\n"
    "void main() {\n"
    "    gl_Position = vec4(a_pos, 0.0, 1.0);\n"
    "    v_tex = a_tex;\n"
    "}\n";

static const char *frag_shader_src =
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
        fprintf(stderr, "Shader compile error: %s\n", log);
        glDeleteShader(s);
        return 0;
    }
    return s;
}

static void gl_init_shaders(void) {
    GLuint vs = compile_shader(GL_VERTEX_SHADER, vert_shader_src);
    GLuint fs = compile_shader(GL_FRAGMENT_SHADER, frag_shader_src);
    gl_program = glCreateProgram();
    glAttachShader(gl_program, vs);
    glAttachShader(gl_program, fs);
    glLinkProgram(gl_program);
    GLint ok;
    glGetProgramiv(gl_program, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[512];
        glGetProgramInfoLog(gl_program, 512, NULL, log);
        fprintf(stderr, "Program link error: %s\n", log);
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
}

static void gl_render_tex(uint8_t *data, int width, int height, int win_w, int win_h) {
    if (!gl_program || !data) return;
    glViewport(0, 0, win_w, win_h);
    glClearColor(0.02f, 0.02f, 0.02f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    glUseProgram(gl_program);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, gl_tex_id);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, data);
    glUniform1i(gl_u_tex, 0);

    float aspect = (float)width / height;
    float win_aspect = (float)win_w / win_h;
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

static void gl_render_clear(void) {
    glViewport(0, 0, 100, 100);
    glClearColor(0.05f, 0.05f, 0.05f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
}

/* ============================================================
 * Wayland callbacks
 * ============================================================ */

static void wl_registry_handler(void *data, struct wl_registry *reg,
                                uint32_t id, const char *interface, uint32_t ver) {
    App *app = (App*)data;
    if (!strcmp(interface, "wl_compositor"))
        app->compositor = wl_registry_bind(reg, id, &wl_compositor_interface, 5);
    else if (!strcmp(interface, "wl_subcompositor"))
        app->subcompositor = wl_registry_bind(reg, id, &wl_subcompositor_interface, 1);
    else if (!strcmp(interface, "wl_seat"))
        app->seat = wl_registry_bind(reg, id, &wl_seat_interface, 7);
    else if (!strcmp(interface, "wl_shm"))
        app->shm = wl_registry_bind(reg, id, &wl_shm_interface, 1);
    else if (!strcmp(interface, "xdg_wm_base"))
        app->wm_base = wl_registry_bind(reg, id, &xdg_wm_base_interface, 3);
    else if (!strcmp(interface, "xdg_activation_v1"))
        app->activation = wl_registry_bind(reg, id, &xdg_activation_v1_interface, 1);
}

static void wl_registry_remove(void *data, struct wl_registry *reg, uint32_t id) {}

static const struct wl_registry_listener registry_listener = {
    wl_registry_handler,
    wl_registry_remove
};

static void xdg_wm_base_ping(void *data, struct xdg_wm_base *wm, uint32_t serial) {
    xdg_wm_base_pong(wm, serial);
}

static const struct xdg_wm_base_listener wm_base_listener = {
    xdg_wm_base_ping
};

static void xdg_surface_configure(void *data, struct xdg_surface *xdg_surf, uint32_t serial) {
    App *app = (App*)data;
    xdg_surface_ack_configure(xdg_surf, serial);
    if (app->win_width > 0 && app->win_height > 0 && app->egl_window && app->egl_init) {
        wl_egl_window_resize(app->egl_window, app->win_width, app->win_height, 0, 0);
    }
}

static const struct xdg_surface_listener xdg_surface_listener = {
    xdg_surface_configure
};

static void xdg_toplevel_configure(void *data, struct xdg_toplevel *tl,
                                   int32_t w, int32_t h, struct wl_array *states) {
    App *app = (App*)data;
    if (w > 0 && h > 0) { app->win_width = w; app->win_height = h; }
    uint32_t *state;
    wl_array_for_each(state, states) {
        if (*state == XDG_TOPLEVEL_STATE_FULLSCREEN) app->fullscreen = true;
    }
}

static void xdg_toplevel_close(void *data, struct xdg_toplevel *tl) {
    App *app = (App*)data;
    app->closed = true;
}

static const struct xdg_toplevel_listener toplevel_listener = {
    xdg_toplevel_configure,
    xdg_toplevel_close
};

/* Pointer */
static void pointer_enter(void *data, struct wl_pointer *ptr, uint32_t serial,
                          struct wl_surface *surf, wl_fixed_t sx, wl_fixed_t sy) {
    App *app = (App*)data;
    app->mouse_visible = true;
    app->ui_show_controls = true;
    app->ui_show_controls_time = av_gettime() / 1000000.0;
    if (app->cursor_surface && app->cursor_default) {
        wl_surface_attach(app->cursor_surface,
            wl_cursor_image_get_buffer(app->cursor_default->images[0]), 0, 0);
        wl_surface_commit(app->cursor_surface);
    }
}

static void pointer_leave(void *data, struct wl_pointer *ptr, uint32_t serial,
                          struct wl_surface *surf) {
    App *app = (App*)data;
    app->mouse_visible = false;
}

static void pointer_motion(void *data, struct wl_pointer *ptr, uint32_t time,
                           wl_fixed_t sx, wl_fixed_t sy) {
    App *app = (App*)data;
    app->mouse_x = wl_fixed_to_int(sx);
    app->mouse_y = wl_fixed_to_int(sy);
    app->ui_show_controls = true;
    app->ui_show_controls_time = av_gettime() / 1000000.0;
}

static void pointer_button(void *data, struct wl_pointer *ptr, uint32_t serial,
                           uint32_t time, uint32_t button, uint32_t state) {
    App *app = (App*)data;
    if (state == WL_POINTER_BUTTON_STATE_PRESSED) {
        if (button == BTN_LEFT) ui_handle_click(app, app->mouse_x, app->mouse_y);
        if (button == BTN_RIGHT) app->playlist_open = !app->playlist_open;
    }
}

static void pointer_axis(void *data, struct wl_pointer *ptr, uint32_t time,
                         uint32_t axis, wl_fixed_t value) {
    App *app = (App*)data;
    if (axis == WL_POINTER_AXIS_VERTICAL_SCROLL) {
        double v = -wl_fixed_to_double(value);
        playback_set_volume(app, fmax(0, fmin(1, app->volume + v * 0.05)));
    }
}

static const struct wl_pointer_listener pointer_listener = {
    pointer_enter, pointer_leave, pointer_motion, pointer_button, pointer_axis
};

/* Keyboard */
static void keyboard_keymap(void *data, struct wl_keyboard *kb, uint32_t fmt,
                            int fd, uint32_t size) {
    App *app = (App*)data;
    if (fmt != WL_KEYBOARD_KEYMAP_FORMAT_XKB_V1) { close(fd); return; }
    char *map_str = mmap(NULL, size, PROT_READ, MAP_PRIVATE, fd, 0);
    if (map_str == MAP_FAILED) { close(fd); return; }
    app->xkb_keymap = xkb_keymap_new_from_string(app->xkb_ctx, map_str,
        XKB_KEYMAP_FORMAT_TEXT_V1, 0);
    munmap(map_str, size);
    close(fd);
    if (!app->xkb_keymap) { fprintf(stderr, "Failed to compile keymap\n"); return; }
    app->xkb_state = xkb_state_new(app->xkb_keymap);
}

static void keyboard_key(void *data, struct wl_keyboard *kb, uint32_t serial,
                         uint32_t time, uint32_t key, uint32_t state_w) {
    App *app = (App*)data;
    if (state_w != WL_KEYBOARD_KEY_STATE_PRESSED) return;
    xkb_keysym_t sym = xkb_state_key_get_one_sym(app->xkb_state, key + 8);
    uint32_t keycode = key + 8;

    switch (sym) {
        case XKB_KEY_space:
            if (app->state == STATE_PLAYING) playback_pause(app);
            else if (app->state == STATE_PAUSED) playback_play(app);
            break;
        case XKB_KEY_Left:
            playback_seek(app, fmax(0, app->current_time - 10));
            break;
        case XKB_KEY_Right:
            playback_seek(app, fmin(app->duration, app->current_time + 10));
            break;
        case XKB_KEY_Down:
            playback_set_volume(app, fmax(0, app->volume - 0.1));
            break;
        case XKB_KEY_Up:
            playback_set_volume(app, fmin(1, app->volume + 0.1));
            break;
        case XKB_KEY_Tab:
            app->playlist_open = !app->playlist_open;
            break;
        case XKB_KEY_f:
        case XKB_KEY_F:
            if (app->fullscreen)
                xdg_toplevel_unset_fullscreen(app->xdg_toplevel);
            else
                xdg_toplevel_set_fullscreen(app->xdg_toplevel, NULL);
            break;
        case XKB_KEY_Escape:
            if (app->fullscreen)
                xdg_toplevel_unset_fullscreen(app->xdg_toplevel);
            app->playlist_open = false;
            break;
        case XKB_KEY_o:
        case XKB_KEY_O:
            playlist_open_file_dialog(app);
            break;
    }
}

static void keyboard_modifiers(void *data, struct wl_keyboard *kb, uint32_t serial,
                               uint32_t mods_dep, uint32_t mods_lat,
                               uint32_t mods_lck, uint32_t mods_grp) {
    App *app = (App*)data;
    xkb_state_update_mask(app->xkb_state, mods_dep, mods_lat, mods_lck, 0, 0, mods_grp);
}

static const struct wl_keyboard_listener keyboard_listener = {
    keyboard_keymap, NULL, NULL, keyboard_key, keyboard_modifiers
};

static void seat_caps(void *data, struct wl_seat *seat, uint32_t caps) {
    App *app = (App*)data;
    if ((caps & WL_SEAT_CAPABILITY_KEYBOARD) && !app->keyboard)
        app->keyboard = wl_seat_get_keyboard(seat);
    if ((caps & WL_SEAT_CAPABILITY_POINTER) && !app->pointer)
        app->pointer = wl_seat_get_pointer(seat);
}

static const struct wl_seat_listener seat_listener = {
    seat_caps
};

/* ============================================================
 * App core
 * ============================================================ */

static void app_init(App *app) {
    memset(app, 0, sizeof(App));
    app->state = STATE_STOPPED;
    app->volume = 0.8;
    app->video_stream_idx = -1;
    app->audio_stream_idx = -1;
    app->win_width = 1280;
    app->win_height = 720;
    app->seek_target = AV_NOPTS_VALUE;
    app->ui_show_controls = true;
    app->ui_show_controls_time = INFINITY;
    app->timer_fd = -1;
    app->xkb_ctx = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
    avformat_network_init();
}

static void app_cleanup(App *app) {
    if (app->xkb_state) xkb_state_unref(app->xkb_state);
    if (app->xkb_keymap) xkb_keymap_unref(app->xkb_keymap);
    if (app->xkb_ctx) xkb_context_unref(app->xkb_ctx);
    if (app->display) wl_display_disconnect(app->display);
    avformat_network_deinit();
}

static void app_create_window(App *app, int w, int h) {
    app->display = wl_display_connect(NULL);
    if (!app->display) {
        fprintf(stderr, "Cannot connect to Wayland display\n");
        exit(1);
    }

    app->registry = wl_display_get_registry(app->display);
    wl_registry_add_listener(app->registry, &registry_listener, app);
    wl_display_roundtrip(app->display);
    wl_display_roundtrip(app->display);

    if (!app->compositor || !app->wm_base) {
        fprintf(stderr, "Missing required Wayland protocols\n");
        exit(1);
    }

    xdg_wm_base_add_listener(app->wm_base, &wm_base_listener, app);

    app->surface = wl_compositor_create_surface(app->compositor);
    app->xdg_surface = xdg_wm_base_get_xdg_surface(app->wm_base, app->surface);
    xdg_surface_add_listener(app->xdg_surface, &xdg_surface_listener, app);

    app->xdg_toplevel = xdg_surface_get_toplevel(app->xdg_surface);
    xdg_toplevel_add_listener(app->xdg_toplevel, &toplevel_listener, app);
    xdg_toplevel_set_title(app->xdg_toplevel, APP_TITLE);
    xdg_toplevel_set_app_id(app->xdg_toplevel, "jnet-video-player");

    if (app->activation) {
        struct xdg_activation_token_v1 *token =
            xdg_activation_v1_get_activation_token(app->activation);
        xdg_activation_token_v1_set_app_id(token, "jnet-video-player");
        xdg_activation_token_v1_commit(token);
        xdg_activation_token_v1_destroy(token);
    }

    /* EGL init must happen BEFORE first commit/roundtrip */
    app->egl_disp = eglGetDisplay(app->display);
    if (app->egl_disp == EGL_NO_DISPLAY) {
        fprintf(stderr, "Failed to get EGL display\n");
        exit(1);
    }
    if (!eglInitialize(app->egl_disp, &app->egl_major, &app->egl_minor)) {
        fprintf(stderr, "Failed to initialize EGL\n");
        exit(1);
    }
    eglBindAPI(EGL_OPENGL_ES_API);

    EGLint count = 0;
    eglChooseConfig(app->egl_disp, egl_config_attribs, &app->egl_config, 1, &count);
    if (count == 0) {
        fprintf(stderr, "No suitable EGL config found\n");
        exit(1);
    }

    app->egl_ctx = eglCreateContext(app->egl_disp, app->egl_config,
                                     EGL_NO_CONTEXT, egl_context_attribs);
    app->win_width = w;
    app->win_height = h;
    app->egl_window = wl_egl_window_create(app->surface, w, h);
    app->egl_surf = eglCreateWindowSurface(app->egl_disp, app->egl_config,
                                            app->egl_window, NULL);
    eglMakeCurrent(app->egl_disp, app->egl_surf, app->egl_surf, app->egl_ctx);
    app->egl_init = true;

    gl_init_shaders();
    gl_render_clear();
    eglSwapBuffers(app->egl_disp, app->egl_surf);

    if (app->activation) {
        struct xdg_activation_token_v1 *token =
            xdg_activation_v1_get_activation_token(app->activation);
        xdg_activation_token_v1_set_app_id(token, "jnet-video-player");
        xdg_activation_token_v1_commit(token);
        xdg_activation_token_v1_destroy(token);
    }

    wl_surface_commit(app->surface);
    wl_display_roundtrip(app->display);

    /* Seat */
    if (app->seat) {
        wl_seat_add_listener(app->seat, &seat_listener, app);
    }

    /* Cursor */
    app->cursor_theme = wl_cursor_theme_load(NULL, 24, app->shm);
    if (app->cursor_theme)
        app->cursor_default = wl_cursor_theme_get_cursor(app->cursor_theme, "left_ptr");
    app->cursor_surface = wl_compositor_create_surface(app->compositor);

    /* Timer */
    app->timer_fd = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK);
    struct itimerspec ts = { .it_interval = {0, 16 * 1000000}, .it_value = {0, 16 * 1000000} };
    timerfd_settime(app->timer_fd, 0, &ts, NULL);
}

static void app_destroy_window(App *app) {
    if (app->timer_fd >= 0) close(app->timer_fd);
    if (app->cursor_surface) wl_surface_destroy(app->cursor_surface);
    if (app->cursor_theme) wl_cursor_theme_destroy(app->cursor_theme);
    if (app->pointer) wl_pointer_destroy(app->pointer);
    if (app->keyboard) wl_keyboard_destroy(app->keyboard);
    if (app->egl_init) {
        eglMakeCurrent(app->egl_disp, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        eglDestroySurface(app->egl_disp, app->egl_surf);
        eglDestroyContext(app->egl_disp, app->egl_ctx);
        eglTerminate(app->egl_disp);
    }
    if (app->egl_window) wl_egl_window_destroy(app->egl_window);
    if (app->xdg_toplevel) xdg_toplevel_destroy(app->xdg_toplevel);
    if (app->xdg_surface) xdg_surface_destroy(app->xdg_surface);
    if (app->surface) wl_surface_destroy(app->surface);
    if (app->wm_base) xdg_wm_base_destroy(app->wm_base);
    if (app->compositor) wl_compositor_destroy(app->compositor);
    if (app->shm) wl_shm_destroy(app->shm);
    if (app->registry) wl_registry_destroy(app->registry);
    if (app->activation) xdg_activation_v1_destroy(app->activation);
}

static void app_render_frame(App *app) {
    if (!app->egl_init) return;

    uint8_t *rgb = NULL;
    int rgb_size = 0;
    double pts = 0;
    
    if (get_latest_frame(&rgb, &rgb_size, &pts) && app->video_width > 0 && app->video_height > 0) {
        gl_render_tex(rgb, app->video_width, app->video_height, app->win_width, app->win_height);
        av_free(rgb);
    } else {
        gl_render_clear();
    }

    if (app->ui_show_controls) {
        ui_draw_controls(app);
    }
    if (app->playlist_open) {
        ui_draw_playlist(app);
    }

    eglSwapBuffers(app->egl_disp, app->egl_surf);
}

static int app_run(App *app) {
    struct pollfd fds[2];
    fds[0].fd = wl_display_get_fd(app->display);
    fds[0].events = POLLIN;
    fds[1].fd = app->timer_fd;
    fds[1].events = POLLIN;

    while (!app->closed) {
        struct pollfd pfd[2];
        memcpy(pfd, fds, sizeof(fds));
        int ret = poll(pfd, 2, 16);
        if (ret < 0) break;

        if (pfd[0].revents & POLLIN) {
            wl_display_dispatch(app->display);
        }

        if (pfd[1].revents & POLLIN) {
            uint64_t exp;
            read(app->timer_fd, &exp, sizeof(exp));
            
            if (app->seek_pending) {
                int64_t ts = app->seek_target;
                av_seek_frame(app->fmt_ctx, -1, ts, AVSEEK_FLAG_BACKWARD);
                avcodec_flush_buffers(app->video_dec_ctx);
                if (app->audio_dec_ctx) avcodec_flush_buffers(app->audio_dec_ctx);
                app->seek_pending = false;
                app->eof_reached = false;
            }
            
            app_render_frame(app);
        }

        double now = av_gettime() / 1000000.0;
        if (app->ui_show_controls && (now - app->ui_show_controls_time) > 3.0) {
            app->ui_show_controls = false;
        }

        if (app->state == STATE_STOPPED) {
            gl_render_clear();
            eglSwapBuffers(app->egl_disp, app->egl_surf);
        }
    }

    return 0;
}

/* Global app reference */
App *g_app;

/* ============================================================
 * Entry point
 * ============================================================ */
int main(int argc, char *argv[]) {
    App app;
    g_app = &app;
    app_init(&app);
    app_create_window(&app, 1280, 720);
    renderer_init();
    playback_init(&app);

    for (int i = 1; i < argc; i++) {
        playlist_add(&app, argv[i]);
    }
    if (app.playlist.count == 0) {
        /* Will start black, user can press 'o' to open */
    } else {
        app.playlist.current = 0;
        playback_open(&app, app.playlist.entries[0].path);
    }

    int ret = app_run(&app);
    playback_shutdown();
    renderer_shutdown();
    app_destroy_window(&app);
    app_cleanup(&app);
    return ret;
}
