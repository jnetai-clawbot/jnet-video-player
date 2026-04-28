/*
 * main.h - J~NET Video Player header
 */

#ifndef JNET_VIDEO_PLAYER_H
#define JNET_VIDEO_PLAYER_H

#include <stdbool.h>
#include <stdint.h>
#include <sys/types.h>

/* Forward declare */
typedef struct App App;

/* Playlist max */
#define MAX_PLAYLIST 256
#define PATH_MAX_LEN 1024

/* Playback states */
typedef enum {
    STATE_STOPPED,
    STATE_PLAYING,
    STATE_PAUSED,
    STATE_ENDED
} PlayState;

/* Playlist entry */
typedef struct {
    char path[PATH_MAX_LEN];
    char name[256];
} PlaylistEntry;

/* Playlist */
typedef struct {
    PlaylistEntry entries[MAX_PLAYLIST];
    int count;
    int current;
    int scroll;
} Playlist;

/* Wayland forward declarations */
struct wl_display;
struct wl_registry;
struct wl_compositor;
struct wl_subcompositor;
struct wl_seat;
struct wl_pointer;
struct wl_keyboard;
struct wl_shm;
struct wl_surface;
struct wl_egl_window;
struct wl_cursor_theme;
struct wl_cursor;
struct xdg_wm_base;
struct xdg_surface;
struct xdg_toplevel;
struct xdg_activation_v1;
struct xkb_context;
struct xkb_keymap;
struct xkb_state;
struct SwsContext;
struct pa_simple;

/* FFmpeg forward declarations */
typedef struct AVFormatContext AVFormatContext;
typedef struct AVCodecContext AVCodecContext;
typedef struct AVFrame AVFrame;
typedef struct AVPacket AVPacket;
#include <libavutil/rational.h>

/* App structure */
struct App {
    /* Wayland */
    struct wl_display *display;
    struct wl_registry *registry;
    struct wl_compositor *compositor;
    struct wl_subcompositor *subcompositor;
    struct wl_seat *seat;
    struct wl_pointer *pointer;
    struct wl_keyboard *keyboard;
    struct wl_shm *shm;
    struct wl_surface *surface;
    struct wl_egl_window *egl_window;
    struct wl_cursor_theme *cursor_theme;
    struct wl_cursor *cursor_default;
    struct wl_surface *cursor_surface;
    struct xdg_wm_base *wm_base;
    struct xdg_surface *xdg_surface;
    struct xdg_toplevel *xdg_toplevel;
    struct xdg_activation_v1 *activation;

    /* EGL */
    void *egl_disp;
    void *egl_config;
    void *egl_ctx;
    void *egl_surf;
    int egl_major, egl_minor;
    bool egl_init;

    /* XKB */
    struct xkb_context *xkb_ctx;
    struct xkb_keymap *xkb_keymap;
    struct xkb_state *xkb_state;

    /* Window */
    int win_width, win_height;
    bool fullscreen;
    bool closed;

    /* Decoder */
    AVFormatContext *fmt_ctx;
    AVCodecContext *video_dec_ctx;
    AVCodecContext *audio_dec_ctx;
    int video_stream_idx;
    int audio_stream_idx;
    struct SwsContext *sws_ctx;
    AVFrame *frame;
    AVPacket *pkt;
    uint8_t *video_buf;
    int video_buf_size;
    int video_width, video_height;
    AVRational video_time_base;

    /* Audio */
    struct pa_simple *pa_stream;
    bool audio_enabled;

    /* Playback */
    PlayState state;
    Playlist playlist;
    double duration;
    double current_time;
    double volume;
    bool volume_changed;
    int64_t seek_target;
    bool seek_pending;
    bool eof_reached;

    /* Timer */
    int timer_fd;
    void *frame_callback;

    /* UI */
    bool mouse_visible;
    int mouse_x, mouse_y;
    bool ui_show_controls;
    double ui_show_controls_time;
    bool dragging_seek;
    double drag_seek_value;
    bool playlist_open;
    int playlist_scroll;
};

/* Global app reference */
extern App *g_app;

/* External function declarations */
void renderer_init(void);
void renderer_shutdown(void);

int playback_open(App *app, const char *path);
int playback_open_internal(App *app, const char *path);
void playback_close(App *app);
void playback_play(App *app);
void playback_pause(App *app);
void playback_seek(App *app, double pos);
void playback_set_volume(App *app, double vol);
int get_latest_frame(uint8_t **out_rgb, int *out_size, double *out_pts);
void playback_init(App *app);
void playback_shutdown(void);

void playlist_add(App *app, const char *path);
void playlist_clear(App *app);
int playlist_next(App *app);
int playlist_prev(App *app);
void playlist_open_file_dialog(App *app);
void playlist_remove_index(App *app, int idx);
void playlist_move_up(App *app, int idx);
void playlist_move_down(App *app, int idx);
int playlist_add_directory(App *app, const char *dir_path);

void ui_draw_controls(App *app);
void ui_draw_playlist(App *app);
void ui_draw_center_indicator(App *app);
void ui_handle_click(App *app, int x, int y);

#endif /* JNET_VIDEO_PLAYER_H */
