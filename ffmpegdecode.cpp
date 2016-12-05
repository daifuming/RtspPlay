#include "ffmpegdecode.h"
#include <QDateTime>
#include <QDebug>
#include <QCoreApplication>
#include "stdio.h"

//static   int   interrupt_cb( void   *ctx)

//{
//    // do something
//    time_out++;
//    if   (time_out > 40) {
//        time_out=0;
//        if   (firsttimeplay) {
//            firsttimeplay=0;
//            return   1; //������ǳ�ʱ�ķ���
//        }
//    }
//    return   0;
//}

ffmpegDecode::ffmpegDecode(QObject *parent):
    QObject(parent)
{
    pCodecCtx = NULL;
    videoStreamIndex=-1;
    av_register_all();//ע��������п��õ��ļ���ʽ�ͽ�����
    avformat_network_init();//��ʼ����������ʽ,ʹ��RTSP������ʱ������ִ��
    pFormatCtx = avformat_alloc_context();//����һ��AVFormatContext�ṹ���ڴ�,�����м򵥳�ʼ��
    o_pFormatCtx = NULL;

    //out
    ofmt = NULL;

//   s pFormatCtx->interrupt_callback = interrupt_cb;  //ע��ص�����
    pFrame=av_frame_alloc();
    //outFileName = "test.mp4";
    isRecord = false;
    flag = true;
}

ffmpegDecode::~ffmpegDecode()
{
	avformat_free_context(o_pFormatCtx);
    avformat_free_context(pFormatCtx);
    av_frame_free(&pFrame);
    sws_freeContext(pSwsCtx);
}

bool ffmpegDecode::init()
{
    //����Ƶ��
    int err = avformat_open_input(&pFormatCtx, url.toStdString().c_str(), NULL,NULL);
    if (err < 0)
    {
        qDebug() << "Can not open this file";
        return false;
    }

    //��ȡ��Ƶ����Ϣ   //����һ���� ���ڷ���
    //if (av_find_stream_info(pFormatCtx) < 0)
    err=avformat_find_stream_info(pFormatCtx,NULL);
    if(err < 0)
    {
        qDebug() << "Unable to get stream info";
        return false;
    }

    /*ԭ���벿��*/
    //��ȡ��Ƶ������
    videoStreamIndex = -1;
    for (unsigned int i = 0; i < pFormatCtx->nb_streams; i++)
    {
        if (pFormatCtx->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO)
        {
            videoStreamIndex = i;
            break;
        }
    }
    if (videoStreamIndex == -1)
    {
        qDebug() <<"Unable to find video stream";
        return false;
    }

    //��ȡ��Ƶ���ķֱ��ʴ�С
    pCodecCtx = pFormatCtx->streams[videoStreamIndex]->codec;
    width=pCodecCtx->width;
    height=pCodecCtx->height;

    avpicture_alloc(&pAVPicture,PIX_FMT_RGB24,pCodecCtx->width,pCodecCtx->height);

    //��ȡ��Ƶ��������
    AVCodec *pCodec;
    pCodec = avcodec_find_decoder(pCodecCtx->codec_id);
    //sws_getContext()ǰ���������ֱ�Ϊԭʼͼ��Ŀ�ߺ�ͼ����ʽ��4-6ΪĿ��ͼ��Ŀ�ߺ�ͼ����ʽ�������������Ϊת��ʱʹ�õ��㷨��ת��ʱ�����filter����
    pSwsCtx = sws_getContext(width, height, PIX_FMT_YUV420P, width,height, PIX_FMT_RGB24,SWS_BICUBIC, 0, 0, 0);
    if (pCodec == NULL)
    {
        qDebug() << "Unsupported codec";
        return false;
    }
    qDebug() << QString("video size : width=%d height=%d \n").arg(pCodecCtx->width).arg(pCodecCtx->height);

    //�򿪶�Ӧ������
    if (avcodec_open2(pCodecCtx, pCodec, NULL) < 0)
    {
        qDebug() << "Unable to open codec";
        return false;
    }
    qDebug() << "initial successfully";

    return true;

}

bool ffmpegDecode::initRecord()
{
    /*�����ʼ��*/
    QString datetime = QDateTime::currentDateTime().toString("yyyy-mm-dd-hh-mm-ss");
	outFileName = datetime.append(".mp4");
    avformat_alloc_output_context2(&o_pFormatCtx, NULL, NULL, outFileName.toStdString().c_str()); //��ʼ�������Ƶ������AVFormatContext��
    if (!o_pFormatCtx) {
        printf( "Could not create output context\n");
        return false;
    }
    ofmt = o_pFormatCtx->oformat;
    for (unsigned int i = 0; i < pFormatCtx->nb_streams; i++) {
        //Create output AVStream according to input AVStream
        AVStream *in_stream = pFormatCtx->streams[i];
        AVStream *out_stream = avformat_new_stream(o_pFormatCtx, in_stream->codec->codec); //�������������AVStream��
        if (!out_stream) {
            printf( "Failed allocating output stream\n");
//            ret = AVERROR_UNKNOWN;
//            goto end;
            return false;
        }
        //Copy the settings of AVCodecContext
        if (avcodec_copy_context(out_stream->codec, in_stream->codec) < 0) {   //����������Ƶ������AVCodecContex����ֵt�������Ƶ��AVCodecContext��
            printf( "Failed to copy context from input to output stream codec context\n");
            //goto end;
        }
        out_stream->codec->codec_tag = 0;
        if (o_pFormatCtx->oformat->flags & AVFMT_GLOBALHEADER)
            out_stream->codec->flags |= CODEC_FLAG_GLOBAL_HEADER;
    }
    //Output information------------------
    av_dump_format(o_pFormatCtx, 0, outFileName.toStdString().c_str(), 1);
    //Open output file
    if (!(ofmt->flags & AVFMT_NOFILE)) {
        int ret = avio_open(&o_pFormatCtx->pb, outFileName.toStdString().c_str(), AVIO_FLAG_WRITE);  //������ļ���
        if (ret < 0) {
            printf( "Could not open output file '%s'", outFileName.toStdString().c_str());
            //goto end;
        }
    }
    //Write file header
    if (avformat_write_header(o_pFormatCtx, NULL) < 0) {  //д�ļ�ͷ������ĳЩû���ļ�ͷ�ķ�װ��ʽ������Ҫ�˺���������˵MPEG2TS����
        printf( "Error occurred when opening output file\n");
        //goto end;
    }

    qDebug() << "initial output successfully";
    return true;
}

void ffmpegDecode::setRecordState(bool state)
{
    isRecord = state;
}

void ffmpegDecode::h264Decodec()
{
    //һ֡һ֡��ȡ��Ƶ
    int frameFinished=0;
    while(av_read_frame(pFormatCtx, &packet) >= 0 && flag){
		if(packet.stream_index==videoStreamIndex)
		{
			//qDebug()<<"��ʼ����"<<QDateTime::currentDateTime().toString("HH:mm:ss zzz");
			avcodec_decode_video2(pCodecCtx, pFrame, &frameFinished, &packet);
			if (frameFinished)
			{
				printf("***************ffmpeg decodec*******************\n");
				mutex.lock();

                if(isRecord)
                {
                    //output
                    AVStream *in_stream, *out_stream;
                    in_stream  = pFormatCtx->streams[packet.stream_index];
                    out_stream = o_pFormatCtx->streams[packet.stream_index];
                    //Convert PTS/DTS
                    packet.pts = av_rescale_q_rnd(packet.pts, in_stream->time_base, out_stream->time_base, (AVRounding)(AV_ROUND_NEAR_INF|AV_ROUND_PASS_MINMAX));
                    packet.dts = av_rescale_q_rnd(packet.dts, in_stream->time_base, out_stream->time_base, (AVRounding)(AV_ROUND_NEAR_INF|AV_ROUND_PASS_MINMAX));
                    packet.duration = av_rescale_q(packet.duration, in_stream->time_base, out_stream->time_base);
                    packet.pos = -1;
                    //Write
                    if (av_interleaved_write_frame(o_pFormatCtx, &packet) < 0) { //��AVPacket���洢��Ƶѹ���������ݣ�д���ļ���
                        printf( "Error muxing packet\n");
                        break;
                    }
                }

				int rs = sws_scale(pSwsCtx, (const uint8_t* const *) pFrame->data,
					pFrame->linesize, 0,
					height, pAVPicture.data, pAVPicture.linesize);

				//���ͻ�ȡһ֡ͼ���ź�
				QImage image(pAVPicture.data[0],width,height,QImage::Format_RGB888);
				emit GetImage(image);

				mutex.unlock();
			}
		}
        av_free_packet(&packet);//�ͷ���Դ,�����ڴ��һֱ����
    }
    if(isRecord)
    {
        wFileTrailer();
    }
}

void ffmpegDecode::wFileTrailer()
{
    //Write file trailer
    av_write_trailer(o_pFormatCtx);//д�ļ�β������ĳЩû���ļ�ͷ�ķ�װ��ʽ������Ҫ�˺���������˵MPEG2TS����\
    ///* close output */
    if (o_pFormatCtx && !(ofmt->flags & AVFMT_NOFILE))
        avio_close(o_pFormatCtx->pb);
}

void ffmpegDecode::delRecord()
{
    avformat_free_context(o_pFormatCtx);
    //avformat_free_context(ofmt);

}
