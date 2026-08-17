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

// Which encoder to use.
//
// On Apple Silicon, VideoToolbox routes H.264 and HEVC through the dedicated
// media engine rather than the CPU, which is dramatically faster and leaves
// the cores free for the renderer — the CPU tier in particular is competing
// for exactly those cores.
//
// It is not, however, bit-reproducible: hardware encoders make no such
// guarantee across driver or silicon revisions, so a build that needs
// byte-identical video must pin EncoderBackend::Software. (Note that the
// software path is not reproducible today either — see the note in
// docs/larimar/ARCHITECTURE.md §7 — but it is the one that can be made so.)
enum class EncoderBackend {
    Auto,     // Hardware when present, software otherwise.
    Hardware, // Fail rather than silently fall back.
    Software
};

enum class EncoderCodec { H264, HEVC };

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

    // The VideoToolbox encoders only exist in an Apple build of FFmpeg, so
    // looking them up by name is enough — no platform #ifdef needed.
    static const AVCodec* find_hardware(EncoderCodec codec) {
        return avcodec_find_encoder_by_name(
            codec == EncoderCodec::HEVC ? "hevc_videotoolbox" : "h264_videotoolbox");
    }

    static const AVCodec* find_software(EncoderCodec codec) {
        return avcodec_find_encoder(
            codec == EncoderCodec::HEVC ? AV_CODEC_ID_HEVC : AV_CODEC_ID_H264);
    }

    void configure_context(const AVCodec* codec) {
        c_ctx = avcodec_alloc_context3(codec);
        c_ctx->width = width; c_ctx->height = height;

        c_ctx->time_base = AVRational{1, fps};
        c_ctx->framerate = AVRational{fps, 1};
        // VideoToolbox accepts yuv420p directly, so both paths share the
        // existing RGBA -> YUV420P conversion.
        c_ctx->pix_fmt = AV_PIX_FMT_YUV420P;
        // Stated rather than inferred. VideoToolbox warns when it has to guess
        // and then assumes MPEG range anyway, so saying so keeps the two
        // encoders explicitly agreed on how to interpret the same frames.
        c_ctx->color_range = AVCOL_RANGE_MPEG;

        int64_t target_bitrate = (int64_t)width * height * fps * 0.25;
        c_ctx->bit_rate = target_bitrate;
        c_ctx->rc_max_rate = target_bitrate * 1.5;
        c_ctx->rc_buffer_size = target_bitrate;

        if (fmt_ctx->oformat->flags & AVFMT_GLOBALHEADER)
            c_ctx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
    }

public:
    SimpleEncoder(const char* filename, int w, int h, int fps_val,
                  EncoderBackend backend = EncoderBackend::Auto,
                  EncoderCodec codec_kind = EncoderCodec::H264)
        : width(w), height(h), fps(fps_val), frame_idx(0) {
       
        avformat_alloc_output_context2(&fmt_ctx, NULL, NULL, filename);
        if (!fmt_ctx) { fprintf(stderr, "Could not create output context\n"); exit(1); }

        const char* codec_label = (codec_kind == EncoderCodec::HEVC) ? "HEVC" : "H.264";

        const AVCodec* codec = NULL;
        bool hardware = false;

        if (backend != EncoderBackend::Software) {
            codec = find_hardware(codec_kind);
            hardware = (codec != NULL);
            if (!codec && backend == EncoderBackend::Hardware) {
                fprintf(stderr,
                        "Error: hardware %s encoder (VideoToolbox) was requested but is "
                        "not available in this FFmpeg build.\n", codec_label);
                exit(1);
            }
        }
        if (!codec) codec = find_software(codec_kind);
        if (!codec) {
            fprintf(stderr, "Error: no %s encoder available.\n", codec_label);
            exit(1);
        }

        stream = avformat_new_stream(fmt_ctx, codec);
        configure_context(codec);

        if (avcodec_open2(c_ctx, codec, NULL) < 0) {
            // Present but unusable — VideoToolbox can refuse a session under
            // virtualization or an odd geometry. Under Auto that is a reason
            // to encode on the CPU, not to fail the render; an explicit
            // request still fails loudly.
            if (hardware && backend == EncoderBackend::Auto) {
                fprintf(stderr,
                        "Note: hardware %s encoder unavailable at runtime, using software.\n",
                        codec_label);
                avcodec_free_context(&c_ctx);
                codec = find_software(codec_kind);
                hardware = false;
                if (!codec) { fprintf(stderr, "Error: no software %s encoder.\n", codec_label); exit(1); }
                configure_context(codec);
                if (avcodec_open2(c_ctx, codec, NULL) < 0) {
                    fprintf(stderr, "Could not open codec\n"); exit(1);
                }
            } else {
                fprintf(stderr, "Could not open codec\n"); exit(1);
            }
        }

        printf("Video Encoder: %s (%s)\n", codec->name,
               hardware ? "Hardware / Apple media engine" : "Software");

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
