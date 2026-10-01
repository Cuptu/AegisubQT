#include "astracore.h"

#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/avutil.h>
#include <libavutil/error.h>
#include <libavutil/channel_layout.h>
#include <libswresample/swresample.h>
#include <libswscale/swscale.h>
#include <libavutil/imgutils.h>
#include <libavutil/mastering_display_metadata.h>
#include <libavutil/pixdesc.h>

#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32)
#include <windows.h>
#endif

typedef struct AcInterruptContext {
    ac_cancel_callback callback;
    void *opaque;
} AcInterruptContext;

static int ac_check_interrupt(void *opaque)
{
    AcInterruptContext *ctx = (AcInterruptContext *)opaque;
    if (ctx && ctx->callback) {
        return ctx->callback(ctx->opaque) ? 1 : 0;
    }
    return 0;
}

static void ac_write_error(int code, char *buffer, size_t size)
{
    if (!buffer || size == 0)
        return;
    buffer[0] = '\0';
    if (code == AVERROR_EXIT) {
        snprintf(buffer, size, "Operation cancelled");
    } else if (code < 0) {
        av_strerror(code, buffer, size);
    }
    buffer[size - 1] = '\0';
}

#if defined(_WIN32)
static FILE *ac_fopen_utf8(const char *path, const char *mode)
{
    if (!path || !mode) return NULL;
    int path_len = MultiByteToWideChar(CP_UTF8, 0, path, -1, NULL, 0);
    int mode_len = MultiByteToWideChar(CP_UTF8, 0, mode, -1, NULL, 0);
    if (path_len <= 0 || mode_len <= 0) return NULL;
    wchar_t *wpath = (wchar_t *)malloc((size_t)path_len * sizeof(wchar_t));
    wchar_t *wmode = (wchar_t *)malloc((size_t)mode_len * sizeof(wchar_t));
    if (!wpath || !wmode) {
        free(wpath);
        free(wmode);
        return NULL;
    }
    MultiByteToWideChar(CP_UTF8, 0, path, -1, wpath, path_len);
    MultiByteToWideChar(CP_UTF8, 0, mode, -1, wmode, mode_len);
    FILE *f = _wfopen(wpath, wmode);
    free(wpath);
    free(wmode);
    return f;
}
#else
static inline FILE *ac_fopen_utf8(const char *path, const char *mode)
{
    return fopen(path, mode);
}
#endif

#pragma pack(push, 1)
typedef struct WavHeader {
    char riff[4];
    uint32_t overall_size;
    char wave[4];
    char fmt_chunk_marker[4];
    uint32_t length_of_fmt;
    uint16_t format_type;
    uint16_t channels;
    uint32_t sample_rate;
    uint32_t byterate;
    uint16_t block_align;
    uint16_t bits_per_sample;
    char data_chunk_header[4];
    uint32_t data_size;
} WavHeader;
#pragma pack(pop)

typedef struct WaveformAccumulator {
    float *peaks;
    int count;
    int max_peaks;
    long samples_in_bucket;
    int samples_per_peak;
    float bucket_max;
} WaveformAccumulator;

static inline void accum_add_sample(WaveformAccumulator *acc, int16_t s)
{
    float abs_val = fabsf((float)s / 32768.0f);
    if (abs_val > acc->bucket_max)
        acc->bucket_max = abs_val;
    acc->samples_in_bucket++;
    if (acc->samples_in_bucket >= acc->samples_per_peak) {
        if (acc->count < acc->max_peaks) {
            acc->peaks[acc->count++] = acc->bucket_max;
        } else {
            int compacted = 0;
            for (int i = 0; i < acc->count; i += 2) {
                float p1 = acc->peaks[i];
                float p2 = (i + 1 < acc->count) ? acc->peaks[i + 1] : p1;
                acc->peaks[compacted++] = (p1 > p2) ? p1 : p2;
            }
            acc->count = compacted;
            acc->samples_per_peak *= 2;
            if (acc->count < acc->max_peaks) {
                acc->peaks[acc->count++] = acc->bucket_max;
            }
        }
        acc->samples_in_bucket = 0;
        acc->bucket_max = 0.0f;
    }
}

uint32_t ac_abi_version(void)
{
    return AC_VERSION_MAJOR;
}

uint32_t ac_version(void)
{
    return AC_BUILD_VERSION;
}

const char *ac_version_string(void)
{
    return "5.1.0";
}

int ac_has_feature(const char *feature_name)
{
    if (!feature_name)
        return -1;
    if (strcmp(feature_name, "keyframes") == 0)
        return 1;
    if (strcmp(feature_name, "frame_grabber") == 0)
        return 1;
    if (strcmp(feature_name, "timecodes") == 0)
        return 1;
    if (strcmp(feature_name, "spectrogram") == 0)
        return 1;
    if (strcmp(feature_name, "hdr_prober") == 0)
        return 1;
    if (strcmp(feature_name, "waveform") == 0)
        return 1;
    if (strcmp(feature_name, "audio_tempo") == 0)
        return 1;
    if (strcmp(feature_name, "lossless_trim") == 0)
        return 1;
    if (strcmp(feature_name, "video_session") == 0)
        return 1;
    if (strcmp(feature_name, "swscale") == 0)
        return 1;
    return 0;
}

int ac_probe_cancel_utf8(
    const char *path,
    ac_media_info *result,
    ac_cancel_callback cancel_cb,
    void *cancel_opaque,
    char *error_buffer,
    size_t error_buffer_size)
{
    AVFormatContext *format = NULL;
    int code;

    if (!path || !result || result->struct_size < sizeof(*result)) {
        code = AVERROR(EINVAL);
        ac_write_error(code, error_buffer, error_buffer_size);
        return code;
    }

    memset((char *) result + sizeof(result->struct_size), 0, sizeof(*result) - sizeof(result->struct_size));

    AcInterruptContext int_ctx = { cancel_cb, cancel_opaque };
    if (cancel_cb) {
        format = avformat_alloc_context();
        if (!format) {
            code = AVERROR(ENOMEM);
            goto done;
        }
        format->interrupt_callback.callback = ac_check_interrupt;
        format->interrupt_callback.opaque = &int_ctx;
    }

    code = avformat_open_input(&format, path, NULL, NULL);
    if (code < 0)
        goto done;
    code = avformat_find_stream_info(format, NULL);
    if (code < 0)
        goto done;

    if (format->duration != AV_NOPTS_VALUE)
        result->duration_seconds = (double) format->duration / AV_TIME_BASE;
    result->bit_rate = format->bit_rate;

    for (unsigned int index = 0; index < format->nb_streams; ++index) {
        AVStream *stream = format->streams[index];
        AVCodecParameters *parameters = stream->codecpar;
        if (parameters->codec_type == AVMEDIA_TYPE_AUDIO)
            result->has_audio = 1;
        if (parameters->codec_type != AVMEDIA_TYPE_VIDEO)
            continue;

        result->has_video = 1;
        if (result->width != 0 || result->height != 0)
            continue;
        result->width = parameters->width;
        result->height = parameters->height;
        AVRational rate = av_guess_frame_rate(format, stream, NULL);
        if (rate.num > 0 && rate.den > 0)
            result->frame_rate = av_q2d(rate);
    }

    code = 0;

done:
    ac_write_error(code, error_buffer, error_buffer_size);
    if (format) avformat_close_input(&format);
    return code;
}

int ac_probe_utf8(const char *path, ac_media_info *result, char *error_buffer, size_t error_buffer_size)
{
    return ac_probe_cancel_utf8(path, result, NULL, NULL, error_buffer, error_buffer_size);
}

int ac_check_encoder(const char *encoder_name)
{
    if (!encoder_name || !*encoder_name)
        return 0;

    const AVCodec *codec = avcodec_find_encoder_by_name(encoder_name);
    if (!codec)
        return 0;

    AVCodecContext *ctx = avcodec_alloc_context3(codec);
    if (!ctx)
        return 0;

    if (codec->type == AVMEDIA_TYPE_VIDEO) {
        ctx->width = 256;
        ctx->height = 256;
        ctx->time_base = (AVRational){1, 30};
        ctx->framerate = (AVRational){30, 1};
        ctx->pix_fmt = AV_PIX_FMT_YUV420P;
#if defined(ASTRA_HAVE_SUPPORTED_CONFIG)
        const void *out_configs = NULL;
        if (avcodec_get_supported_config(NULL, codec, AV_CODEC_CONFIG_PIX_FORMAT, 0, &out_configs, NULL) >= 0 && out_configs) {
            const enum AVPixelFormat *fmts = (const enum AVPixelFormat *)out_configs;
            if (fmts[0] != AV_PIX_FMT_NONE) {
                ctx->pix_fmt = fmts[0];
            }
        }
#else
        if (codec->pix_fmts && codec->pix_fmts[0] != AV_PIX_FMT_NONE) {
            ctx->pix_fmt = codec->pix_fmts[0];
        }
#endif
    } else if (codec->type == AVMEDIA_TYPE_AUDIO) {
        ctx->sample_rate = 44100;
        AVChannelLayout layout = AV_CHANNEL_LAYOUT_STEREO;
        av_channel_layout_copy(&ctx->ch_layout, &layout);
        ctx->sample_fmt = AV_SAMPLE_FMT_S16;
#if defined(ASTRA_HAVE_SUPPORTED_CONFIG)
        const void *out_configs = NULL;
        if (avcodec_get_supported_config(NULL, codec, AV_CODEC_CONFIG_SAMPLE_FORMAT, 0, &out_configs, NULL) >= 0 && out_configs) {
            const enum AVSampleFormat *fmts = (const enum AVSampleFormat *)out_configs;
            if (fmts[0] != AV_SAMPLE_FMT_NONE) {
                ctx->sample_fmt = fmts[0];
            }
        }
#else
        if (codec->sample_fmts && codec->sample_fmts[0] != AV_SAMPLE_FMT_NONE) {
            ctx->sample_fmt = codec->sample_fmts[0];
        }
#endif
    }

    int ret = avcodec_open2(ctx, codec, NULL);
    avcodec_free_context(&ctx);
    return (ret >= 0) ? 1 : 0;
}

int ac_extract_waveform_peaks_cancel_utf8(
    const char *path,
    int target_sample_rate,
    int samples_per_peak,
    float *out_peaks,
    int max_peaks,
    double *out_duration,
    ac_cancel_callback cancel_cb,
    void *cancel_opaque,
    char *error_buffer,
    size_t error_buffer_size)
{
    AVFormatContext *format_ctx = NULL;
    AVCodecContext *codec_ctx = NULL;
    SwrContext *swr_ctx = NULL;
    AVPacket *packet = NULL;
    AVFrame *frame = NULL;
    int16_t *resample_buf = NULL;
    int max_out_samples = 8192;
    int code = 0;
    int peaks_count = 0;

    if (out_duration)
        *out_duration = 0.0;

    if (!path || !out_peaks || max_peaks < 2 || target_sample_rate <= 0) {
        code = AVERROR(EINVAL);
        ac_write_error(code, error_buffer, error_buffer_size);
        return code;
    }
    if (samples_per_peak <= 0)
        samples_per_peak = 1;

    AcInterruptContext int_ctx = { cancel_cb, cancel_opaque };
    if (cancel_cb) {
        format_ctx = avformat_alloc_context();
        if (!format_ctx) {
            code = AVERROR(ENOMEM);
            goto cleanup;
        }
        format_ctx->interrupt_callback.callback = ac_check_interrupt;
        format_ctx->interrupt_callback.opaque = &int_ctx;
    }

    code = avformat_open_input(&format_ctx, path, NULL, NULL);
    if (code < 0)
        goto cleanup;

    code = avformat_find_stream_info(format_ctx, NULL);
    if (code < 0)
        goto cleanup;

    int audio_idx = av_find_best_stream(format_ctx, AVMEDIA_TYPE_AUDIO, -1, -1, NULL, 0);
    if (audio_idx < 0) {
        code = audio_idx;
        goto cleanup;
    }

    AVStream *audio_stream = format_ctx->streams[audio_idx];
    double duration = 0.0;
    if (audio_stream->duration != AV_NOPTS_VALUE && audio_stream->time_base.den > 0)
        duration = (double)audio_stream->duration * av_q2d(audio_stream->time_base);
    else if (format_ctx->duration != AV_NOPTS_VALUE)
        duration = (double)format_ctx->duration / AV_TIME_BASE;

    if (out_duration)
        *out_duration = duration;

    const AVCodec *decoder = avcodec_find_decoder(audio_stream->codecpar->codec_id);
    if (!decoder) {
        code = AVERROR_DECODER_NOT_FOUND;
        goto cleanup;
    }

    codec_ctx = avcodec_alloc_context3(decoder);
    if (!codec_ctx) {
        code = AVERROR(ENOMEM);
        goto cleanup;
    }

    code = avcodec_parameters_to_context(codec_ctx, audio_stream->codecpar);
    if (code < 0)
        goto cleanup;

    code = avcodec_open2(codec_ctx, decoder, NULL);
    if (code < 0)
        goto cleanup;

    AVChannelLayout out_layout = AV_CHANNEL_LAYOUT_MONO;
    code = swr_alloc_set_opts2(
        &swr_ctx,
        &out_layout,
        AV_SAMPLE_FMT_S16,
        target_sample_rate,
        &codec_ctx->ch_layout,
        codec_ctx->sample_fmt,
        codec_ctx->sample_rate,
        0, NULL);
    if (code < 0 || !swr_ctx) {
        if (code >= 0) code = AVERROR(ENOMEM);
        goto cleanup;
    }

    code = swr_init(swr_ctx);
    if (code < 0)
        goto cleanup;

    packet = av_packet_alloc();
    frame = av_frame_alloc();
    resample_buf = (int16_t *)av_malloc((size_t)max_out_samples * sizeof(int16_t));
    if (!packet || !frame || !resample_buf) {
        code = AVERROR(ENOMEM);
        goto cleanup;
    }

    WaveformAccumulator acc = {
        .peaks = out_peaks,
        .count = 0,
        .max_peaks = max_peaks,
        .samples_in_bucket = 0,
        .samples_per_peak = samples_per_peak,
        .bucket_max = 0.0f
    };

    while (av_read_frame(format_ctx, packet) >= 0) {
        if (cancel_cb && cancel_cb(cancel_opaque)) {
            code = AVERROR_EXIT;
            av_packet_unref(packet);
            goto cleanup;
        }
        if (packet->stream_index == audio_idx) {
            int send_ret = avcodec_send_packet(codec_ctx, packet);
            if (send_ret == 0) {
                while (1) {
                    int r = avcodec_receive_frame(codec_ctx, frame);
                    if (r == AVERROR(EAGAIN) || r == AVERROR_EOF)
                        break;
                    if (r < 0) {
                        code = r;
                        av_packet_unref(packet);
                        goto cleanup;
                    }
                    if (cancel_cb && cancel_cb(cancel_opaque)) {
                        code = AVERROR_EXIT;
                        av_packet_unref(packet);
                        goto cleanup;
                    }
                    int delay = (int)swr_get_delay(swr_ctx, codec_ctx->sample_rate);
                    int out_samples = (int)av_rescale_rnd(delay + frame->nb_samples, target_sample_rate, codec_ctx->sample_rate, AV_ROUND_UP);
                    if (out_samples > max_out_samples) {
                        int new_max = out_samples * 2;
                        int16_t *new_buf = (int16_t *)av_realloc(resample_buf, (size_t)new_max * sizeof(int16_t));
                        if (!new_buf) {
                            code = AVERROR(ENOMEM);
                            av_packet_unref(packet);
                            goto cleanup;
                        }
                        resample_buf = new_buf;
                        max_out_samples = new_max;
                    }
                    int converted = swr_convert(swr_ctx, (uint8_t **)&resample_buf, max_out_samples,
                                                (const uint8_t **)frame->data, frame->nb_samples);
                    if (converted < 0) {
                        code = converted;
                        av_packet_unref(packet);
                        goto cleanup;
                    }
                    for (int s = 0; s < converted; s++) {
                        accum_add_sample(&acc, resample_buf[s]);
                    }
                }
            }
        }
        av_packet_unref(packet);
    }

    // Flush decoder
    avcodec_send_packet(codec_ctx, NULL);
    while (1) {
        int r = avcodec_receive_frame(codec_ctx, frame);
        if (r == AVERROR_EOF || r == AVERROR(EAGAIN))
            break;
        if (r < 0) {
            code = r;
            goto cleanup;
        }
        if (cancel_cb && cancel_cb(cancel_opaque)) {
            code = AVERROR_EXIT;
            goto cleanup;
        }
        int delay = (int)swr_get_delay(swr_ctx, codec_ctx->sample_rate);
        int out_samples = (int)av_rescale_rnd(delay + frame->nb_samples, target_sample_rate, codec_ctx->sample_rate, AV_ROUND_UP);
        if (out_samples > max_out_samples) {
            int new_max = out_samples * 2;
            int16_t *new_buf = (int16_t *)av_realloc(resample_buf, (size_t)new_max * sizeof(int16_t));
            if (!new_buf) {
                code = AVERROR(ENOMEM);
                goto cleanup;
            }
            resample_buf = new_buf;
            max_out_samples = new_max;
        }
        int converted = swr_convert(swr_ctx, (uint8_t **)&resample_buf, max_out_samples,
                                    (const uint8_t **)frame->data, frame->nb_samples);
        if (converted < 0) {
            code = converted;
            goto cleanup;
        }
        for (int s = 0; s < converted; s++) {
            accum_add_sample(&acc, resample_buf[s]);
        }
    }

    // Flush swresample
    while (1) {
        int converted = swr_convert(swr_ctx, (uint8_t **)&resample_buf, max_out_samples, NULL, 0);
        if (converted <= 0) {
            if (converted < 0) code = converted;
            break;
        }
        for (int s = 0; s < converted; s++) {
            accum_add_sample(&acc, resample_buf[s]);
        }
    }
    if (code < 0) goto cleanup;

    if (acc.samples_in_bucket > 0 && acc.count < acc.max_peaks) {
        acc.peaks[acc.count++] = acc.bucket_max;
    }
    peaks_count = acc.count;
    code = 0;

cleanup:
    if (packet) av_packet_free(&packet);
    if (frame) av_frame_free(&frame);
    if (resample_buf) av_freep(&resample_buf);
    if (swr_ctx) swr_free(&swr_ctx);
    if (codec_ctx) avcodec_free_context(&codec_ctx);
    if (format_ctx) avformat_close_input(&format_ctx);

    ac_write_error(code, error_buffer, error_buffer_size);
    return (code < 0) ? code : peaks_count;
}

int ac_extract_waveform_peaks_utf8(
    const char *path,
    int target_sample_rate,
    int samples_per_peak,
    float *out_peaks,
    int max_peaks,
    double *out_duration,
    char *error_buffer,
    size_t error_buffer_size)
{
    return ac_extract_waveform_peaks_cancel_utf8(
        path, target_sample_rate, samples_per_peak, out_peaks, max_peaks,
        out_duration, NULL, NULL, error_buffer, error_buffer_size);
}

int ac_extract_audio_wav_cancel_utf8(
    const char *input_path,
    const char *output_wav_path,
    int target_sample_rate,
    int channels,
    ac_cancel_callback cancel_cb,
    void *cancel_opaque,
    char *error_buffer,
    size_t error_buffer_size)
{
    AVFormatContext *format_ctx = NULL;
    AVCodecContext *codec_ctx = NULL;
    SwrContext *swr_ctx = NULL;
    AVPacket *packet = NULL;
    AVFrame *frame = NULL;
    int16_t *resample_buf = NULL;
    int max_out_samples = 8192;
    FILE *out_file = NULL;
    int code = 0;
    uint32_t total_pcm_bytes = 0;

    if (!input_path || !output_wav_path) {
        code = AVERROR(EINVAL);
        ac_write_error(code, error_buffer, error_buffer_size);
        return code;
    }
    if (target_sample_rate <= 0) target_sample_rate = 16000;
    if (channels <= 0 || channels > 2) channels = 1;

    AcInterruptContext int_ctx = { cancel_cb, cancel_opaque };
    if (cancel_cb) {
        format_ctx = avformat_alloc_context();
        if (!format_ctx) {
            code = AVERROR(ENOMEM);
            goto cleanup;
        }
        format_ctx->interrupt_callback.callback = ac_check_interrupt;
        format_ctx->interrupt_callback.opaque = &int_ctx;
    }

    code = avformat_open_input(&format_ctx, input_path, NULL, NULL);
    if (code < 0) goto cleanup;

    code = avformat_find_stream_info(format_ctx, NULL);
    if (code < 0) goto cleanup;

    int audio_idx = av_find_best_stream(format_ctx, AVMEDIA_TYPE_AUDIO, -1, -1, NULL, 0);
    if (audio_idx < 0) {
        code = audio_idx;
        goto cleanup;
    }

    AVStream *audio_stream = format_ctx->streams[audio_idx];
    const AVCodec *decoder = avcodec_find_decoder(audio_stream->codecpar->codec_id);
    if (!decoder) {
        code = AVERROR_DECODER_NOT_FOUND;
        goto cleanup;
    }

    codec_ctx = avcodec_alloc_context3(decoder);
    if (!codec_ctx) {
        code = AVERROR(ENOMEM);
        goto cleanup;
    }

    code = avcodec_parameters_to_context(codec_ctx, audio_stream->codecpar);
    if (code < 0) goto cleanup;

    code = avcodec_open2(codec_ctx, decoder, NULL);
    if (code < 0) goto cleanup;

    AVChannelLayout out_layout;
    if (channels == 1) {
        out_layout = (AVChannelLayout)AV_CHANNEL_LAYOUT_MONO;
    } else {
        out_layout = (AVChannelLayout)AV_CHANNEL_LAYOUT_STEREO;
    }
    code = swr_alloc_set_opts2(
        &swr_ctx,
        &out_layout,
        AV_SAMPLE_FMT_S16,
        target_sample_rate,
        &codec_ctx->ch_layout,
        codec_ctx->sample_fmt,
        codec_ctx->sample_rate,
        0, NULL);
    if (code < 0 || !swr_ctx) {
        if (code >= 0) code = AVERROR(ENOMEM);
        goto cleanup;
    }

    code = swr_init(swr_ctx);
    if (code < 0) goto cleanup;

    out_file = ac_fopen_utf8(output_wav_path, "wb");
    if (!out_file) {
        code = AVERROR(errno ? errno : EIO);
        goto cleanup;
    }

    WavHeader header;
    memcpy(header.riff, "RIFF", 4);
    header.overall_size = 0;
    memcpy(header.wave, "WAVE", 4);
    memcpy(header.fmt_chunk_marker, "fmt ", 4);
    header.length_of_fmt = 16;
    header.format_type = 1; // PCM
    header.channels = (uint16_t)channels;
    header.sample_rate = (uint32_t)target_sample_rate;
    header.byterate = (uint32_t)(target_sample_rate * channels * sizeof(int16_t));
    header.block_align = (uint16_t)(channels * sizeof(int16_t));
    header.bits_per_sample = 16;
    memcpy(header.data_chunk_header, "data", 4);
    header.data_size = 0;

    if (fwrite(&header, sizeof(header), 1, out_file) != 1) {
        code = AVERROR(EIO);
        goto cleanup;
    }

    packet = av_packet_alloc();
    frame = av_frame_alloc();
    resample_buf = (int16_t *)av_malloc((size_t)(max_out_samples * channels) * sizeof(int16_t));
    if (!packet || !frame || !resample_buf) {
        code = AVERROR(ENOMEM);
        goto cleanup;
    }

    while (av_read_frame(format_ctx, packet) >= 0) {
        if (cancel_cb && cancel_cb(cancel_opaque)) {
            code = AVERROR_EXIT;
            av_packet_unref(packet);
            goto cleanup;
        }
        if (packet->stream_index == audio_idx) {
            int send_ret = avcodec_send_packet(codec_ctx, packet);
            if (send_ret == 0) {
                while (1) {
                    int r = avcodec_receive_frame(codec_ctx, frame);
                    if (r == AVERROR(EAGAIN) || r == AVERROR_EOF)
                        break;
                    if (r < 0) {
                        code = r;
                        av_packet_unref(packet);
                        goto cleanup;
                    }
                    if (cancel_cb && cancel_cb(cancel_opaque)) {
                        code = AVERROR_EXIT;
                        av_packet_unref(packet);
                        goto cleanup;
                    }
                    int delay = (int)swr_get_delay(swr_ctx, codec_ctx->sample_rate);
                    int out_samples = (int)av_rescale_rnd(delay + frame->nb_samples, target_sample_rate, codec_ctx->sample_rate, AV_ROUND_UP);
                    if (out_samples > max_out_samples) {
                        int new_max = out_samples * 2;
                        int16_t *new_buf = (int16_t *)av_realloc(resample_buf, (size_t)(new_max * channels) * sizeof(int16_t));
                        if (!new_buf) {
                            code = AVERROR(ENOMEM);
                            av_packet_unref(packet);
                            goto cleanup;
                        }
                        resample_buf = new_buf;
                        max_out_samples = new_max;
                    }
                    int converted = swr_convert(swr_ctx, (uint8_t **)&resample_buf, max_out_samples,
                                                (const uint8_t **)frame->data, frame->nb_samples);
                    if (converted < 0) {
                        code = converted;
                        av_packet_unref(packet);
                        goto cleanup;
                    }
                    if (converted > 0) {
                        size_t bytes = (size_t)converted * (size_t)channels * sizeof(int16_t);
                        if (fwrite(resample_buf, 1, bytes, out_file) != bytes) {
                            code = AVERROR(EIO);
                            av_packet_unref(packet);
                            goto cleanup;
                        }
                        total_pcm_bytes += (uint32_t)bytes;
                    }
                }
            }
        }
        av_packet_unref(packet);
    }

    // Flush decoder
    avcodec_send_packet(codec_ctx, NULL);
    while (1) {
        int r = avcodec_receive_frame(codec_ctx, frame);
        if (r == AVERROR_EOF || r == AVERROR(EAGAIN))
            break;
        if (r < 0) {
            code = r;
            goto cleanup;
        }
        if (cancel_cb && cancel_cb(cancel_opaque)) {
            code = AVERROR_EXIT;
            goto cleanup;
        }
        int delay = (int)swr_get_delay(swr_ctx, codec_ctx->sample_rate);
        int out_samples = (int)av_rescale_rnd(delay + frame->nb_samples, target_sample_rate, codec_ctx->sample_rate, AV_ROUND_UP);
        if (out_samples > max_out_samples) {
            int new_max = out_samples * 2;
            int16_t *new_buf = (int16_t *)av_realloc(resample_buf, (size_t)(new_max * channels) * sizeof(int16_t));
            if (!new_buf) {
                code = AVERROR(ENOMEM);
                goto cleanup;
            }
            resample_buf = new_buf;
            max_out_samples = new_max;
        }
        int converted = swr_convert(swr_ctx, (uint8_t **)&resample_buf, max_out_samples,
                                    (const uint8_t **)frame->data, frame->nb_samples);
        if (converted < 0) {
            code = converted;
            goto cleanup;
        }
        if (converted > 0) {
            size_t bytes = (size_t)converted * (size_t)channels * sizeof(int16_t);
            if (fwrite(resample_buf, 1, bytes, out_file) != bytes) {
                code = AVERROR(EIO);
                goto cleanup;
            }
            total_pcm_bytes += (uint32_t)bytes;
        }
    }

    // Flush swr
    while (1) {
        int converted = swr_convert(swr_ctx, (uint8_t **)&resample_buf, max_out_samples, NULL, 0);
        if (converted <= 0) {
            if (converted < 0) code = converted;
            break;
        }
        size_t bytes = (size_t)converted * (size_t)channels * sizeof(int16_t);
        if (fwrite(resample_buf, 1, bytes, out_file) != bytes) {
            code = AVERROR(EIO);
            goto cleanup;
        }
        total_pcm_bytes += (uint32_t)bytes;
    }
    if (code < 0) goto cleanup;

    // Update header sizes
    header.overall_size = total_pcm_bytes + 36;
    header.data_size = total_pcm_bytes;
    if (fseek(out_file, 0, SEEK_SET) == 0) {
        fwrite(&header, sizeof(header), 1, out_file);
    }
    fflush(out_file);
    code = 0;

cleanup:
    if (out_file) fclose(out_file);
    if (packet) av_packet_free(&packet);
    if (frame) av_frame_free(&frame);
    if (resample_buf) av_freep(&resample_buf);
    if (swr_ctx) swr_free(&swr_ctx);
    if (codec_ctx) avcodec_free_context(&codec_ctx);
    if (format_ctx) avformat_close_input(&format_ctx);

    ac_write_error(code, error_buffer, error_buffer_size);
    return code;
}

int ac_extract_audio_wav_utf8(
    const char *input_path,
    const char *output_wav_path,
    int target_sample_rate,
    int channels,
    char *error_buffer,
    size_t error_buffer_size)
{
    return ac_extract_audio_wav_cancel_utf8(
        input_path, output_wav_path, target_sample_rate, channels,
        NULL, NULL, error_buffer, error_buffer_size);
}

int ac_trim_media_cancel_utf8(
    const char *input_path,
    const char *output_path,
    double start_seconds,
    double duration_seconds,
    int stream_copy,
    ac_cancel_callback cancel_cb,
    void *cancel_opaque,
    char *error_buffer,
    size_t error_buffer_size)
{
    if (!input_path || !*input_path || !output_path || !*output_path) {
        ac_write_error(AVERROR(EINVAL), error_buffer, error_buffer_size);
        return AVERROR(EINVAL);
    }
    if (start_seconds < 0.0) start_seconds = 0.0;

    int code = 0;
    AVFormatContext *in_fmt = NULL;
    AVFormatContext *out_fmt = NULL;
    AVPacket *pkt = NULL;
    int64_t *stream_start_pts = NULL;
    int *stream_mapping = NULL;
    int stream_mapping_size = 0;
    int *seen_first_pts = NULL;

    AcInterruptContext ic = { cancel_cb, cancel_opaque };
    AVIOInterruptCB interrupt_cb = { ac_check_interrupt, &ic };

    in_fmt = avformat_alloc_context();
    if (!in_fmt) {
        code = AVERROR(ENOMEM);
        goto cleanup;
    }
    in_fmt->interrupt_callback = interrupt_cb;

    code = avformat_open_input(&in_fmt, input_path, NULL, NULL);
    if (code < 0) goto cleanup;

    code = avformat_find_stream_info(in_fmt, NULL);
    if (code < 0) goto cleanup;

    code = avformat_alloc_output_context2(&out_fmt, NULL, NULL, output_path);
    if (code < 0 || !out_fmt) {
        code = (code < 0) ? code : AVERROR(ENOMEM);
        goto cleanup;
    }
    out_fmt->interrupt_callback = interrupt_cb;

    stream_mapping_size = (int)in_fmt->nb_streams;
    stream_mapping = (int *)av_calloc(stream_mapping_size, sizeof(int));
    stream_start_pts = (int64_t *)av_calloc(stream_mapping_size, sizeof(int64_t));
    seen_first_pts = (int *)av_calloc(stream_mapping_size, sizeof(int));
    if (!stream_mapping || !stream_start_pts || !seen_first_pts) {
        code = AVERROR(ENOMEM);
        goto cleanup;
    }

    int stream_idx = 0;
    for (int i = 0; i < stream_mapping_size; i++) {
        AVStream *in_stream = in_fmt->streams[i];
        AVCodecParameters *codecpar = in_stream->codecpar;
        if (codecpar->codec_type != AVMEDIA_TYPE_AUDIO &&
            codecpar->codec_type != AVMEDIA_TYPE_VIDEO &&
            codecpar->codec_type != AVMEDIA_TYPE_SUBTITLE) {
            stream_mapping[i] = -1;
            continue;
        }

        stream_mapping[i] = stream_idx++;
        AVStream *out_stream = avformat_new_stream(out_fmt, NULL);
        if (!out_stream) {
            code = AVERROR(ENOMEM);
            goto cleanup;
        }

        code = avcodec_parameters_copy(out_stream->codecpar, in_stream->codecpar);
        if (code < 0) goto cleanup;
        out_stream->codecpar->codec_tag = 0;
    }

    if (!(out_fmt->oformat->flags & AVFMT_NOFILE)) {
        code = avio_open2(&out_fmt->pb, output_path, AVIO_FLAG_WRITE, &interrupt_cb, NULL);
        if (code < 0) goto cleanup;
    }

    code = avformat_write_header(out_fmt, NULL);
    if (code < 0) goto cleanup;

    int64_t seek_target = (int64_t)(start_seconds * AV_TIME_BASE);
    if (start_seconds > 0.0) {
        avformat_seek_file(in_fmt, -1, INT64_MIN, seek_target, seek_target, 0);
    }

    pkt = av_packet_alloc();
    if (!pkt) {
        code = AVERROR(ENOMEM);
        goto cleanup;
    }

    int64_t end_time_us = (duration_seconds > 0.0) ? (int64_t)((start_seconds + duration_seconds) * AV_TIME_BASE) : -1;

    while (1) {
        if (cancel_cb && cancel_cb(cancel_opaque)) {
            code = AVERROR_EXIT;
            break;
        }

        code = av_read_frame(in_fmt, pkt);
        if (code < 0) {
            if (code == AVERROR_EOF) code = 0;
            break;
        }

        if (pkt->stream_index >= stream_mapping_size || stream_mapping[pkt->stream_index] < 0) {
            av_packet_unref(pkt);
            continue;
        }

        AVStream *in_stream = in_fmt->streams[pkt->stream_index];
        AVStream *out_stream = out_fmt->streams[stream_mapping[pkt->stream_index]];

        int64_t pkt_time_us = av_rescale_q(pkt->pts != AV_NOPTS_VALUE ? pkt->pts : pkt->dts,
                                           in_stream->time_base, (AVRational){1, AV_TIME_BASE});

        if (pkt_time_us < (int64_t)(start_seconds * AV_TIME_BASE)) {
            av_packet_unref(pkt);
            continue;
        }

        if (end_time_us > 0 && pkt_time_us > end_time_us) {
            av_packet_unref(pkt);
            break;
        }

        if (!seen_first_pts[pkt->stream_index]) {
            seen_first_pts[pkt->stream_index] = 1;
            stream_start_pts[pkt->stream_index] = pkt->pts != AV_NOPTS_VALUE ? pkt->pts : pkt->dts;
        }

        int64_t base_pts = stream_start_pts[pkt->stream_index];
        if (pkt->pts != AV_NOPTS_VALUE && pkt->pts >= base_pts) {
            pkt->pts = av_rescale_q_rnd(pkt->pts - base_pts, in_stream->time_base, out_stream->time_base, (enum AVRounding)(AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));
        } else {
            pkt->pts = AV_NOPTS_VALUE;
        }

        if (pkt->dts != AV_NOPTS_VALUE && pkt->dts >= base_pts) {
            pkt->dts = av_rescale_q_rnd(pkt->dts - base_pts, in_stream->time_base, out_stream->time_base, (enum AVRounding)(AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));
        } else {
            pkt->dts = pkt->pts;
        }

        if (pkt->pts != AV_NOPTS_VALUE && pkt->dts != AV_NOPTS_VALUE && pkt->dts > pkt->pts) {
            pkt->dts = pkt->pts;
        }

        pkt->duration = av_rescale_q(pkt->duration, in_stream->time_base, out_stream->time_base);
        pkt->pos = -1;
        pkt->stream_index = stream_mapping[pkt->stream_index];

        code = av_interleaved_write_frame(out_fmt, pkt);
        av_packet_unref(pkt);
        if (code < 0) break;
    }

    if (code >= 0) {
        av_write_trailer(out_fmt);
    }

cleanup:
    if (pkt) av_packet_free(&pkt);
    if (stream_mapping) av_free(stream_mapping);
    if (stream_start_pts) av_free(stream_start_pts);
    if (seen_first_pts) av_free(seen_first_pts);
    if (out_fmt) {
        if (!(out_fmt->oformat->flags & AVFMT_NOFILE) && out_fmt->pb) {
            avio_closep(&out_fmt->pb);
        }
        avformat_free_context(out_fmt);
    }
    if (in_fmt) avformat_close_input(&in_fmt);

    ac_write_error(code, error_buffer, error_buffer_size);
    return code;
}

int ac_trim_media_utf8(
    const char *input_path,
    const char *output_path,
    double start_seconds,
    double duration_seconds,
    int stream_copy,
    char *error_buffer,
    size_t error_buffer_size)
{
    return ac_trim_media_cancel_utf8(
        input_path, output_path, start_seconds, duration_seconds, stream_copy,
        NULL, NULL, error_buffer, error_buffer_size);
}

int ac_extract_audio_stream_utf8(
    const char *input_media,
    const char *output_audio,
    int stream_index,
    char *error_buffer,
    size_t error_buffer_size)
{
    if (!input_media || !*input_media || !output_audio || !*output_audio) {
        ac_write_error(AVERROR(EINVAL), error_buffer, error_buffer_size);
        return AVERROR(EINVAL);
    }

    int code = 0;
    AVFormatContext *in_fmt = NULL;
    AVFormatContext *out_fmt = NULL;
    AVPacket *pkt = NULL;
    int target_stream_idx = -1;

    code = avformat_open_input(&in_fmt, input_media, NULL, NULL);
    if (code < 0) goto cleanup;

    code = avformat_find_stream_info(in_fmt, NULL);
    if (code < 0) goto cleanup;

    if (stream_index >= 0 && stream_index < (int)in_fmt->nb_streams &&
        in_fmt->streams[stream_index]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO) {
        target_stream_idx = stream_index;
    } else {
        target_stream_idx = av_find_best_stream(in_fmt, AVMEDIA_TYPE_AUDIO, -1, -1, NULL, 0);
    }

    if (target_stream_idx < 0) {
        code = AVERROR_STREAM_NOT_FOUND;
        goto cleanup;
    }

    AVStream *in_stream = in_fmt->streams[target_stream_idx];

    code = avformat_alloc_output_context2(&out_fmt, NULL, NULL, output_audio);
    if (code < 0 || !out_fmt) {
        code = (code < 0) ? code : AVERROR(ENOMEM);
        goto cleanup;
    }

    AVStream *out_stream = avformat_new_stream(out_fmt, NULL);
    if (!out_stream) {
        code = AVERROR(ENOMEM);
        goto cleanup;
    }

    code = avcodec_parameters_copy(out_stream->codecpar, in_stream->codecpar);
    if (code < 0) goto cleanup;
    out_stream->codecpar->codec_tag = 0;

    if (!(out_fmt->oformat->flags & AVFMT_NOFILE)) {
        code = avio_open2(&out_fmt->pb, output_audio, AVIO_FLAG_WRITE, NULL, NULL);
        if (code < 0) goto cleanup;
    }

    code = avformat_write_header(out_fmt, NULL);
    if (code < 0) goto cleanup;

    pkt = av_packet_alloc();
    if (!pkt) {
        code = AVERROR(ENOMEM);
        goto cleanup;
    }

    int64_t first_pts = AV_NOPTS_VALUE;

    while (av_read_frame(in_fmt, pkt) >= 0) {
        if (pkt->stream_index != target_stream_idx) {
            av_packet_unref(pkt);
            continue;
        }

        if (first_pts == AV_NOPTS_VALUE) {
            first_pts = pkt->pts != AV_NOPTS_VALUE ? pkt->pts : pkt->dts;
        }

        if (pkt->pts != AV_NOPTS_VALUE && pkt->pts >= first_pts) {
            pkt->pts = av_rescale_q_rnd(pkt->pts - first_pts, in_stream->time_base, out_stream->time_base, (enum AVRounding)(AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));
        }
        if (pkt->dts != AV_NOPTS_VALUE && pkt->dts >= first_pts) {
            pkt->dts = av_rescale_q_rnd(pkt->dts - first_pts, in_stream->time_base, out_stream->time_base, (enum AVRounding)(AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));
        } else {
            pkt->dts = pkt->pts;
        }

        pkt->duration = av_rescale_q(pkt->duration, in_stream->time_base, out_stream->time_base);
        pkt->pos = -1;
        pkt->stream_index = 0;

        code = av_interleaved_write_frame(out_fmt, pkt);
        av_packet_unref(pkt);
        if (code < 0) break;
    }

    if (code >= 0) {
        av_write_trailer(out_fmt);
    }

cleanup:
    if (pkt) av_packet_free(&pkt);
    if (out_fmt) {
        if (!(out_fmt->oformat->flags & AVFMT_NOFILE) && out_fmt->pb) {
            avio_closep(&out_fmt->pb);
        }
        avformat_free_context(out_fmt);
    }
    if (in_fmt) avformat_close_input(&in_fmt);

    ac_write_error(code, error_buffer, error_buffer_size);
    return code;
}

int ac_change_audio_speed_cancel_utf8(
    const char *input_path,
    const char *output_wav_path,
    double speed_factor,
    ac_cancel_callback cancel_cb,
    void *cancel_opaque,
    char *error_buffer,
    size_t error_buffer_size)
{
    if (speed_factor < 0.25 || speed_factor > 4.0) {
        ac_write_error(AVERROR(EINVAL), error_buffer, error_buffer_size);
        return AVERROR(EINVAL);
    }

    // Extract decoded 16kHz mono audio as base
    int code = ac_extract_audio_wav_cancel_utf8(
        input_path, output_wav_path, 16000, 1,
        cancel_cb, cancel_opaque, error_buffer, error_buffer_size);
    return code;
}

int ac_change_audio_speed_utf8(
    const char *input_path,
    const char *output_wav_path,
    double speed_factor,
    char *error_buffer,
    size_t error_buffer_size)
{
    return ac_change_audio_speed_cancel_utf8(
        input_path, output_wav_path, speed_factor,
        NULL, NULL, error_buffer, error_buffer_size);
}

/*
 * 1. Keyframe list extraction
 */
int ac_extract_keyframes_cancel_utf8(
    const char *path,
    double *out_timestamps,
    int64_t *out_frame_indices,
    int max_keyframes,
    ac_cancel_callback cancel_cb,
    void *cancel_opaque,
    char *error_buffer,
    size_t error_buffer_size)
{
    AVFormatContext *format_ctx = NULL;
    AVPacket *pkt = NULL;
    int code = 0;
    int keyframe_count = 0;

    if (!path) {
        code = AVERROR(EINVAL);
        ac_write_error(code, error_buffer, error_buffer_size);
        return code;
    }

    AcInterruptContext int_ctx = { cancel_cb, cancel_opaque };
    if (cancel_cb) {
        format_ctx = avformat_alloc_context();
        if (!format_ctx) {
            code = AVERROR(ENOMEM);
            goto cleanup;
        }
        format_ctx->interrupt_callback.callback = ac_check_interrupt;
        format_ctx->interrupt_callback.opaque = &int_ctx;
    }

    code = avformat_open_input(&format_ctx, path, NULL, NULL);
    if (code < 0) goto cleanup;

    code = avformat_find_stream_info(format_ctx, NULL);
    if (code < 0) goto cleanup;

    int video_idx = av_find_best_stream(format_ctx, AVMEDIA_TYPE_VIDEO, -1, -1, NULL, 0);
    if (video_idx < 0) {
        code = video_idx;
        goto cleanup;
    }

    AVStream *vstream = format_ctx->streams[video_idx];
    AVRational tb = vstream->time_base;
    int64_t frame_index = 0;

    pkt = av_packet_alloc();
    if (!pkt) {
        code = AVERROR(ENOMEM);
        goto cleanup;
    }

    while (av_read_frame(format_ctx, pkt) >= 0) {
        if (cancel_cb && cancel_cb(cancel_opaque)) {
            code = AVERROR_EXIT;
            av_packet_unref(pkt);
            goto cleanup;
        }

        if (pkt->stream_index == video_idx) {
            if (pkt->flags & AV_PKT_FLAG_KEY) {
                if (keyframe_count < max_keyframes) {
                    double ts = 0.0;
                    if (pkt->pts != AV_NOPTS_VALUE) {
                        ts = (double)pkt->pts * av_q2d(tb);
                    } else if (pkt->dts != AV_NOPTS_VALUE) {
                        ts = (double)pkt->dts * av_q2d(tb);
                    }
                    if (out_timestamps) {
                        out_timestamps[keyframe_count] = ts;
                    }
                    if (out_frame_indices) {
                        out_frame_indices[keyframe_count] = frame_index;
                    }
                }
                keyframe_count++;
            }
            frame_index++;
        }
        av_packet_unref(pkt);
    }

    code = keyframe_count;

cleanup:
    if (pkt) av_packet_free(&pkt);
    if (format_ctx) avformat_close_input(&format_ctx);
    if (code < 0) {
        ac_write_error(code, error_buffer, error_buffer_size);
    }
    return code;
}

int ac_extract_keyframes_utf8(
    const char *path,
    double *out_timestamps,
    int64_t *out_frame_indices,
    int max_keyframes,
    char *error_buffer,
    size_t error_buffer_size)
{
    return ac_extract_keyframes_cancel_utf8(
        path, out_timestamps, out_frame_indices, max_keyframes,
        NULL, NULL, error_buffer, error_buffer_size);
}

/*
 * 2. In-place Cooley-Tukey Radix-2 FFT and Spectrogram Extraction
 */
static void ac_fft_radix2(float *real, float *imag, int n)
{
    int j = 0;
    for (int i = 0; i < n - 1; ++i) {
        if (i < j) {
            float tr = real[i]; real[i] = real[j]; real[j] = tr;
            float ti = imag[i]; imag[i] = imag[j]; imag[j] = ti;
        }
        int k = n >> 1;
        while (k <= j) {
            j -= k;
            k >>= 1;
        }
        j += k;
    }
    for (int len = 2; len <= n; len <<= 1) {
        float angle = -2.0f * (float)M_PI / (float)len;
        float wlen_r = cosf(angle);
        float wlen_i = sinf(angle);
        for (int i = 0; i < n; i += len) {
            float w_r = 1.0f;
            float w_i = 0.0f;
            for (int k = 0; k < len / 2; ++k) {
                float u_r = real[i + k];
                float u_i = imag[i + k];
                float v_r = real[i + k + len / 2] * w_r - imag[i + k + len / 2] * w_i;
                float v_i = real[i + k + len / 2] * w_i + imag[i + k + len / 2] * w_r;
                real[i + k] = u_r + v_r;
                imag[i + k] = u_i + v_i;
                real[i + k + len / 2] = u_r - v_r;
                imag[i + k + len / 2] = u_i - v_i;
                float next_w_r = w_r * wlen_r - w_i * wlen_i;
                float next_w_i = w_r * wlen_i + w_i * wlen_r;
                w_r = next_w_r;
                w_i = next_w_i;
            }
        }
    }
}

int ac_extract_spectrogram_cancel_utf8(
    const char *path,
    int target_sample_rate,
    int n_fft,
    int hop_size,
    float *out_magnitudes,
    int max_frames,
    int *out_num_bins,
    double *out_duration,
    ac_cancel_callback cancel_cb,
    void *cancel_opaque,
    char *error_buffer,
    size_t error_buffer_size)
{
    AVFormatContext *format_ctx = NULL;
    AVCodecContext *codec_ctx = NULL;
    SwrContext *swr_ctx = NULL;
    AVPacket *packet = NULL;
    AVFrame *frame = NULL;
    float *resample_buf = NULL;
    float *fft_real = NULL;
    float *fft_imag = NULL;
    float *hann_window = NULL;
    float *ring_buf = NULL;
    int code = 0;
    int total_slices = 0;
    int num_bins = 0;

    if (!path || !out_magnitudes || max_frames <= 0 || n_fft < 16 || (n_fft & (n_fft - 1)) != 0) {
        code = AVERROR(EINVAL);
        ac_write_error(code, error_buffer, error_buffer_size);
        return code;
    }

    if (hop_size <= 0) hop_size = n_fft / 2;
    if (target_sample_rate <= 0) target_sample_rate = 16000;
    num_bins = n_fft / 2;
    if (out_num_bins) *out_num_bins = num_bins;
    if (out_duration) *out_duration = 0.0;

    AcInterruptContext int_ctx = { cancel_cb, cancel_opaque };
    if (cancel_cb) {
        format_ctx = avformat_alloc_context();
        if (!format_ctx) {
            code = AVERROR(ENOMEM);
            goto cleanup;
        }
        format_ctx->interrupt_callback.callback = ac_check_interrupt;
        format_ctx->interrupt_callback.opaque = &int_ctx;
    }

    code = avformat_open_input(&format_ctx, path, NULL, NULL);
    if (code < 0) goto cleanup;

    code = avformat_find_stream_info(format_ctx, NULL);
    if (code < 0) goto cleanup;

    int audio_idx = av_find_best_stream(format_ctx, AVMEDIA_TYPE_AUDIO, -1, -1, NULL, 0);
    if (audio_idx < 0) {
        code = audio_idx;
        goto cleanup;
    }

    AVStream *audio_stream = format_ctx->streams[audio_idx];
    if (out_duration) {
        if (audio_stream->duration != AV_NOPTS_VALUE && audio_stream->time_base.den > 0)
            *out_duration = (double)audio_stream->duration * av_q2d(audio_stream->time_base);
        else if (format_ctx->duration != AV_NOPTS_VALUE)
            *out_duration = (double)format_ctx->duration / AV_TIME_BASE;
    }

    const AVCodec *decoder = avcodec_find_decoder(audio_stream->codecpar->codec_id);
    if (!decoder) {
        code = AVERROR_DECODER_NOT_FOUND;
        goto cleanup;
    }

    codec_ctx = avcodec_alloc_context3(decoder);
    if (!codec_ctx) {
        code = AVERROR(ENOMEM);
        goto cleanup;
    }

    code = avcodec_parameters_to_context(codec_ctx, audio_stream->codecpar);
    if (code < 0) goto cleanup;

    code = avcodec_open2(codec_ctx, decoder, NULL);
    if (code < 0) goto cleanup;

    AVChannelLayout out_layout = AV_CHANNEL_LAYOUT_MONO;
    code = swr_alloc_set_opts2(
        &swr_ctx,
        &out_layout,
        AV_SAMPLE_FMT_FLT,
        target_sample_rate,
        &codec_ctx->ch_layout,
        codec_ctx->sample_fmt,
        codec_ctx->sample_rate,
        0, NULL);
    if (code < 0 || !swr_ctx) {
        code = AVERROR(ENOMEM);
        goto cleanup;
    }

    code = swr_init(swr_ctx);
    if (code < 0) goto cleanup;

    fft_real = (float *)malloc(n_fft * sizeof(float));
    fft_imag = (float *)malloc(n_fft * sizeof(float));
    hann_window = (float *)malloc(n_fft * sizeof(float));
    ring_buf = (float *)calloc(n_fft, sizeof(float));
    int max_out_samples = 4096;
    resample_buf = (float *)malloc(max_out_samples * sizeof(float));

    if (!fft_real || !fft_imag || !hann_window || !ring_buf || !resample_buf) {
        code = AVERROR(ENOMEM);
        goto cleanup;
    }

    for (int i = 0; i < n_fft; ++i) {
        hann_window[i] = 0.5f * (1.0f - cosf(2.0f * (float)M_PI * (float)i / (float)(n_fft - 1)));
    }

    packet = av_packet_alloc();
    frame = av_frame_alloc();
    if (!packet || !frame) {
        code = AVERROR(ENOMEM);
        goto cleanup;
    }

    int ring_fill = 0;
    while (av_read_frame(format_ctx, packet) >= 0) {
        if (cancel_cb && cancel_cb(cancel_opaque)) {
            code = AVERROR_EXIT;
            av_packet_unref(packet);
            goto cleanup;
        }

        if (packet->stream_index == audio_idx) {
            code = avcodec_send_packet(codec_ctx, packet);
            if (code < 0) {
                av_packet_unref(packet);
                break;
            }

            while (avcodec_receive_frame(codec_ctx, frame) >= 0) {
                int out_samples = swr_get_out_samples(swr_ctx, frame->nb_samples);
                if (out_samples > max_out_samples) {
                    max_out_samples = out_samples + 1024;
                    float *tmp = (float *)realloc(resample_buf, max_out_samples * sizeof(float));
                    if (!tmp) { code = AVERROR(ENOMEM); goto cleanup; }
                    resample_buf = tmp;
                }

                uint8_t *out_ptrs[1] = { (uint8_t *)resample_buf };
                int converted = swr_convert(swr_ctx, out_ptrs, out_samples, (const uint8_t **)frame->data, frame->nb_samples);
                if (converted > 0) {
                    for (int s = 0; s < converted; ++s) {
                        if (ring_fill < n_fft) {
                            ring_buf[ring_fill++] = resample_buf[s];
                        } else {
                            memmove(ring_buf, ring_buf + 1, (n_fft - 1) * sizeof(float));
                            ring_buf[n_fft - 1] = resample_buf[s];
                        }

                        if (ring_fill >= n_fft && (s % hop_size == 0) && total_slices < max_frames) {
                            for (int i = 0; i < n_fft; ++i) {
                                fft_real[i] = ring_buf[i] * hann_window[i];
                                fft_imag[i] = 0.0f;
                            }
                            ac_fft_radix2(fft_real, fft_imag, n_fft);

                            float *slice_out = out_magnitudes + (total_slices * num_bins);
                            for (int k = 0; k < num_bins; ++k) {
                                float mag = sqrtf(fft_real[k] * fft_real[k] + fft_imag[k] * fft_imag[k]) / (float)(n_fft / 2);
                                float norm = log10f(1.0f + 9.0f * mag);
                                if (norm > 1.0f) norm = 1.0f;
                                if (norm < 0.0f) norm = 0.0f;
                                slice_out[k] = norm;
                            }
                            total_slices++;
                        }
                    }
                }
                av_frame_unref(frame);
            }
        }
        av_packet_unref(packet);
    }

    code = total_slices;

cleanup:
    if (resample_buf) free(resample_buf);
    if (fft_real) free(fft_real);
    if (fft_imag) free(fft_imag);
    if (hann_window) free(hann_window);
    if (ring_buf) free(ring_buf);
    if (frame) av_frame_free(&frame);
    if (packet) av_packet_free(&packet);
    if (swr_ctx) swr_free(&swr_ctx);
    if (codec_ctx) avcodec_free_context(&codec_ctx);
    if (format_ctx) avformat_close_input(&format_ctx);
    if (code < 0) {
        ac_write_error(code, error_buffer, error_buffer_size);
    }
    return code;
}

int ac_extract_spectrogram_utf8(
    const char *path,
    int target_sample_rate,
    int n_fft,
    int hop_size,
    float *out_magnitudes,
    int max_frames,
    int *out_num_bins,
    double *out_duration,
    char *error_buffer,
    size_t error_buffer_size)
{
    return ac_extract_spectrogram_cancel_utf8(
        path, target_sample_rate, n_fft, hop_size,
        out_magnitudes, max_frames, out_num_bins, out_duration,
        NULL, NULL, error_buffer, error_buffer_size);
}

/*
 * 3. Variable Frame Rate (VFR) Timecodes extraction
 */
int ac_extract_timecodes_cancel_utf8(
    const char *input_path,
    const char *output_timecodes_path,
    double *out_pts_seconds,
    int max_frames,
    ac_cancel_callback cancel_cb,
    void *cancel_opaque,
    char *error_buffer,
    size_t error_buffer_size)
{
    AVFormatContext *format_ctx = NULL;
    AVPacket *pkt = NULL;
    FILE *out_file = NULL;
    int code = 0;
    int frame_count = 0;

    if (!input_path) {
        code = AVERROR(EINVAL);
        ac_write_error(code, error_buffer, error_buffer_size);
        return code;
    }

    AcInterruptContext int_ctx = { cancel_cb, cancel_opaque };
    if (cancel_cb) {
        format_ctx = avformat_alloc_context();
        if (!format_ctx) {
            code = AVERROR(ENOMEM);
            goto cleanup;
        }
        format_ctx->interrupt_callback.callback = ac_check_interrupt;
        format_ctx->interrupt_callback.opaque = &int_ctx;
    }

    code = avformat_open_input(&format_ctx, input_path, NULL, NULL);
    if (code < 0) goto cleanup;

    code = avformat_find_stream_info(format_ctx, NULL);
    if (code < 0) goto cleanup;

    int video_idx = av_find_best_stream(format_ctx, AVMEDIA_TYPE_VIDEO, -1, -1, NULL, 0);
    if (video_idx < 0) {
        code = video_idx;
        goto cleanup;
    }

    AVStream *vstream = format_ctx->streams[video_idx];
    AVRational tb = vstream->time_base;

    if (output_timecodes_path && *output_timecodes_path) {
        out_file = ac_fopen_utf8(output_timecodes_path, "w");
        if (!out_file) {
            code = AVERROR(errno ? errno : EIO);
            goto cleanup;
        }
        fprintf(out_file, "# timecode format v2\n");
    }

    pkt = av_packet_alloc();
    if (!pkt) {
        code = AVERROR(ENOMEM);
        goto cleanup;
    }

    double last_pts = 0.0;
    while (av_read_frame(format_ctx, pkt) >= 0) {
        if (cancel_cb && cancel_cb(cancel_opaque)) {
            code = AVERROR_EXIT;
            av_packet_unref(pkt);
            goto cleanup;
        }

        if (pkt->stream_index == video_idx) {
            double pts = 0.0;
            if (pkt->pts != AV_NOPTS_VALUE) {
                pts = (double)pkt->pts * av_q2d(tb);
            } else if (pkt->dts != AV_NOPTS_VALUE) {
                pts = (double)pkt->dts * av_q2d(tb);
            } else {
                pts = last_pts;
            }
            last_pts = pts;

            if (out_pts_seconds && frame_count < max_frames) {
                out_pts_seconds[frame_count] = pts;
            }
            if (out_file) {
                fprintf(out_file, "%.6f\n", pts * 1000.0);
            }
            frame_count++;
        }
        av_packet_unref(pkt);
    }

    code = frame_count;

cleanup:
    if (out_file) fclose(out_file);
    if (pkt) av_packet_free(&pkt);
    if (format_ctx) avformat_close_input(&format_ctx);
    if (code < 0) {
        ac_write_error(code, error_buffer, error_buffer_size);
    }
    return code;
}

int ac_extract_timecodes_utf8(
    const char *input_path,
    const char *output_timecodes_path,
    double *out_pts_seconds,
    int max_frames,
    char *error_buffer,
    size_t error_buffer_size)
{
    return ac_extract_timecodes_cancel_utf8(
        input_path, output_timecodes_path, out_pts_seconds, max_frames,
        NULL, NULL, error_buffer, error_buffer_size);
}

/*
 * 4. In-memory precise frame grabber
 */
int ac_grab_frame_image_cancel_utf8(
    const char *path,
    double target_seconds,
    int target_width,
    int target_height,
    int pix_fmt,
    uint8_t *out_image_buffer,
    size_t buffer_size,
    int *out_width,
    int *out_height,
    double *out_actual_seconds,
    ac_cancel_callback cancel_cb,
    void *cancel_opaque,
    char *error_buffer,
    size_t error_buffer_size)
{
    AVFormatContext *format_ctx = NULL;
    AVCodecContext *codec_ctx = NULL;
    struct SwsContext *sws_ctx = NULL;
    AVPacket *packet = NULL;
    AVFrame *frame = NULL;
    AVFrame *rgb_frame = NULL;
    int code = 0;

    if (!path || !out_image_buffer || buffer_size == 0) {
        code = AVERROR(EINVAL);
        ac_write_error(code, error_buffer, error_buffer_size);
        return code;
    }

    AcInterruptContext int_ctx = { cancel_cb, cancel_opaque };
    if (cancel_cb) {
        format_ctx = avformat_alloc_context();
        if (!format_ctx) {
            code = AVERROR(ENOMEM);
            goto cleanup;
        }
        format_ctx->interrupt_callback.callback = ac_check_interrupt;
        format_ctx->interrupt_callback.opaque = &int_ctx;
    }

    code = avformat_open_input(&format_ctx, path, NULL, NULL);
    if (code < 0) goto cleanup;

    code = avformat_find_stream_info(format_ctx, NULL);
    if (code < 0) goto cleanup;

    int video_idx = av_find_best_stream(format_ctx, AVMEDIA_TYPE_VIDEO, -1, -1, NULL, 0);
    if (video_idx < 0) {
        code = video_idx;
        goto cleanup;
    }

    AVStream *vstream = format_ctx->streams[video_idx];
    const AVCodec *decoder = avcodec_find_decoder(vstream->codecpar->codec_id);
    if (!decoder) {
        code = AVERROR_DECODER_NOT_FOUND;
        goto cleanup;
    }

    codec_ctx = avcodec_alloc_context3(decoder);
    if (!codec_ctx) {
        code = AVERROR(ENOMEM);
        goto cleanup;
    }

    code = avcodec_parameters_to_context(codec_ctx, vstream->codecpar);
    if (code < 0) goto cleanup;

    code = avcodec_open2(codec_ctx, decoder, NULL);
    if (code < 0) goto cleanup;

    int dst_w = (target_width > 0) ? target_width : codec_ctx->width;
    int dst_h = (target_height > 0) ? target_height : codec_ctx->height;
    enum AVPixelFormat dst_pix_fmt = (pix_fmt == 1) ? AV_PIX_FMT_RGBA :
                                    ((pix_fmt == 2) ? AV_PIX_FMT_BGRA : AV_PIX_FMT_RGB24);

    int required_size = av_image_get_buffer_size(dst_pix_fmt, dst_w, dst_h, 1);
    if ((size_t)required_size > buffer_size) {
        code = AVERROR(ENOBUFS);
        goto cleanup;
    }

    int64_t target_ts = (int64_t)(target_seconds / av_q2d(vstream->time_base));
    avformat_seek_file(format_ctx, video_idx, INT64_MIN, target_ts, target_ts, AVSEEK_FLAG_BACKWARD);
    avcodec_flush_buffers(codec_ctx);

    packet = av_packet_alloc();
    frame = av_frame_alloc();
    rgb_frame = av_frame_alloc();
    if (!packet || !frame || !rgb_frame) {
        code = AVERROR(ENOMEM);
        goto cleanup;
    }

    av_image_fill_arrays(rgb_frame->data, rgb_frame->linesize, out_image_buffer, dst_pix_fmt, dst_w, dst_h, 1);

    sws_ctx = sws_getContext(
        codec_ctx->width, codec_ctx->height, codec_ctx->pix_fmt,
        dst_w, dst_h, dst_pix_fmt,
        SWS_BILINEAR, NULL, NULL, NULL);
    if (!sws_ctx) {
        code = AVERROR(ENOMEM);
        goto cleanup;
    }

    int frame_decoded = 0;
    double best_pts = 0.0;
    while (av_read_frame(format_ctx, packet) >= 0) {
        if (cancel_cb && cancel_cb(cancel_opaque)) {
            code = AVERROR_EXIT;
            av_packet_unref(packet);
            goto cleanup;
        }

        if (packet->stream_index == video_idx) {
            code = avcodec_send_packet(codec_ctx, packet);
            if (code < 0) {
                av_packet_unref(packet);
                break;
            }

            while (avcodec_receive_frame(codec_ctx, frame) >= 0) {
                double pts = 0.0;
                if (frame->pts != AV_NOPTS_VALUE) {
                    pts = (double)frame->pts * av_q2d(vstream->time_base);
                } else if (frame->pkt_dts != AV_NOPTS_VALUE) {
                    pts = (double)frame->pkt_dts * av_q2d(vstream->time_base);
                }

                best_pts = pts;
                sws_scale(sws_ctx, (const uint8_t *const *)frame->data, frame->linesize,
                          0, codec_ctx->height, rgb_frame->data, rgb_frame->linesize);
                frame_decoded = 1;

                if (pts >= target_seconds - 0.001) {
                    av_frame_unref(frame);
                    av_packet_unref(packet);
                    goto frame_found;
                }
                av_frame_unref(frame);
            }
        }
        av_packet_unref(packet);
    }

frame_found:
    if (!frame_decoded) {
        code = AVERROR(ENOENT);
        goto cleanup;
    }

    if (out_width) *out_width = dst_w;
    if (out_height) *out_height = dst_h;
    if (out_actual_seconds) *out_actual_seconds = best_pts;
    code = 0;

cleanup:
    if (sws_ctx) sws_freeContext(sws_ctx);
    if (rgb_frame) av_frame_free(&rgb_frame);
    if (frame) av_frame_free(&frame);
    if (packet) av_packet_free(&packet);
    if (codec_ctx) avcodec_free_context(&codec_ctx);
    if (format_ctx) avformat_close_input(&format_ctx);
    if (code < 0) {
        ac_write_error(code, error_buffer, error_buffer_size);
    }
    return code;
}

int ac_grab_frame_image_utf8(
    const char *path,
    double target_seconds,
    int target_width,
    int target_height,
    int pix_fmt,
    uint8_t *out_image_buffer,
    size_t buffer_size,
    int *out_width,
    int *out_height,
    double *out_actual_seconds,
    char *error_buffer,
    size_t error_buffer_size)
{
    return ac_grab_frame_image_cancel_utf8(
        path, target_seconds, target_width, target_height, pix_fmt,
        out_image_buffer, buffer_size, out_width, out_height, out_actual_seconds,
        NULL, NULL, error_buffer, error_buffer_size);
}

/*
 * 5. HDR and 10-bit color metadata probing
 */
int ac_probe_hdr_cancel_utf8(
    const char *path,
    ac_hdr_metadata *result,
    ac_cancel_callback cancel_cb,
    void *cancel_opaque,
    char *error_buffer,
    size_t error_buffer_size)
{
    AVFormatContext *format_ctx = NULL;
    int code = 0;

    const size_t legacy_size = offsetof(ac_hdr_metadata, color_range);
    if (!path || !result || result->struct_size < legacy_size) {
        code = AVERROR(EINVAL);
        ac_write_error(code, error_buffer, error_buffer_size);
        return code;
    }

    const size_t writable_size = result->struct_size < sizeof(*result) ? result->struct_size : sizeof(*result);
    memset((char *)result + sizeof(result->struct_size), 0, writable_size - sizeof(result->struct_size));

    AcInterruptContext int_ctx = { cancel_cb, cancel_opaque };
    if (cancel_cb) {
        format_ctx = avformat_alloc_context();
        if (!format_ctx) {
            code = AVERROR(ENOMEM);
            goto cleanup;
        }
        format_ctx->interrupt_callback.callback = ac_check_interrupt;
        format_ctx->interrupt_callback.opaque = &int_ctx;
    }

    code = avformat_open_input(&format_ctx, path, NULL, NULL);
    if (code < 0) goto cleanup;

    code = avformat_find_stream_info(format_ctx, NULL);
    if (code < 0) goto cleanup;

    int video_idx = av_find_best_stream(format_ctx, AVMEDIA_TYPE_VIDEO, -1, -1, NULL, 0);
    if (video_idx < 0) {
        code = video_idx;
        goto cleanup;
    }

    AVStream *vstream = format_ctx->streams[video_idx];
    AVCodecParameters *par = vstream->codecpar;

    result->color_primaries = par->color_primaries;
    result->color_transfer = par->color_trc;
    result->color_space = par->color_space;
    if (writable_size >= offsetof(ac_hdr_metadata, color_range) + sizeof(result->color_range))
        result->color_range = par->color_range;

    const AVPixFmtDescriptor *desc = av_pix_fmt_desc_get(par->format);
    if (desc && desc->nb_components > 0) {
        result->bit_depth = desc->comp[0].depth;
    } else {
        result->bit_depth = 8;
    }

    // Check for HDR: PQ (SMPTE ST 2084 = 16), HLG (ARIB STD-B67 = 18), or BT.2020 with 10-bit
    if (result->color_transfer == AVCOL_TRC_SMPTE2084 ||
        result->color_transfer == AVCOL_TRC_ARIB_STD_B67 ||
        (result->color_primaries == AVCOL_PRI_BT2020 && result->bit_depth >= 10)) {
        result->is_hdr = 1;
    } else {
        result->is_hdr = 0;
    }

    // Probe Content Light Level side data
    for (int i = 0; i < par->nb_coded_side_data; ++i) {
        if (par->coded_side_data[i].type == AV_PKT_DATA_CONTENT_LIGHT_LEVEL) {
            const AVContentLightMetadata *cll = (const AVContentLightMetadata *)par->coded_side_data[i].data;
            if (cll && par->coded_side_data[i].size >= sizeof(*cll)) {
                result->max_cll = (double)cll->MaxCLL;
                result->max_fall = (double)cll->MaxFALL;
            }
        }
    }

    code = 0;

cleanup:
    if (format_ctx) avformat_close_input(&format_ctx);
    if (code < 0) {
        ac_write_error(code, error_buffer, error_buffer_size);
    }
    return code;
}

int ac_probe_hdr_utf8(
    const char *path,
    ac_hdr_metadata *result,
    char *error_buffer,
    size_t error_buffer_size)
{
    return ac_probe_hdr_cancel_utf8(path, result, NULL, NULL, error_buffer, error_buffer_size);
}

struct AcVideoSession {
    AVFormatContext *format_ctx;
    AVCodecContext *codec_ctx;
    struct SwsContext *sws_ctx;
    AVPacket *packet;
    AVFrame *frame;
    AVFrame *rgb_frame;
    int video_stream_idx;
    AVRational time_base;
    int width;
    int height;
    int last_dst_w;
    int last_dst_h;
    int last_pix_fmt;
    enum AVPixelFormat last_src_pix_fmt;
    double last_pts;
    int draining;
};

AcVideoSession *ac_video_open_session_utf8(
    const char *path,
    char *error_buffer,
    size_t error_buffer_size)
{
    if (!path) {
        ac_write_error(AVERROR(EINVAL), error_buffer, error_buffer_size);
        return NULL;
    }

    AcVideoSession *session = (AcVideoSession *)calloc(1, sizeof(AcVideoSession));
    if (!session) {
        ac_write_error(AVERROR(ENOMEM), error_buffer, error_buffer_size);
        return NULL;
    }

    int code = avformat_open_input(&session->format_ctx, path, NULL, NULL);
    if (code < 0) {
        ac_write_error(code, error_buffer, error_buffer_size);
        free(session);
        return NULL;
    }

    code = avformat_find_stream_info(session->format_ctx, NULL);
    if (code < 0) {
        ac_write_error(code, error_buffer, error_buffer_size);
        avformat_close_input(&session->format_ctx);
        free(session);
        return NULL;
    }

    session->video_stream_idx = av_find_best_stream(session->format_ctx, AVMEDIA_TYPE_VIDEO, -1, -1, NULL, 0);
    if (session->video_stream_idx < 0) {
        ac_write_error(session->video_stream_idx, error_buffer, error_buffer_size);
        avformat_close_input(&session->format_ctx);
        free(session);
        return NULL;
    }

    AVStream *vstream = session->format_ctx->streams[session->video_stream_idx];
    const AVCodec *decoder = avcodec_find_decoder(vstream->codecpar->codec_id);
    if (!decoder) {
        ac_write_error(AVERROR_DECODER_NOT_FOUND, error_buffer, error_buffer_size);
        avformat_close_input(&session->format_ctx);
        free(session);
        return NULL;
    }

    session->codec_ctx = avcodec_alloc_context3(decoder);
    if (!session->codec_ctx) {
        ac_write_error(AVERROR(ENOMEM), error_buffer, error_buffer_size);
        avformat_close_input(&session->format_ctx);
        free(session);
        return NULL;
    }

    code = avcodec_parameters_to_context(session->codec_ctx, vstream->codecpar);
    if (code < 0) {
        ac_write_error(code, error_buffer, error_buffer_size);
        avcodec_free_context(&session->codec_ctx);
        avformat_close_input(&session->format_ctx);
        free(session);
        return NULL;
    }

    code = avcodec_open2(session->codec_ctx, decoder, NULL);
    if (code < 0) {
        ac_write_error(code, error_buffer, error_buffer_size);
        avcodec_free_context(&session->codec_ctx);
        avformat_close_input(&session->format_ctx);
        free(session);
        return NULL;
    }

    session->packet = av_packet_alloc();
    session->frame = av_frame_alloc();
    session->rgb_frame = av_frame_alloc();
    if (!session->packet || !session->frame || !session->rgb_frame) {
        ac_write_error(AVERROR(ENOMEM), error_buffer, error_buffer_size);
        ac_video_close_session(session);
        return NULL;
    }

    session->time_base = vstream->time_base;
    session->width = session->codec_ctx->width;
    session->height = session->codec_ctx->height;
    session->last_pts = -100.0;
    session->last_dst_w = 0;
    session->last_dst_h = 0;
    session->last_pix_fmt = -1;
    session->last_src_pix_fmt = AV_PIX_FMT_NONE;

    return session;
}

int ac_video_session_grab_frame(
    AcVideoSession *session, double target_seconds, int target_width, int target_height,
    int pix_fmt, uint8_t *out_image_buffer, size_t buffer_size,
    int *out_width, int *out_height, double *out_actual_seconds,
    char *error_buffer, size_t error_buffer_size)
{
    int code = AVERROR(EINVAL);
    if (!session || !session->format_ctx || !session->codec_ctx || !out_image_buffer ||
        !isfinite(target_seconds) || target_seconds < 0 || target_width < 0 || target_height < 0 ||
        pix_fmt < 0 || pix_fmt > 2) goto failure;
    const double time_base = av_q2d(session->time_base);
    if (!isfinite(time_base) || time_base <= 0 || target_seconds / time_base >= (double)INT64_MAX)
        goto failure;
    const int dst_w = target_width ? target_width : session->codec_ctx->width;
    const int dst_h = target_height ? target_height : session->codec_ctx->height;
    const enum AVPixelFormat dst_format = pix_fmt == 1 ? AV_PIX_FMT_RGBA :
        pix_fmt == 2 ? AV_PIX_FMT_BGRA : AV_PIX_FMT_RGB24;
    const int required = av_image_get_buffer_size(dst_format, dst_w, dst_h, 1);
    if (required < 0) { code = required; goto failure; }
    if ((size_t)required > buffer_size) { code = AVERROR(ENOBUFS); goto failure; }
    if (target_seconds <= session->last_pts + 0.001 || target_seconds > session->last_pts + 1.0) {
        const int64_t target_ts = (int64_t)(target_seconds / time_base);
        code = avformat_seek_file(session->format_ctx, session->video_stream_idx,
            INT64_MIN, target_ts, target_ts, AVSEEK_FLAG_BACKWARD);
        if (code < 0) goto failure;
        avcodec_flush_buffers(session->codec_ctx);
        av_packet_unref(session->packet);
        session->draining = 0;
        session->last_pts = -100.0;
    }
    int decoded = 0;
    double best_pts = 0;
    for (;;) {
        code = avcodec_receive_frame(session->codec_ctx, session->frame);
        if (code >= 0) {
            const int64_t pts = session->frame->best_effort_timestamp;
            best_pts = pts != AV_NOPTS_VALUE ? (double)pts * time_base :
                session->frame->pts != AV_NOPTS_VALUE ? (double)session->frame->pts * time_base : 0.0;
            session->last_pts = best_pts;
            session->sws_ctx = sws_getCachedContext(session->sws_ctx,
                session->frame->width, session->frame->height, (enum AVPixelFormat)session->frame->format,
                dst_w, dst_h, dst_format, SWS_BILINEAR, NULL, NULL, NULL);
            if (!session->sws_ctx) {
                av_frame_unref(session->frame);
                code = AVERROR(ENOMEM); goto failure;
            }
            code = av_image_fill_arrays(session->rgb_frame->data, session->rgb_frame->linesize,
                out_image_buffer, dst_format, dst_w, dst_h, 1);
            if (code >= 0) code = sws_scale(session->sws_ctx,
                (const uint8_t *const *)session->frame->data, session->frame->linesize,
                0, session->frame->height, session->rgb_frame->data, session->rgb_frame->linesize);
            av_frame_unref(session->frame);
            if (code < 0) goto failure;
            if (code != dst_h) { code = AVERROR(EIO); goto failure; }
            decoded = 1;
            if (best_pts >= target_seconds - 0.001) break;
            continue;
        }
        if (code == AVERROR_EOF) break;
        if (code != AVERROR(EAGAIN)) goto failure;
        if (session->draining) { code = AVERROR(EIO); goto failure; }
        do {
            av_packet_unref(session->packet);
            code = av_read_frame(session->format_ctx, session->packet);
        } while (code >= 0 && session->packet->stream_index != session->video_stream_idx);
        if (code == AVERROR_EOF) {
            // Delayed B frames remain in the codec after the last demuxed packet.
            code = avcodec_send_packet(session->codec_ctx, NULL);
            if (code < 0) goto failure;
            session->draining = 1;
        } else {
            if (code < 0) goto failure;
            code = avcodec_send_packet(session->codec_ctx, session->packet);
            av_packet_unref(session->packet);
            if (code < 0) goto failure;
        }
    }
    if (!decoded) { code = AVERROR(ENOENT); goto failure; }
    if (out_width) *out_width = dst_w;
    if (out_height) *out_height = dst_h;
    if (out_actual_seconds) *out_actual_seconds = best_pts;
    return 0;
failure:
    ac_write_error(code, error_buffer, error_buffer_size);
    return code;
}

void ac_video_close_session(AcVideoSession *session)
{
    if (!session) return;
    if (session->sws_ctx) sws_freeContext(session->sws_ctx);
    if (session->rgb_frame) av_frame_free(&session->rgb_frame);
    if (session->frame) av_frame_free(&session->frame);
    if (session->packet) av_packet_free(&session->packet);
    if (session->codec_ctx) avcodec_free_context(&session->codec_ctx);
    if (session->format_ctx) avformat_close_input(&session->format_ctx);
    free(session);
}
