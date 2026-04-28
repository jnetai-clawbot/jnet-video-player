/*
 * player.c - FFmpeg playback engine for J~NET Video Player
 */

#include "main.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/time.h>
#include <libavutil/rational.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>

/* Global app reference */
extern App *g_app;

/* Decode thread synchronization */
static pthread_t dec_thread;
static pthread_mutex_t video_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t video_cond = PTHREAD_COND_INITIALIZER;
static uint8_t *g_rgb_frame = NULL;
static int g_rgb_size = 0;
static volatile int g_frame_ready = 0;
static volatile double g_frame_pts = 0;
static volatile int g_dec_run = 1;
static volatile double g_seek_to = -1;

/* Decode thread */
static void *decode_thread(void *arg) {
    (void)arg;
    App *app = NULL;
    
    while (g_dec_run) {
        pthread_mutex_lock(&video_mutex);
        app = g_app;
        while (!app || !app->fmt_ctx) {
            pthread_cond_wait(&video_cond, &video_mutex);
            app = g_app;
            if (!g_dec_run) {
                pthread_mutex_unlock(&video_mutex);
                return NULL;
            }
        }
        pthread_mutex_unlock(&video_mutex);
        
        if (!app->fmt_ctx) continue;

        /* Seek request */
        if (g_seek_to >= 0) {
            int64_t ts = (int64_t)(g_seek_to * AV_TIME_BASE);
            av_seek_frame(app->fmt_ctx, -1, ts, AVSEEK_FLAG_BACKWARD);
            avcodec_flush_buffers(app->video_dec_ctx);
            if (app->audio_dec_ctx) avcodec_flush_buffers(app->audio_dec_ctx);
            g_seek_to = -1;
            g_frame_ready = 0;
        }

        AVPacket *pkt = av_packet_alloc();
        int ret = av_read_frame(app->fmt_ctx, pkt);
        
        if (ret < 0) {
            if (ret == AVERROR_EOF) {
                pthread_mutex_lock(&video_mutex);
                app->eof_reached = true;
                app->state = STATE_ENDED;
                if (app->playlist.count > 1) {
                    int idx = (app->playlist.current + 1) % app->playlist.count;
                    app->playlist.current = idx;
                    int ok = playback_open_internal(app, app->playlist.entries[idx].path);
                    if (ok == 0) {
                        app->state = STATE_PLAYING;
                    }
                }
                pthread_mutex_unlock(&video_mutex);
            }
            av_packet_free(&pkt);
            usleep(10000);
            continue;
        }

        if (pkt->stream_index == app->video_stream_idx && app->video_dec_ctx) {
            ret = avcodec_send_packet(app->video_dec_ctx, pkt);
            if (ret >= 0) {
                AVFrame *frame = av_frame_alloc();
                ret = avcodec_receive_frame(app->video_dec_ctx, frame);
                if (ret >= 0) {
                    static struct SwsContext *sws = NULL;
                    int w = frame->width;
                    int h = frame->height;
                    
                    pthread_mutex_lock(&video_mutex);
                    if (!sws || sws_getContext(w, h, app->video_dec_ctx->pix_fmt,
                        w, h, AV_PIX_FMT_RGB24, SWS_BILINEAR, NULL, NULL, NULL) == NULL) {
                        sws = sws_getContext(w, h, app->video_dec_ctx->pix_fmt,
                            w, h, AV_PIX_FMT_RGB24, SWS_BILINEAR, NULL, NULL, NULL);
                        if (!sws) { pthread_mutex_unlock(&video_mutex); av_frame_free(&frame); continue; }
                        if (sws) sws_freeContext(sws);
                        int sz = av_image_get_buffer_size(AV_PIX_FMT_RGB24, w, h, 1);
                        g_rgb_frame = av_realloc(g_rgb_frame, sz);
                        g_rgb_size = sz;
                        sws = sws_getContext(w, h, app->video_dec_ctx->pix_fmt,
                            w, h, AV_PIX_FMT_RGB24, SWS_BILINEAR, NULL, NULL, NULL);
                    }
                    
                    if (sws && g_rgb_frame) {
                        uint8_t *dst[4] = { g_rgb_frame, NULL, NULL, NULL };
                        int dst_linesize[4] = { w * 3, 0, 0, 0 };
                        sws_scale(sws, (const uint8_t *const*)frame->data,
                            frame->linesize, 0, h, dst, dst_linesize);

                        double pts = 0;
                        if (frame->pts != AV_NOPTS_VALUE)
                            pts = frame->pts * av_q2d(app->video_dec_ctx->time_base);
                        
                        g_frame_pts = pts;
                        app->current_time = pts;
                        g_frame_ready = 1;
                    }
                    pthread_mutex_unlock(&video_mutex);
                }
                av_frame_free(&frame);
            }
        } else if (pkt->stream_index == app->audio_stream_idx && app->audio_dec_ctx) {
            /* Audio decoding - simplified */
            avcodec_send_packet(app->audio_dec_ctx, pkt);
            AVFrame *af = av_frame_alloc();
            while (avcodec_receive_frame(app->audio_dec_ctx, af) >= 0) {
                av_frame_unref(af);
            }
            av_frame_free(&af);
        }

        av_packet_unref(pkt);
        av_packet_free(&pkt);
    }
    return NULL;
}

int playback_open_internal(App *app, const char *path) {
    if (app->fmt_ctx) {
        avformat_close_input(&app->fmt_ctx);
        app->fmt_ctx = NULL;
    }
    if (app->video_dec_ctx) {
        avcodec_free_context(&app->video_dec_ctx);
        app->video_dec_ctx = NULL;
    }
    if (app->audio_dec_ctx) {
        avcodec_free_context(&app->audio_dec_ctx);
        app->audio_dec_ctx = NULL;
    }

    app->fmt_ctx = avformat_alloc_context();
    if (avformat_open_input(&app->fmt_ctx, path, NULL, NULL) < 0) {
        fprintf(stderr, "Cannot open %s\n", path);
        avformat_free_context(app->fmt_ctx);
        app->fmt_ctx = NULL;
        return -1;
    }
    avformat_find_stream_info(app->fmt_ctx, NULL);

    app->video_stream_idx = -1;
    app->audio_stream_idx = -1;

    for (unsigned i = 0; i < app->fmt_ctx->nb_streams; i++) {
        AVStream *st = app->fmt_ctx->streams[i];
        const AVCodec *dec = avcodec_find_decoder(st->codecpar->codec_id);
        if (!dec) continue;

        if (st->codecpar->codec_type == AVMEDIA_TYPE_VIDEO && app->video_stream_idx < 0) {
            app->video_dec_ctx = avcodec_alloc_context3(dec);
            avcodec_parameters_to_context(app->video_dec_ctx, st->codecpar);
            if (avcodec_open2(app->video_dec_ctx, dec, NULL) >= 0) {
                app->video_stream_idx = i;
                app->video_width = app->video_dec_ctx->width;
                app->video_height = app->video_dec_ctx->height;
                app->video_time_base = st->time_base;
            }
        }
        if (st->codecpar->codec_type == AVMEDIA_TYPE_AUDIO && app->audio_stream_idx < 0) {
            app->audio_dec_ctx = avcodec_alloc_context3(dec);
            avcodec_parameters_to_context(app->audio_dec_ctx, st->codecpar);
            if (avcodec_open2(app->audio_dec_ctx, dec, NULL) >= 0) {
                app->audio_stream_idx = i;
            }
        }
    }

    if (app->video_stream_idx < 0 && app->audio_stream_idx < 0) {
        fprintf(stderr, "No suitable streams found in %s\n", path);
        return -1;
    }

    app->duration = app->fmt_ctx->duration == AV_NOPTS_VALUE ?
                    0 : (double)app->fmt_ctx->duration / AV_TIME_BASE;
    app->current_time = 0;
    app->eof_reached = false;
    app->seek_pending = false;
    app->state = STATE_PLAYING;

    return 0;
}

int playback_open(App *app, const char *path) {
    pthread_mutex_lock(&video_mutex);
    int ret = playback_open_internal(app, path);
    pthread_mutex_unlock(&video_mutex);
    return ret;
}

void playback_close(App *app) {
    pthread_mutex_lock(&video_mutex);
    app->state = STATE_STOPPED;
    if (app->fmt_ctx) {
        avformat_close_input(&app->fmt_ctx);
        app->fmt_ctx = NULL;
    }
    if (app->video_dec_ctx) {
        avcodec_free_context(&app->video_dec_ctx);
        app->video_dec_ctx = NULL;
    }
    if (app->audio_dec_ctx) {
        avcodec_free_context(&app->audio_dec_ctx);
        app->audio_dec_ctx = NULL;
    }
    app->video_stream_idx = -1;
    app->audio_stream_idx = -1;
    if (g_rgb_frame) {
        av_free(g_rgb_frame);
        g_rgb_frame = NULL;
    }
    g_frame_ready = 0;
    pthread_mutex_unlock(&video_mutex);
}

void playback_play(App *app) {
    pthread_mutex_lock(&video_mutex);
    if (app->state == STATE_PAUSED) {
        app->state = STATE_PLAYING;
    } else if ((app->state == STATE_STOPPED || app->state == STATE_ENDED) 
               && app->playlist.count > 0) {
        if (!app->fmt_ctx) {
            playback_open_internal(app, app->playlist.entries[app->playlist.current].path);
        }
        app->state = STATE_PLAYING;
    }
    pthread_mutex_unlock(&video_mutex);
}

void playback_pause(App *app) {
    pthread_mutex_lock(&video_mutex);
    if (app->state == STATE_PLAYING) {
        app->state = STATE_PAUSED;
    }
    pthread_mutex_unlock(&video_mutex);
}

void playback_seek(App *app, double pos) {
    pthread_mutex_lock(&video_mutex);
    pos = fmax(0, fmin(pos, app->duration));
    g_seek_to = pos;
    app->current_time = pos;
    app->seek_pending = true;
    pthread_mutex_unlock(&video_mutex);
}

void playback_set_volume(App *app, double vol) {
    app->volume = fmax(0, fmin(1, vol));
    app->volume_changed = true;
}

int get_latest_frame(uint8_t **out_rgb, int *out_size, double *out_pts) {
    int ready = 0;
    pthread_mutex_lock(&video_mutex);
    if (g_frame_ready && g_rgb_frame && g_rgb_size > 0) {
        *out_rgb = av_malloc(g_rgb_size);
        memcpy(*out_rgb, g_rgb_frame, g_rgb_size);
        *out_size = g_rgb_size;
        *out_pts = g_frame_pts;
        g_frame_ready = 0;
        ready = 1;
    } else {
        *out_rgb = NULL;
        *out_size = 0;
    }
    pthread_mutex_unlock(&video_mutex);
    return ready;
}

void playback_init(App *app) {
    g_app = app;
    g_dec_run = 1;
    g_rgb_frame = NULL;
    g_frame_ready = 0;
    g_seek_to = -1;
    pthread_create(&dec_thread, NULL, decode_thread, app);
}

void playback_shutdown(void) {
    g_dec_run = 0;
    pthread_cond_broadcast(&video_cond);
    pthread_join(dec_thread, NULL);
    if (g_rgb_frame) {
        av_free(g_rgb_frame);
        g_rgb_frame = NULL;
    }
}
