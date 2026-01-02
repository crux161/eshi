#pragma once
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <vector>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/opt.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>
}

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
        if (ret < 0) { fprintf(stderr, "Error sending frame for encoding\n"); exit(1); }
        
        while (ret >= 0) {
            AVPacket *pkt = av_packet_alloc();
            ret = avcodec_receive_packet(c_ctx, pkt);
            if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) { av_packet_free(&pkt); return; }
            if (ret < 0) { fprintf(stderr, "Error during encoding\n"); exit(1); }
            
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
        if (!fmt_ctx) { fprintf(stderr, "Could not create output context\n"); exit(1); }

        
        const AVCodec *codec = avcodec_find_encoder(AV_CODEC_ID_H264);
        if (codec) printf("Video Encoder: Selected %s (Software)\n", codec->name);
        else { fprintf(stderr, "Error: H.264 encoder (libx264) not found.\n"); exit(1); }

        stream = avformat_new_stream(fmt_ctx, codec);
        c_ctx = avcodec_alloc_context3(codec);
        c_ctx->width = width; c_ctx->height = height;

        c_ctx->time_base = AVRational{1, fps};
        c_ctx->framerate = AVRational{fps, 1};
        c_ctx->pix_fmt = AV_PIX_FMT_YUV420P;
        
        int64_t target_bitrate = (int64_t)width * height * fps * 0.25; 
        c_ctx->bit_rate = target_bitrate;
        c_ctx->rc_max_rate = target_bitrate * 1.5;
        c_ctx->rc_buffer_size = target_bitrate;

        if (fmt_ctx->oformat->flags & AVFMT_GLOBALHEADER) 
            c_ctx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

        if (avcodec_open2(c_ctx, codec, NULL) < 0) { fprintf(stderr, "Could not open codec\n"); exit(1); }

        avcodec_parameters_from_context(stream->codecpar, c_ctx);

        if (!(fmt_ctx->oformat->flags & AVFMT_NOFILE)) {
            if (avio_open(&fmt_ctx->pb, filename, AVIO_FLAG_WRITE) < 0) {
                fprintf(stderr, "Could not open output file: %s\n", filename);
                exit(1);
            }
        }

        if(avformat_write_header(fmt_ctx, NULL) < 0) { fprintf(stderr, "Error occurred when opening output file\n"); exit(1); }

        
        
        rgb_frame = av_frame_alloc(); 
        rgb_frame->format = AV_PIX_FMT_RGBA; 
        rgb_frame->width=w; rgb_frame->height=h; 
        av_frame_get_buffer(rgb_frame, 32);
        
        yuv_frame = av_frame_alloc(); 
        yuv_frame->format = AV_PIX_FMT_YUV420P; 
        yuv_frame->width=w; yuv_frame->height=h; 
        av_frame_get_buffer(yuv_frame, 32);
        
        sws_ctx = sws_getContext(w, h, AV_PIX_FMT_RGBA, w, h, AV_PIX_FMT_YUV420P, SWS_BILINEAR, NULL, NULL, NULL);
    }

    ~SimpleEncoder() {
        encode_internal(NULL); 
        av_write_trailer(fmt_ctx);
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
