#define GL_GLES2_PROTOTYPES 1
#include "main.h"
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <math.h>
#include <GLES2/gl2platform.h>
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#include "xdg-shell.h"


/* Colors - dark theme */
static const float C_BG_VAL[3]     = {0.05f, 0.05f, 0.06f};
static const float C_BG2_VAL[3]    = {0.10f, 0.10f, 0.12f};
static const float C_SURFACE_VAL[3]= {0.15f, 0.15f, 0.18f};
static const float C_ACCENT_VAL[3] = {0.15f, 0.65f, 0.20f};
static const float C_ACCENT2_VAL[3]= {0.20f, 0.75f, 0.25f};
static const float C_TEXT_VAL[3]   = {0.85f, 0.85f, 0.90f};
static const float C_TEXT_DIM[3]   = {0.50f, 0.50f, 0.55f};
static const float C_DANGER_VAL[3] = {0.75f, 0.20f, 0.15f};
static const float C_SEEK_BG[3]    = {0.20f, 0.20f, 0.22f};
static const float C_PROGRESS[3]  = {0.20f, 0.60f, 0.20f};

/* Button state */
typedef enum {
    BTN_NORMAL,
    BTN_HOVER,
    BTN_PRESSED
} BtnState;

static BtnState btn_state = BTN_NORMAL;

/* Helper: set current color via generic attribute (index 3) */
static void set_color(float r, float g, float b, float a) {
    glVertexAttrib4f(3, r, g, b, a);
}

/* Helper: reset color to white */
static void reset_color(void) {
    glVertexAttrib4f(3, 1.0f, 1.0f, 1.0f, 1.0f);
}

/* Helper: draw rounded rectangle */
static void draw_rounded_rect(float x, float y, float w, float h,
                               float r, float g, float b, float a, float radius) {
    (void)radius;
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    set_color(r, g, b, a);
    
    /* Simple rect with border */
    GLfloat verts[12] = {
        x, y,      x+w, y,      x+w, y+h,
        x, y,      x+w, y+h,    x, y+h
    };
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, verts);
    glEnableVertexAttribArray(0);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glDisableVertexAttribArray(0);
    reset_color();
    glDisable(GL_BLEND);
}

/* Helper: draw horizontal gradient bar */
static void draw_bar(float x, float y, float w, float h,
                     float r1, float g1, float b1,
                     float r2, float g2, float b2,
                     float progress) {
    /* Background */
    draw_rounded_rect(x, y, w, h, C_SEEK_BG[0], C_SEEK_BG[1], C_SEEK_BG[2], 1.0f, 0.95f);
    /* Progress portion */
    if (progress > 0 && progress <= 1.0f) {
        float pw = w * progress;
        draw_rounded_rect(x, y, pw, h, C_PROGRESS[0], C_PROGRESS[1], C_PROGRESS[2], 1.0f, 0.95f);
    }
}

/* Helper: check if point is in rectangle */
static bool in_rect(int px, int py, int x, int y, int w, int h) {
    return px >= x && px <= x + w && py >= y && py <= y + h;
}

/* Draw text (simplified - uses GL program) */
static void draw_text(float x, float y, const char *text, float size,
                      float r, float g, float b, float a) {
    (void)x; (void)y; (void)text; (void)size; (void)r; (void)g; (void)b; (void)a;
    /* Full text rendering requires FreeType.
     * For now, we draw the control bar text via the window surface
     * using a separate cairo-like approach or Pango.
     * Placeholder: just return. */
}

/* Draw the main controls overlay */
void ui_draw_controls(App *app) {
    if (!app->ui_show_controls) return;
    if (app->state == STATE_STOPPED && app->playlist.count == 0) return;

    int w = app->win_width;
    int h = app->win_height;
    int bar_h = 80;
    int bar_y = h - bar_h;

    /* Semi-transparent gradient overlay at bottom */
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    GLfloat bg_verts[] = {
        0, bar_y - 40,  0, h,
        w, bar_y - 40,  w, h
    };
    set_color(C_BG_VAL[0], C_BG_VAL[1], C_BG_VAL[2], 0.85f);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, bg_verts);
    glEnableVertexAttribArray(0);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glDisableVertexAttribArray(0);
    reset_color();
    glDisable(GL_BLEND);

    /* Top border line */
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    set_color(C_ACCENT_VAL[0], C_ACCENT_VAL[1], C_ACCENT_VAL[2], 0.6f);
    GLfloat line_verts[] = { 0, bar_y - 40, w, bar_y - 40 };
    glLineWidth(1.0f);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, line_verts);
    glEnableVertexAttribArray(0);
    glDrawArrays(GL_LINES, 0, 2);
    glDisableVertexAttribArray(0);
    reset_color();
    glDisable(GL_BLEND);

    /* Title bar at top when in fullscreen */
    if (app->fullscreen && app->fmt_ctx) {
        int top_h = 36;
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        set_color(C_BG_VAL[0], C_BG_VAL[1], C_BG_VAL[2], 0.85f);
        GLfloat top_verts[] = { 0, h - top_h,  0, h,  w, h - top_h,  w, h };
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, top_verts);
        glEnableVertexAttribArray(0);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
        glDisableVertexAttribArray(0);
        reset_color();
        glDisable(GL_BLEND);

        /* File name */
        const char *name = app->playlist.count > 0 ?
            app->playlist.entries[app->playlist.current].name : "No file";
        (void)name; /* would render text here */
    }

    /* Seek bar */
    int seek_x = 20;
    int seek_y = bar_y + 16;
    int seek_w = w - 40;
    int seek_h = 8;

    double prog = app->duration > 0 ? app->current_time / app->duration : 0;

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    /* Track bg */
    set_color(0.20f, 0.20f, 0.22f, 1.0f);
    GLfloat track_verts[] = {
        seek_x, seek_y,          seek_x + seek_w, seek_y,
        seek_x + seek_w, seek_y + seek_h,
        seek_x, seek_y,          seek_x + seek_w, seek_y + seek_h,
        seek_x, seek_y + seek_h
    };
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, track_verts);
    glEnableVertexAttribArray(0);
    glDrawArrays(GL_TRIANGLES, 0, 6);

    /* Progress fill */
    int prog_w = (int)(seek_w * prog);
    if (prog_w > 0) {
        set_color(0.20f, 0.60f, 0.20f, 1.0f);
        GLfloat prog_verts[] = {
            seek_x, seek_y,              seek_x + prog_w, seek_y,
            seek_x + prog_w, seek_y + seek_h,
            seek_x, seek_y,              seek_x + prog_w, seek_y + seek_h,
            seek_x, seek_y + seek_h
        };
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, prog_verts);
        glDrawArrays(GL_TRIANGLES, 0, 6);
    }
    glDisableVertexAttribArray(0);

    /* Seek handle */
    int handle_x = seek_x + prog_w;
    int handle_y = seek_y - 4;
    set_color(C_ACCENT2_VAL[0], C_ACCENT2_VAL[1], C_ACCENT2_VAL[2], 1.0f);
    GLfloat handle_verts[] = {
        handle_x - 6, handle_y,      handle_x + 6, handle_y,
        handle_x + 6, handle_y + seek_h + 8,
        handle_x - 6, handle_y,      handle_x + 6, handle_y + seek_h + 8,
        handle_x - 6, handle_y + seek_h + 8
    };
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, handle_verts);
    glEnableVertexAttribArray(0);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glDisableVertexAttribArray(0);
    reset_color();
    glDisable(GL_BLEND);

    /* Time display */
    int t = (int)app->current_time;
    int d = (int)app->duration;
    char time_str[64];
    snprintf(time_str, sizeof(time_str), "%02d:%02d:%02d / %02d:%02d:%02d",
            t/3600, (t/60)%60, t%60,
            d/3600, (d/60)%60, d%60);
    (void)time_str; /* text rendering placeholder */

    /* Control buttons row */
    int btn_y = bar_y + 38;
    int left = 20;

    /* Previous */
    int btn_w = 44, btn_h = 28, btn_gap = 8;
    draw_rounded_rect(left, btn_y, btn_w, btn_h, 0.18f, 0.18f, 0.20f, 0.9f, 4.0f);
    /* ◄◄ symbol - use 'P' label for now */
    left += btn_w + btn_gap;

    /* Play/Pause */
    bool is_playing = (app->state == STATE_PLAYING);
    if (is_playing) {
        draw_rounded_rect(left, btn_y, btn_w + 4, btn_h + 4, C_ACCENT2_VAL[0], C_ACCENT2_VAL[1], C_ACCENT2_VAL[2], 0.95f, 4.0f);
    } else {
        draw_rounded_rect(left, btn_y, btn_w + 4, btn_h + 4, C_ACCENT_VAL[0], C_ACCENT_VAL[1], C_ACCENT_VAL[2], 0.95f, 4.0f);
    }
    left += btn_w + 4 + btn_gap;

    /* Stop */
    draw_rounded_rect(left, btn_y, btn_w, btn_h, C_DANGER_VAL[0], C_DANGER_VAL[1], C_DANGER_VAL[2], 0.9f, 4.0f);
    left += btn_w + btn_gap;

    /* Next */
    draw_rounded_rect(left, btn_y, btn_w, btn_h, 0.18f, 0.18f, 0.20f, 0.9f, 4.0f);
    left += btn_w + btn_gap + 20;

    /* Volume slider */
    int vol_x = left, vol_y = btn_y + 4, vol_w = 80, vol_h = 6;
    double vol = app->volume;
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    set_color(0.20f, 0.20f, 0.22f, 1.0f);
    GLfloat vol_track[] = {
        vol_x, vol_y,          vol_x + vol_w, vol_y,
        vol_x + vol_w, vol_y + vol_h,
        vol_x, vol_y,          vol_x + vol_w, vol_y + vol_h,
        vol_x, vol_y + vol_h
    };
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, vol_track);
    glEnableVertexAttribArray(0);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    set_color(0.20f, 0.60f, 0.20f, 1.0f);
    GLfloat vol_fill[] = {
        vol_x, vol_y,              vol_x + (int)(vol_w * vol), vol_y,
        vol_x + (int)(vol_w * vol), vol_y + vol_h,
        vol_x, vol_y,              vol_x + (int)(vol_w * vol), vol_y + vol_h,
        vol_x, vol_y + vol_h
    };
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, vol_fill);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glDisableVertexAttribArray(0);
    reset_color();
    glDisable(GL_BLEND);
    left += vol_w + 20;

    /* Right side buttons */
    int right = w - 20;
    int fs_btn_w = 36;

    /* Fullscreen */
    right -= fs_btn_w;
    draw_rounded_rect(right, btn_y, fs_btn_w, btn_h, 0.25f, 0.25f, 0.28f, 0.9f, 4.0f);

    right -= btn_gap + fs_btn_w;
    /* Playlist */
    if (app->playlist_open) {
        draw_rounded_rect(right, btn_y, fs_btn_w, btn_h, C_ACCENT_VAL[0], C_ACCENT_VAL[1], C_ACCENT_VAL[2], 0.95f, 4.0f);
    } else {
        draw_rounded_rect(right, btn_y, fs_btn_w, btn_h, 0.25f, 0.25f, 0.28f, 0.9f, 4.0f);
    }

    right -= btn_gap + fs_btn_w;
    /* Open file */
    draw_rounded_rect(right, btn_y, fs_btn_w, btn_h, 0.25f, 0.25f, 0.28f, 0.9f, 4.0f);
}

/* Draw the playlist sidebar */
void ui_draw_playlist(App *app) {
    int pw = app->win_width / 3;
    int ph = app->win_height;
    int x = 0;

    /* Background panel */
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    set_color(C_BG2_VAL[0], C_BG2_VAL[1], C_BG2_VAL[2], 0.96f);
    GLfloat bg_verts[] = { x, 0,  x + pw, 0,  x + pw, ph,  x, ph };
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, bg_verts);
    glEnableVertexAttribArray(0);
    glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
    glDisableVertexAttribArray(0);

    /* Right border */
    set_color(C_SURFACE_VAL[0], C_SURFACE_VAL[1], C_SURFACE_VAL[2], 0.8f);
    GLfloat border[] = { x + pw, 0,  x + pw, ph };
    glLineWidth(2.0f);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, border);
    glEnableVertexAttribArray(0);
    glDrawArrays(GL_LINES, 0, 2);
    glDisableVertexAttribArray(0);
    reset_color();
    glDisable(GL_BLEND);

    /* Header */
    int header_h = 44;
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    set_color(C_SURFACE_VAL[0], C_SURFACE_VAL[1], C_SURFACE_VAL[2], 1.0f);
    GLfloat hdr_verts[] = { x, 0,  x + pw, 0,  x + pw, header_h,  x, header_h };
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, hdr_verts);
    glEnableVertexAttribArray(0);
    glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
    glDisableVertexAttribArray(0);
    reset_color();
    glDisable(GL_BLEND);

    /* Title text */
    char title[64];
    snprintf(title, sizeof(title), "Playlist (%d)", app->playlist.count);
    (void)title;

    /* Playlist items */
    int item_h = 36;
    int y = header_h + 4;
    int vis_count = (ph - header_h - 8) / (item_h + 2);

    for (int i = app->playlist_scroll; i < app->playlist.count && i < app->playlist_scroll + vis_count; i++) {
        PlaylistEntry *e = &app->playlist.entries[i];
        int is_current = (i == app->playlist.current);

        if (is_current) {
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            set_color(0.15f, 0.45f, 0.15f, 0.9f);
            GLfloat cur_verts[] = { x + 4, y,  x + pw - 4, y,
                                    x + pw - 4, y + item_h,
                                    x + 4, y,  x + pw - 4, y + item_h,  x + 4, y + item_h };
            glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, cur_verts);
            glEnableVertexAttribArray(0);
            glDrawArrays(GL_TRIANGLES, 0, 6);
            glDisableVertexAttribArray(0);
            reset_color();
            glDisable(GL_BLEND);
        }

        /* Item name (text placeholder) */
        (void)e->name;
        y += item_h + 2;
    }

    /* Scroll indicators */
    if (app->playlist_scroll > 0) {
        /* Up arrow */
    }
    if (app->playlist_scroll + vis_count < app->playlist.count) {
        /* Down arrow */
    }
}

/* Draw center play/pause indicator */
void ui_draw_center_indicator(App *app) {
    if (app->state == STATE_PLAYING || app->state == STATE_PAUSED) {
        /* Don't show when controls are visible */
        if (app->ui_show_controls) return;
    }

    int cx = app->win_width / 2;
    int cy = app->win_height / 2;
    int sz = 80;

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    set_color(0.0f, 0.0f, 0.0f, 0.6f);
    /* Circle background */
    GLfloat circ_verts[64];
    int n = 32;
    for (int i = 0; i < n; i++) {
        float a = i * 2 * (float)M_PI / n;
        float r = sz / 2.0f;
        circ_verts[i*2] = cx + r * cosf(a);
        circ_verts[i*2+1] = cy + r * sinf(a);
    }
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, circ_verts);
    glEnableVertexAttribArray(0);
    glDrawArrays(GL_TRIANGLE_FAN, 0, n);
    glDisableVertexAttribArray(0);

    /* Play/Pause symbol */
    if (app->state == STATE_PLAYING) {
        /* Pause icon */
        set_color(1.0f, 1.0f, 1.0f, 0.9f);
        int bar_w = 8, bar_h = 30, gap = 8;
        GLfloat pause_verts[] = {
            cx - gap - bar_w, cy - bar_h/2,
            cx - gap, cy - bar_h/2,
            cx - gap, cy + bar_h/2,
            cx - gap - bar_w, cy + bar_h/2,
            cx + gap, cy - bar_h/2,
            cx + gap + bar_w, cy - bar_h/2,
            cx + gap + bar_w, cy + bar_h/2,
            cx + gap, cy + bar_h/2
        };
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, pause_verts);
        glDrawArrays(GL_TRIANGLE_FAN, 0, 8);
    } else {
        /* Play icon */
        set_color(1.0f, 1.0f, 1.0f, 0.9f);
        GLfloat play_verts[] = {
            cx - 15, cy - 20,
            cx + 25, cy,
            cx - 15, cy + 20
        };
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, play_verts);
        glEnableVertexAttribArray(0);
        glDrawArrays(GL_TRIANGLES, 0, 3);
        glDisableVertexAttribArray(0);
    }
    reset_color();
    glDisable(GL_BLEND);
}

/* Handle mouse click */
void ui_handle_click(App *app, int x, int y) {
    int w = app->win_width;
    int h = app->win_height;
    int bar_h = 80;
    int bar_y = h - bar_h;

    /* Playlist panel click */
    if (app->playlist_open) {
        int pw = w / 3;
        if (x < pw) {
            int header_h = 44;
            int item_h = 36;
            int item_y = header_h + 4;
            int idx = (y - item_y) / (item_h + 2) + app->playlist_scroll;
            if (idx >= 0 && idx < app->playlist.count) {
                app->playlist.current = idx;
                playback_open(app, app->playlist.entries[idx].path);
            }
            return;
        }
    }

    /* Seek bar click */
    int seek_x = 20, seek_w = w - 40, seek_h = 8;
    int seek_y = bar_y + 16;
    if (y >= seek_y - 10 && y <= seek_y + seek_h + 10) {
        double pct = (double)(x - seek_x) / seek_w;
        pct = fmax(0, fmin(1, pct));
        playback_seek(app, app->duration * pct);
        return;
    }

    /* Volume slider click */
    int vol_x = 340, vol_w = 80, vol_h = 6;
    int vol_y = bar_y + 42;
    if (x >= vol_x && x <= vol_x + vol_w && y >= vol_y - 10 && y <= vol_y + vol_h + 10) {
        double pct = (double)(x - vol_x) / vol_w;
        playback_set_volume(app, pct);
        return;
    }

    /* Button clicks */
    int btn_y = bar_y + 38;
    int left = 20;
    int btn_w = 44, btn_h = 28, btn_gap = 8;

    /* Prev */
    if (in_rect(x, y, left, btn_y, btn_w, btn_h)) {
        playlist_prev(app);
        return;
    }
    left += btn_w + btn_gap;

    /* Play/Pause */
    if (in_rect(x, y, left, btn_y, btn_w + 4, btn_h + 4)) {
        if (app->state == STATE_PLAYING) playback_pause(app);
        else playback_play(app);
        return;
    }
    left += btn_w + 4 + btn_gap;

    /* Stop */
    if (in_rect(x, y, left, btn_y, btn_w, btn_h)) {
        playback_close(app);
        return;
    }
    left += btn_w + btn_gap;

    /* Next */
    if (in_rect(x, y, left, btn_y, btn_w, btn_h)) {
        playlist_next(app);
        return;
    }
    left += btn_w + btn_gap + 20;

    /* Skip to vol slider */
    left = 340 + 80 + 20;

    /* Right side buttons */
    int right = w - 20;
    int fs_btn_w = 36;

    /* Fullscreen */
    right -= fs_btn_w;
    if (in_rect(x, y, right, btn_y, fs_btn_w, btn_h)) {
        if (app->fullscreen)
            xdg_toplevel_unset_fullscreen(app->xdg_toplevel);
        else
            xdg_toplevel_set_fullscreen(app->xdg_toplevel, NULL);
        return;
    }
    right -= btn_gap + fs_btn_w;

    /* Playlist toggle */
    if (in_rect(x, y, right, btn_y, fs_btn_w, btn_h)) {
        app->playlist_open = !app->playlist_open;
        return;
    }
    right -= btn_gap + fs_btn_w;

    /* Open file */
    if (in_rect(x, y, right, btn_y, fs_btn_w, btn_h)) {
        playlist_open_file_dialog(app);
        return;
    }

    /* Click on video area: toggle play/pause */
    if (y < bar_y - 40) {
        if (app->state == STATE_PLAYING) playback_pause(app);
        else if (app->fmt_ctx) playback_play(app);
    }
}
