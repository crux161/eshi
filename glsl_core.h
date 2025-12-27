/*
 * This file contains mathematical structures based on Tzozen's code
 * Copyright (c) 2025 rexim (Tsoding)
 * Licensed under the MIT License.
 */
#pragma once
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <vector>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/opt.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>
}

// --- GLSL MATH ---
struct vec4 { 
    float x, y, z, w; 
    vec4(float x=0, float y=0, float z=0, float w=0) : x(x), y(y), z(z), w(w) {} 
};

struct vec2 { 
    float x, y; 
    vec2(float x=0, float y=0) : x(x), y(y) {} 
    vec2 yx() const{return vec2(y,x);} 
    vec4 xyyx() const{return vec4(x,y,y,x);} 
};

// Operators
inline vec2 operator *(const vec2 &a, float s) { return vec2(a.x*s, a.y*s); }
inline vec2 operator +(const vec2 &a, float s) { return vec2(a.x+s, a.y+s); }
inline vec2 operator *(float s, const vec2 &a) { return a*s; }
inline vec2 operator -(const vec2 &a, const vec2 &b) { return vec2(a.x-b.x, a.y-b.y); }
inline vec2 operator +(const vec2 &a, const vec2 &b) { return vec2(a.x+b.x, a.y+b.y); }
inline vec2 operator *(const vec2 &a, const vec2 &b) { return vec2(a.x*b.x, a.y*b.y); }
inline vec2 operator /(const vec2 &a, float s) { return vec2(a.x/s, a.y/s); }
inline float dot(const vec2 &a, const vec2 &b) { return a.x*b.x + a.y*b.y; }
inline vec2 abs(const vec2 &a) { return vec2(fabsf(a.x), fabsf(a.y)); } 
inline vec2 &operator +=(vec2 &a, const vec2 &b) { a = a + b; return a; }
inline vec2 &operator +=(vec2 &a, float s) { a = a + s; return a; }
inline vec2 cos(const vec2 &a) { return vec2(cosf(a.x), cosf(a.y)); } 
inline vec4 sin(const vec4 &a) { return vec4(sinf(a.x), sinf(a.y), sinf(a.z), sinf(a.w)); } 
inline vec4 exp(const vec4 &a) { return vec4(expf(a.x), expf(a.y), expf(a.z), expf(a.w)); } 
inline vec4 tanh(const vec4 &a) { return vec4(tanhf(a.x), tanhf(a.y), tanhf(a.z), tanhf(a.w)); } 
inline vec4 operator +(const vec4 &a, float s) { return vec4(a.x+s, a.y+s, a.z+s, a.w+s); }
inline vec4 operator *(const vec4 &a, float s) { return vec4(a.x*s, a.y*s, a.z*s, a.w*s); }
inline vec4 operator *(float s, const vec4 &a) { return a*s; }
inline vec4 operator +(const vec4 &a, const vec4 &b) { return vec4(a.x+b.x, a.y+b.y, a.z+b.z, a.w+b.w); }
inline vec4 &operator +=(vec4 &a, const vec4 &b) { a = a + b; return a; }
inline vec4 operator -(float s, const vec4 &a) { return vec4(s-a.x, s-a.y, s-a.z, s-a.w); }
inline vec4 operator /(const vec4 &a, const vec4 &b) { return vec4(a.x/b.x, a.y/b.y, a.z/b.z, a.w/b.w); }
 
class SimpleEncoder {
    int width, height, fps, frame_idx;
    AVFormatContext *fmt_ctx = NULL;
    AVCodecContext *c_ctx = NULL;
    AVStream *stream = NULL;
    AVFrame *rgb_frame = NULL;
    AVFrame *yuv_frame = NULL;
    struct SwsContext *sws_ctx = NULL;

    void encode_internal(AVFrame *frame) {
        int ret = avcodec_send_frame(c_ctx, frame);
        if (ret < 0) exit(1);
        while (ret >= 0) {
            AVPacket *pkt = av_packet_alloc();
            ret = avcodec_receive_packet(c_ctx, pkt);
            if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) { av_packet_free(&pkt); return; }
            av_packet_rescale_ts(pkt, c_ctx->time_base, stream->time_base);
            pkt->stream_index = stream->index;
            av_interleaved_write_frame(fmt_ctx, pkt);
            av_packet_free(&pkt);
        }
    }

public:
    SimpleEncoder(const char* filename, int w, int h, int fps_val) 
        : width(w), height(h), fps(fps_val), frame_idx(0) {
        avformat_alloc_output_context2(&fmt_ctx, NULL, NULL, filename);
        const AVCodec *codec = avcodec_find_encoder(AV_CODEC_ID_H264);
        stream = avformat_new_stream(fmt_ctx, codec);
        c_ctx = avcodec_alloc_context3(codec);
        c_ctx->width = width; c_ctx->height = height;
        c_ctx->time_base = (AVRational){1, fps};
        c_ctx->framerate = (AVRational){fps, 1};
        c_ctx->pix_fmt = AV_PIX_FMT_YUV420P;
        if (fmt_ctx->oformat->flags & AVFMT_GLOBALHEADER) c_ctx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
        avcodec_open2(c_ctx, codec, NULL);
        avcodec_parameters_from_context(stream->codecpar, c_ctx);
        if (!(fmt_ctx->oformat->flags & AVFMT_NOFILE)) avio_open(&fmt_ctx->pb, filename, AVIO_FLAG_WRITE);
        avformat_write_header(fmt_ctx, NULL); // Ignored return value fixed by (void) if strict
        rgb_frame = av_frame_alloc(); rgb_frame->format = AV_PIX_FMT_RGB24; rgb_frame->width=w; rgb_frame->height=h; av_frame_get_buffer(rgb_frame, 32);
        yuv_frame = av_frame_alloc(); yuv_frame->format = AV_PIX_FMT_YUV420P; yuv_frame->width=w; yuv_frame->height=h; av_frame_get_buffer(yuv_frame, 32);
        sws_ctx = sws_getContext(w, h, AV_PIX_FMT_RGB24, w, h, AV_PIX_FMT_YUV420P, SWS_BILINEAR, NULL, NULL, NULL);
    }
    ~SimpleEncoder() {
        encode_internal(NULL); av_write_trailer(fmt_ctx);
        if (!(fmt_ctx->oformat->flags & AVFMT_NOFILE)) avio_closep(&fmt_ctx->pb);
        printf("\nDone.\n");
    }
    uint8_t* get_pixel_buffer(int& out_linesize) {
        av_frame_make_writable(rgb_frame);
        out_linesize = rgb_frame->linesize[0];
        return rgb_frame->data[0];
    }
    void submit_frame() {
        sws_scale(sws_ctx, (const uint8_t * const *)rgb_frame->data, rgb_frame->linesize, 0, height, yuv_frame->data, yuv_frame->linesize);
        yuv_frame->pts = frame_idx++;
        encode_internal(yuv_frame);
        printf("Frame %d\r", frame_idx); fflush(stdout);
    }
};
