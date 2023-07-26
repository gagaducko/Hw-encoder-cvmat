#pragma once

#include <iostream>
#include <string>
#include <vector>
#include <cuda.h>
#include <mutex>
#include <condition_variable>

#include <opencv2/core/mat.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/core/cuda.hpp>

extern "C" {
#include <libavformat/avformat.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>
#include <libavcodec/avcodec.h>
#include <libavutil/pixdesc.h>
#include <libavutil/hwcontext.h>
#include <libavutil/opt.h>
}

/**
 * @brief   Interface of Class Structure: HwCodecClass
 * detail   This class is used to encode video stream(vector<cv::Mat>)
 *          or just one frame(cv:Mat) based on GPU hard coding of the OpenCV data structure (cv::Mat).
 */
class HwCodecClass
{
private:
    // private varibles
    const AVCodec* pCodec;                      // Encoder
    AVCodecContext* pCodecCtx;                  // Encoder Context
    AVPacket* pkt;                              // The AVPacket encoding output
    AVFrame* frame;                             // AVFrame encoding input.
    int gpuId;                                  // GPU ID
    AVCodecID codecId;                          // Encoder type
    std::string codecName;                      // Encoder name
    int frameRate;                              // Encoded frame rate
    int bitRate;                                // Encoding bitrate
    int resolutionWidth;                        // Resolution Width
    int resolutionHeight;                       // Resolution Height
    std::string systemType;                     // System type
    std::string outputAddress;                  // Output path
    AVDictionary* param;                        // param
    int timeBaseNnum;                           // The number of each unit of time
    int timeBaseDen;                            // The denominator of the time unit, typically used to represent the frame rate of a video
    int frameRateDen;                           // The denominator portion of the frame rate, the number of frames played per second
    int gopSize;                                // the size of Group of Pictures
    int maxBFrames;                             // Maximum number of B frames
    AVPixelFormat pixFmt;                       // Pixel format
    AVFormatContext* formatContext;             // output video context
    AVBufferRef* hw_device_ctx;                 // hardware context
    AVStream* videoStream;                      // video stream
    AVPacket* packet;                           // packet
    bool isRTMP;                                // rtmp
    bool isStart = false;                       // start or not
    int countFrames = 0;                        // frames num

    // private function
    void setPCodecCtxParam();
    void initHwDeviceCtx();                    
    void initOutputCtx();
	inline void checkError(int error, const std::string& message);

public:
    // Constructor and destructor.
	HwCodecClass();
	~HwCodecClass();

    //main functions: execute functions
    void executeCvMatVector(std::vector<cv::Mat>& frames);
    void executeCvOneByOne(cv::Mat& frame);
    void getEncodedFileDown();

    // set varibles function
    void setGpuId(int gpuId);
    void setPCodecId(AVCodecID codecId);
    void setPCodecName(std::string codecName);
    void setFrameRate(int frameRate);
    void setBitRate(int bitRate);
    void setResolutionWidth(int resolutionWidth);
    void setResolutionHeight(int resolutionHeight);
    void setSystemType(std::string systemType);
    void setOutputAddress(std::string outputAddress);
    void setTimeBaseNnum(int timeBaseNnum);
    void setTimeBaseDen(int timeBaseDen);
    void setFrameRateDen(int frameRateDen);
    void setGopSize(int gopSize);
    void setMaxBFrames(int maxBFrames);
    void setPixFmt(AVPixelFormat pixFmt);
    void setRTMP(bool isRTMP);

    // show function
    void showPrivate();
};


/**
 * @brief   Interface of Class Structure: MatrixBuffer
 * detail   This class is used to save and invoke corresponding encode-class of
 *          the OpenCV data structure (cv::Mat).
 */
class MatrixBuffer
{
private:
    // private varibles
    HwCodecClass* encoder;
    std::vector<cv::Mat> my_frames;
    bool continue_flag;
    bool running;
    // for threads
    std::mutex mtx;
    std::condition_variable cond;

public:
    // Constructor and destructor.
    MatrixBuffer();
    MatrixBuffer(HwCodecClass* new_encoder);
    ~MatrixBuffer();
    // bind encoder
    void bindEncoder(HwCodecClass* new_encoder);
    // funs for add cv:Mat to my_frames
    void addCvMat(cv::Mat& frame);
    void addCvMat(std::vector<cv::Mat>& frames);
    // start encode framse until there is no frame or stop
    void startEncodeAll();
    // stop pause or restart encode frames
    void stopEncode();
    void pauseEncodeForaWhile();
    void restartFromPause();
};