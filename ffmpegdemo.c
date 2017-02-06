
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <signal.h>
#include <time.h>


#include "libavformat/avformat.h"
#include "libavutil/intreadwrite.h"

#define FFMPEG_BUFFER_SIZE (4*1024)
#define RTP_PAYLOAD_MAX_SIZE        (1024)

#define NAL_SPS (7)
#define NAL_PPS (8)

static int quit = 0;
void sig_handle(int signo) { 
    printf("recived quit signal");
    quit = 1; 
}

static int VideoWriteData(void *opaque, uint8_t *buf, int buf_size)
{
    int ret = 0;
    int i = 0;
    int packetType = 0;

    packetType = buf[1];
    if(packetType > 207 || packetType < 200) {
        /*
        printf("video rtp data(%d):\n", buf_size);
        if (buf_size < 128) {
            for (i = 0; i < buf_size; i++) {
                printf("0x%02x ", buf[i]);
            }
            printf("\n");
        }*/
    }
    else {
        printf("VideoWriteData rtcp sr\n");
    }
    
    return ret;
}

static int AudioWriteData(void *opaque, uint8_t *buf, int buf_size)
{
    int ret = 0;
    int i = 0;
     

    return ret;
}


static int h264_extradata_to_annexb(uint8_t *new_extradata, int new_extradata_size, uint8_t **sps_pps)
{
    uint8_t *out = NULL;
    uint8_t sps_done = 0, sps_seen = 0, pps_seen = 0;
    const uint8_t *extradata = new_extradata + 4;
    uint8_t nalu_header[4] = { 0, 0, 0, 1 };
    int length_size = (*extradata++ & 0x3) + 1; // retrieve length coded size
    uint8_t unit_nb = 0;
    uint16_t unit_size = 0;
    uint64_t total_size = 0;
    const int padding = AV_INPUT_BUFFER_PADDING_SIZE;

    /* retrieve sps and pps unit(s) */
    unit_nb = *extradata++ & 0x1f; /* number of sps unit(s) */
    if (!unit_nb) {
        goto pps;
    } else {
        sps_seen = 1;
    }

    while (unit_nb--) {
        int err;

        unit_size   = AV_RB16(extradata);
        total_size += unit_size + 4;
        
        if (total_size > INT_MAX - padding) {
            av_free(out);
            return AVERROR(EINVAL);
        }
        if (extradata + 2 + unit_size > new_extradata + new_extradata_size) {
            av_free(out);
            return AVERROR(EINVAL);
        }
        if ((err = av_reallocp(&out, total_size + padding)) < 0)
            return err;
        nalu_header[3] = unit_size;
        memcpy(out + total_size - unit_size - 4, nalu_header, 4);
        memcpy(out + total_size - unit_size, extradata + 2, unit_size);
        extradata += 2 + unit_size;
pps:
        if (!unit_nb && !sps_done++) {
            unit_nb = *extradata++; /* number of pps unit(s) */
            if (unit_nb) {
                pps_seen = 1;
            }
        }
    }
    *sps_pps = out;
    return total_size;
}


static void *rtmpPullFFMpegThread(void *arg)
{
    char *url = (char *)arg;
    int res;
    char errbuff[500];
    int i = 0;
    time_t tt;

    //input
    AVFormatContext *ifmt_ctx = NULL;
    AVPacket pkt;

    //video
    AVFormatContext *ofmt_ctx_v = NULL;
    AVDictionary *v_options = NULL;
    int video_stream_index_;
    unsigned char *pVideoBuffer = NULL;

    //audio
    AVFormatContext *ofmt_ctx_a = NULL;
    AVDictionary *a_options = NULL;
    int audio_stream_index_;
    unsigned char *pAudioBuffer = NULL;
    int64_t last_audio_pts = 0;

    //open input stream
    tt=time(NULL);
    printf("avformat_open_input %s start, ts:%d\n", url, tt);
    res = avformat_open_input(&ifmt_ctx, url, NULL, NULL);
    if (res != 0) {
        av_strerror(res, errbuff, sizeof(errbuff));
        printf("Error opening input %s\n", errbuff);
        goto end;
    }
    tt=time(NULL);
    printf("avformat_open_input success, ts:%d\n", tt);

    ifmt_ctx->flags = ifmt_ctx->flags | AVFMT_FLAG_KEEP_SIDE_DATA;
    //设置解析输入流大小,减少等待时间
    ifmt_ctx->probesize = 128*1024;
    ifmt_ctx->max_analyze_duration = AV_TIME_BASE;

    printf("avformat_find_stream_info\n");
    res = avformat_find_stream_info(ifmt_ctx, NULL);
    if (res < 0) {
        av_strerror(res, errbuff, sizeof(errbuff));
        printf("Error finding stream info %s\n", errbuff);
        goto end;
    }
    av_dump_format(ifmt_ctx, 0, url, 0);

    
    //output stream
    res = avformat_alloc_output_context2(&ofmt_ctx_v, NULL, "rtp", NULL);
    if (!ofmt_ctx_v) {
        av_strerror(res, errbuff, sizeof(errbuff));
        printf("Error opening output %s\n", errbuff);
        goto end;
    }

    pVideoBuffer = (unsigned char*)av_malloc(FFMPEG_BUFFER_SIZE);
    if(!pVideoBuffer){
        printf("malloc ffmpeg buffer size error\n");
        goto end;
    }
    ofmt_ctx_v->pb = avio_alloc_context(pVideoBuffer, FFMPEG_BUFFER_SIZE, AVIO_FLAG_WRITE,
                                      NULL, NULL, VideoWriteData, NULL);
    if(ofmt_ctx_v->pb == NULL) {
        av_strerror(res, errbuff, sizeof(errbuff));
        printf("Error opening output %s\n", errbuff);
        goto end;
    }
    ofmt_ctx_v->pb->max_packet_size = RTP_PAYLOAD_MAX_SIZE;
    /* 说明采用定制IO函数 */
    ofmt_ctx_v->flags = AVFMT_FLAG_CUSTOM_IO;

    //audio
    res = avformat_alloc_output_context2(&ofmt_ctx_a, NULL, "rtp", NULL);
    if (!ofmt_ctx_a) {
        av_strerror(res, errbuff, sizeof(errbuff));
        printf("Error opening output %s\n", errbuff);
        goto end;
    }

    pAudioBuffer = (unsigned char*)av_malloc(FFMPEG_BUFFER_SIZE);
    if(!pAudioBuffer){
        printf("malloc ffmpeg buffer size error\n");
        goto end;
    }
    ofmt_ctx_a->pb = avio_alloc_context(pAudioBuffer, FFMPEG_BUFFER_SIZE, AVIO_FLAG_WRITE,
                                      NULL, NULL, AudioWriteData, NULL);
    if(ofmt_ctx_a->pb == NULL) {
        av_strerror(res, errbuff, sizeof(errbuff));
        printf("Error opening output %s\n", errbuff);
        goto end;
    }
    ofmt_ctx_a->pb->max_packet_size = RTP_PAYLOAD_MAX_SIZE;
    /* 说明采用定制IO函数 */
    ofmt_ctx_a->flags = AVFMT_FLAG_CUSTOM_IO;


    //
    for (i = 0; i < ifmt_ctx->nb_streams; i++) {
        //Create output AVStream according to input AVStream
        AVFormatContext *ofmt_ctx = NULL;
        AVStream *out_stream = NULL;
        AVStream *in_stream = ifmt_ctx->streams[i];

        if(in_stream->codec->codec_type == AVMEDIA_TYPE_VIDEO) {//video
            video_stream_index_ = i;
            ofmt_ctx = ofmt_ctx_v;
        }
        else if(in_stream->codec->codec_type == AVMEDIA_TYPE_AUDIO){//audio
            audio_stream_index_ = i;
            ofmt_ctx = ofmt_ctx_a;
        }

        out_stream = avformat_new_stream(ofmt_ctx, in_stream->codec->codec);
        //Copy the settings of AVCodecContext
        res = avcodec_copy_context(out_stream->codec, in_stream->codec);
        if (res < 0) {
            printf( "Failed to copy context from input to output stream codec context\n");
            goto end;
        }

        out_stream->codec->codec_tag = 0;
        if (ofmt_ctx->oformat->flags & AVFMT_GLOBALHEADER)
            out_stream->codec->flags |= CODEC_FLAG_GLOBAL_HEADER;
    }
    av_dump_format(ofmt_ctx_v, 0, NULL, 1);
    av_dump_format(ofmt_ctx_a, 0, NULL, 1);

    //Write file header
    av_dict_set_int(&v_options, "payload_type", 107, 0);
    av_dict_set_int(&v_options, "ssrc", 107, 0);

    av_dict_set_int(&a_options, "payload_type", 125, 0); 
    av_dict_set_int(&a_options, "ssrc", 125, 0);

    res = avformat_write_header(ofmt_ctx_v, &v_options);
    if (res < 0) {
        av_strerror(res, errbuff, sizeof(errbuff));
        printf( "Error avformat_write_header, %s\n", errbuff);
        goto end;
    }
    res = avformat_write_header(ofmt_ctx_a, &a_options);
    if (res < 0) {
        av_strerror(res, errbuff, sizeof(errbuff));
        printf( "Error avformat_write_header, %s\n", errbuff);
        goto end;
    }


    while (!quit){
        AVStream *in_stream = NULL, *out_stream = NULL;
        AVFormatContext *ofmt_ctx = NULL;

        //Get an AVPacket
        res = av_read_frame(ifmt_ctx, &pkt);
        if(res < 0) {
            av_strerror(res, errbuff, sizeof(errbuff));
            printf("Error opening input %s\n", errbuff);
            if (res == AVERROR(EAGAIN)) {
                continue;
            }
            break;
        }
        
        
        //
        if(video_stream_index_ == pkt.stream_index) {
            ofmt_ctx = ofmt_ctx_v;
        }
        else if(audio_stream_index_ == pkt.stream_index) {
            ofmt_ctx = ofmt_ctx_a;
        }
        else {
            continue;
        }
        in_stream  = ifmt_ctx->streams[pkt.stream_index];
        out_stream = ofmt_ctx->streams[0];

        if (pkt.dts != AV_NOPTS_VALUE)
            pkt.dts += av_rescale_q(-ifmt_ctx->start_time, AV_TIME_BASE_Q, in_stream->time_base);
        if (pkt.pts != AV_NOPTS_VALUE)
            pkt.pts += av_rescale_q(-ifmt_ctx->start_time, AV_TIME_BASE_Q, in_stream->time_base);


        


        //Convert PTS/DTS
        pkt.pts = av_rescale_q(pkt.pts, in_stream->time_base, out_stream->time_base);
        pkt.dts = av_rescale_q(pkt.dts, in_stream->time_base, out_stream->time_base);
        pkt.duration = av_rescale_q(pkt.duration, in_stream->time_base, out_stream->time_base);
        pkt.pos = -1;

        if (video_stream_index_ == pkt.stream_index) {
            unsigned char nal_type = pkt.data[4] & 0x1f;

            if ((pkt.flags & AV_PKT_FLAG_KEY) && (nal_type != NAL_SPS)){
                AVPacket sps_pps_pkt = pkt;
                unsigned char *sps_pps = NULL;
                int side_size = 0;
                uint8_t *tmp = NULL;

                tmp = av_packet_get_side_data(&pkt, AV_PKT_DATA_NEW_EXTRADATA, &side_size);
                if (!tmp) {
                    tmp = in_stream->codec->extradata;
                    side_size = in_stream->codec->extradata_size;
                    printf("do not have side data, use in_stream codec extradata\n");
                }
                else {
                    printf("have side data, use side data as extradata\n");
                }

                res = h264_extradata_to_annexb(tmp, side_size, &sps_pps);
                if (res > 0) {
                    av_init_packet(&sps_pps_pkt);
                    sps_pps_pkt.pts = pkt.pts;
                    sps_pps_pkt.dts = pkt.dts;
                    sps_pps_pkt.duration = pkt.duration;
                    sps_pps_pkt.pos = pkt.pos;

                    /*

                    printf("new sps pps data: \n");
                    for (i=0; i< res; i++) {
                        printf("0x%02x ", sps_pps[i]);
                    }
                    printf("\n");
                    */

                    av_packet_from_data(&sps_pps_pkt, sps_pps, res);
                    av_interleaved_write_frame(ofmt_ctx, &sps_pps_pkt);
                    av_free_packet(&sps_pps_pkt);
                }
            }
            else if(pkt.flags & AV_PKT_FLAG_KEY){
                printf("key frame\n");
            }
        }
        
        //printf("pkt.pts:%lld\n", pkt.pts);
        pkt.stream_index = 0; 
        av_interleaved_write_frame(ofmt_ctx, &pkt);

        //free
        av_free_packet(&pkt);
    }
    printf("SRS: rtmpPullThread exit.\n");

end:
    if (ifmt_ctx != NULL) {
        avformat_free_context(ifmt_ctx);
        ifmt_ctx = NULL;
    }

    if (ofmt_ctx_v != NULL) {
        if (ofmt_ctx_v->pb) {
            av_freep(&ofmt_ctx_v->pb->buffer);
            av_freep(&ofmt_ctx_v->pb);
            ofmt_ctx_v->pb = NULL;
        }
        avformat_free_context(ofmt_ctx_v); 
    }

    if (ofmt_ctx_a != NULL){
        if (ofmt_ctx_a->pb) {
            av_freep(&ofmt_ctx_a->pb->buffer);
            av_freep(&ofmt_ctx_a->pb);
            ofmt_ctx_a->pb = NULL;
        }
        avformat_free_context(ofmt_ctx_a);
    }

    return NULL;
}

int main(int argc, char *argv[])
{
    avcodec_register_all();
    avdevice_register_all();
    avfilter_register_all();
    av_register_all();
    avformat_network_init();

    signal(SIGINT, sig_handle);

    printf("ffmpeg_init success\n");

    rtmpPullFFMpegThread(argv[1]);

    return 0;
}
