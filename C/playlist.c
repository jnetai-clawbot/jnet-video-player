/*
 * playlist.c - Playlist management for J~NET Video Player
 */

#include "main.h"
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <dirent.h>
#include <sys/stat.h>


/* File extension filter */
static const char *video_exts[] = {
    ".mp4", ".mkv", ".avi", ".mov", ".webm", ".flv",
    ".wmv", ".mpg", ".mpeg", ".m4v", ".3gp", ".ogv",
    ".mp3", ".flac", ".wav", ".ogg", ".aac", ".m4a", ".opus",
    NULL
};

static bool is_media_file(const char *path) {
    const char *ext = strrchr(path, '.');
    if (!ext) return false;
    for (int i = 0; video_exts[i]; i++) {
        if (strcasecmp(ext, video_exts[i]) == 0) return true;
    }
    return false;
}

void playlist_add(App *app, const char *path) {
    if (app->playlist.count >= MAX_PLAYLIST) return;
    PlaylistEntry *e = &app->playlist.entries[app->playlist.count];
    strncpy(e->path, path, PATH_MAX_LEN - 1);
    e->path[PATH_MAX_LEN - 1] = '\0';
    
    const char *name = strrchr(path, '/');
    strncpy(e->name, name ? name + 1 : path, 255);
    e->name[255] = '\0';
    app->playlist.count++;
}

void playlist_clear(App *app) {
    app->playlist.count = 0;
    app->playlist.current = 0;
    app->playlist_scroll = 0;
}

int playlist_next(App *app) {
    if (app->playlist.count == 0) return -1;
    app->playlist.current = (app->playlist.current + 1) % app->playlist.count;
    playback_open(app, app->playlist.entries[app->playlist.current].path);
    return 0;
}

int playlist_prev(App *app) {
    if (app->playlist.count == 0) return -1;
    app->playlist.current = (app->playlist.current - 1 + app->playlist.count) % app->playlist.count;
    playback_open(app, app->playlist.entries[app->playlist.current].path);
    return 0;
}

void playlist_remove_index(App *app, int idx) {
    if (idx < 0 || idx >= app->playlist.count) return;
    for (int i = idx; i < app->playlist.count - 1; i++) {
        app->playlist.entries[i] = app->playlist.entries[i + 1];
    }
    app->playlist.count--;
    if (app->playlist.current >= app->playlist.count) {
        app->playlist.current = app->playlist.count > 0 ? app->playlist.count - 1 : 0;
    }
}

void playlist_move_up(App *app, int idx) {
    if (idx <= 0 || idx >= app->playlist.count) return;
    PlaylistEntry tmp = app->playlist.entries[idx];
    app->playlist.entries[idx] = app->playlist.entries[idx - 1];
    app->playlist.entries[idx - 1] = tmp;
    if (app->playlist.current == idx) app->playlist.current = idx - 1;
    else if (app->playlist.current == idx - 1) app->playlist.current = idx;
}

void playlist_move_down(App *app, int idx) {
    if (idx < 0 || idx >= app->playlist.count - 1) return;
    PlaylistEntry tmp = app->playlist.entries[idx];
    app->playlist.entries[idx] = app->playlist.entries[idx + 1];
    app->playlist.entries[idx + 1] = tmp;
    if (app->playlist.current == idx) app->playlist.current = idx + 1;
    else if (app->playlist.current == idx + 1) app->playlist.current = idx;
}

/* Add all media files from a directory */
int playlist_add_directory(App *app, const char *dir_path) {
    DIR *dir = opendir(dir_path);
    if (!dir) return 0;

    int count = 0;
    struct dirent *entry;
    char full_path[PATH_MAX_LEN];

    while ((entry = readdir(dir)) != NULL && app->playlist.count < MAX_PLAYLIST) {
        if (entry->d_name[0] == '.') continue;
        snprintf(full_path, sizeof(full_path), "%s/%s", dir_path, entry->d_name);
        
        struct stat st;
        if (stat(full_path, &st) == 0) {
            if (S_ISREG(st.st_mode) && is_media_file(full_path)) {
                playlist_add(app, full_path);
                count++;
            } else if (S_ISDIR(st.st_mode)) {
                count += playlist_add_directory(app, full_path);
            }
        }
    }

    closedir(dir);
    return count;
}

/* File dialog using zenity ( GTK ) or kdialog ( KDE ) or native */
void playlist_open_file_dialog(App *app) {
    FILE *fp = NULL;
    char cmd[256] = {0};
    char path[PATH_MAX_LEN] = {0};

    /* Try zenity first ( GTK/GNOME ) */
    if (getenv("XDG_CURRENT_DESKTOP")) {
        snprintf(cmd, sizeof(cmd),
            "zenity --file-selection --title='Open Media File' "
            "--file-filter='Media files | *.mp4 *.mkv *.avi *.mov *.webm *.flv *.wmv *.mpg *.mpeg *.m4v *.mp3 *.flac *.wav *.ogg *.aac *.m4a' "
            "--file-filter='All files | *' 2>/dev/null");
        fp = popen(cmd, "r");
    }

    /* Try kdialog ( KDE ) */
    if (!fp) {
        snprintf(cmd, sizeof(cmd),
            "kdialog --getopenfilename . 'video/* audio/*' --title 'Open Media File' 2>/dev/null");
        fp = popen(cmd, "r");
    }

    /* Try system dialog as fallback */
    if (!fp) {
        snprintf(cmd, sizeof(cmd),
            "dialog --title 'Open Media File' --fselect / 0 0 2>/dev/null");
        fp = popen(cmd, "r");
    }

    if (fp) {
        if (fgets(path, sizeof(path), fp)) {
            size_t len = strlen(path);
            while (len > 0 && (path[len-1] == '\n' || path[len-1] == '\r')) {
                path[--len] = '\0';
            }
            if (len > 0) {
                struct stat st;
                if (stat(path, &st) == 0) {
                    if (S_ISDIR(st.st_mode)) {
                        int added = playlist_add_directory(app, path);
                        if (added > 0 && app->playlist.count > 0) {
                            app->playlist.current = app->playlist.count - added;
                            playback_open(app, app->playlist.entries[app->playlist.current].path);
                        }
                    } else if (S_ISREG(st.st_mode)) {
                        playlist_add(app, path);
                        app->playlist.current = app->playlist.count - 1;
                        playback_open(app, path);
                    }
                }
            }
        }
        pclose(fp);
    } else {
        fprintf(stderr, "No file dialog available. Please install zenity, kdialog, or dialog.\n");
        fprintf(stderr, "Usage: %s <media_file> [media_file2 ...]\n", "jnet-video");
    }
}
