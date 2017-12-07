#include <iostream>

#ifdef __cplusplus
extern "C"{
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#endif
#ifdef __cplusplus
};
#endif

using namespace std;

/**
 * 将AVFrame(YUV420格式)保存为JPEG格式的图片
 *
 * @param width YUV420的宽
 * @param height YUV42的高
 *
 */
int MyWriteJPEG(AVFrame* pFrame, int width, int height, int iIndex)
{
    // 输出文件路径
    char out_file[20] = {0};
    sprintf(out_file, "out%d.jpg", iIndex);
    
    // 分配AVFormatContext对象
    AVFormatContext* pFormatCtx = avformat_alloc_context();
    
    // 设置输出文件格式
    pFormatCtx->oformat = av_guess_format("mjpeg", NULL, NULL);
    // 创建并初始化一个和该url相关的AVIOContext
    if( avio_open(&pFormatCtx->pb, out_file, AVIO_FLAG_READ_WRITE) < 0) {
        printf("Couldn't open output file.");
        return -1;
    }
    
    // 构建一个新stream
    AVStream* pAVStream = avformat_new_stream(pFormatCtx, 0);
    if( pAVStream == NULL ) {
        return -1;
    }
    
    // 设置该stream的信息
    AVCodecContext* pCodecCtx = pAVStream->codec;
    
    pCodecCtx->codec_id = pFormatCtx->oformat->video_codec;
    pCodecCtx->codec_type = AVMEDIA_TYPE_VIDEO;
    pCodecCtx->pix_fmt = AV_PIX_FMT_YUVJ420P;
    pCodecCtx->width = width;
    pCodecCtx->height = height;
    pCodecCtx->time_base.num = 1;
    pCodecCtx->time_base.den = 25;
    
    // Begin Output some information
    av_dump_format(pFormatCtx, 0, out_file, 1);
    // End Output some information
    
    // 查找解码器
    AVCodec* pCodec = avcodec_find_encoder(pCodecCtx->codec_id);
    if( !pCodec ) {
        printf("Codec not found.");
        return -1;
    }
    // 设置pCodecCtx的解码器为pCodec
    if( avcodec_open2(pCodecCtx, pCodec, NULL) < 0 ) {
        printf("Could not open codec.");
        return -1;
    }
    
    //Write Header
    avformat_write_header(pFormatCtx, NULL);
    
    int y_size = pCodecCtx->width * pCodecCtx->height;
    
    //Encode
    // 给AVPacket分配足够大的空间
    AVPacket pkt;
    av_new_packet(&pkt, y_size * 3);
    
    // 
    int got_picture = 0;
    int ret = avcodec_encode_video2(pCodecCtx, &pkt, pFrame, &got_picture);
    if( ret < 0 ) {
        printf("Encode Error.\n");
        return -1;
    }
    if( got_picture == 1 ) {
        //pkt.stream_index = pAVStream->index;
        ret = av_write_frame(pFormatCtx, &pkt);
    }

    av_free_packet(&pkt);

    //Write Trailer
    av_write_trailer(pFormatCtx);

    printf("Encode Successful.\n");

    if( pAVStream ) {
        avcodec_close(pAVStream->codec);
    }
    avio_close(pFormatCtx->pb);
    avformat_free_context(pFormatCtx);
    
    return 0;
}

int Work_Save2JPG()
{
    int videoStream = -1;
    AVCodecContext *pCodecCtx;
    AVFormatContext *pFormatCtx;
    AVCodec *pCodec;
    AVFrame *pFrame, *pFrameRGB;
    struct SwsContext *pSwsCtx;
    const char *filename = "src.yuv";
    AVPacket packet;
    int frameFinished;
    int PictureSize;
    uint8_t *outBuff;
    AVDictionary *options;

    //注册编解码器
    av_register_all();
    // 初始化网络模块
    avformat_network_init();
    // 分配AVFormatContext
    pFormatCtx = avformat_alloc_context();
    
    /**
     *  yuv格式是不包含视频分辨率和帧率信息的，如果不在打开yuv文件之前指定这些值是会打开失败的
     *  如果分辨率video_size设置不正确，得到的图片就是不正常的
     *  获取video_size的一种方法是在shell命令行下使用ffprobe *.mp4
     *  来查询视频的信息，yuv文件获得的方式是在shell命令行下使用
     *  ffmpeg -i *.mp4 *.yuv 直接转换过来的
     */
    av_dict_set(&options, "framerate", "23.98", 0);
    av_dict_set(&options, "video_size", "426x240", 0);
    
    //打开视频文件
    if( avformat_open_input(&pFormatCtx, filename, NULL, &options) != 0 ) {
        printf ("av open input file failed!\n");
        exit (1);
    }

    //获取流信息
    if( avformat_find_stream_info(pFormatCtx, NULL) < 0 ) {
        printf ("av find stream info failed!\n");
        exit (1);
    }
    //获取视频流
    for( int i = 0; i < pFormatCtx->nb_streams; i++ ) {
        if ( pFormatCtx->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO ) {
            videoStream = i;
            break;
        }
    }
    if( videoStream == -1 ) {
        printf ("find video stream failed!\n");
        exit (1);
    }

    // 寻找解码器
    /**
     * 这里读的是yuv格式文件，所以是没有编码的原始数据，所以找到的解码器是AV_CODEC_ID_RAWVIDEO，其实是可以不用解码的，但是这里为了方便调用api，在下面会使用这个
     * 解码到结构体AVFrame,然后在用编码器将其编码为AVPacket,然后保存
     * 
     */
    pCodecCtx = pFormatCtx->streams[videoStream]->codec;
    pCodec = avcodec_find_decoder(pCodecCtx->codec_id);
    if( pCodec == NULL ) {
        printf ("avcode find decoder failed!\n");
        exit (1);
    }

    //打开解码器
    if( avcodec_open2(pCodecCtx, pCodec, NULL) < 0 ) {
        printf ("avcode open failed!\n");
        exit (1);
    }

    //为每帧图像分配内存
    pFrame = av_frame_alloc();//avcodec_alloc_frame();
    pFrameRGB = av_frame_alloc();
    if( (pFrame == NULL) || (pFrameRGB == NULL) ) {
        printf("avcodec alloc frame failed!\n");
        exit (1);
    }

    // 确定图片尺寸
    PictureSize = avpicture_get_size(AV_PIX_FMT_YUVJ420P, pCodecCtx->width, pCodecCtx->height);
    outBuff = (uint8_t*)av_malloc(PictureSize);
    if( outBuff == NULL ) {
        printf("av malloc failed!\n");
        exit(1);
    }
    avpicture_fill((AVPicture *)pFrameRGB, outBuff, AV_PIX_FMT_YUVJ420P, pCodecCtx->width, pCodecCtx->height);

    //设置图像转换上下文
    pSwsCtx = sws_getContext(pCodecCtx->width, pCodecCtx->height, pCodecCtx->pix_fmt,
        pCodecCtx->width, pCodecCtx->height, AV_PIX_FMT_YUVJ420P,
        SWS_BICUBIC, NULL, NULL, NULL);

    int i = 0;
    if( av_read_frame(pFormatCtx, &packet) >= 0 ) {
        if( packet.stream_index == videoStream ) {
            avcodec_decode_video2(pCodecCtx, pFrame, &frameFinished, &packet);

            if( frameFinished ) {
                // 保存为jpeg时不需要反转图像
                static bool b1 = true;
                if( b1 ) {
                    MyWriteJPEG(pFrame, pCodecCtx->width, pCodecCtx->height, i++);
                    
                    b1 = false;
                }
                
            }
        } else {
            int a=2;
            int b=a;
        }

        av_free_packet(&packet);
    }

    sws_freeContext(pSwsCtx);

    av_free(pFrame);
    av_free(pFrameRGB);
    avcodec_close(pCodecCtx);
    avformat_close_input(&pFormatCtx);

    return 0;
}

int main(int argc, char **argv) {
    Work_Save2JPG();
    
    return 0;
}
