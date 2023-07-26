#include "HwCodecClass.h"

/**
 * @brief print static
 * detail Print out x with enough space and ==
 * params \
 *
 * @return \
 */
void printCentered(const char* x) {
	int totalLength = 80;
	int xLength = strlen(x);
	if (xLength == 0) {
		for (int i = 0; i < (totalLength + 2); i++) {
			std::cout << "=";
		}
		std::cout << std::endl;
	}
	else {
		int leftSpaces = (totalLength - xLength - 2) / 2;
		int rightSpaces = (totalLength - xLength - 2) / 2;
		if ((totalLength - xLength - 2) % 2 != 0) {
			rightSpaces++;
		}
		std::cout << "==";
		for (int i = 0; i < leftSpaces; i++) {
			std::cout << " ";
		}
		std::cout << x;
		for (int i = 0; i < rightSpaces; i++) {
			std::cout << " ";
		}
		std::cout << "==" << std::endl;
	}
}



/**
 * @brief Constructor
 * detail Set a series of default values.
 * params \
 *
 * @return \
 */
HwCodecClass::HwCodecClass()
{
	// init ffmpeg
	av_log_set_level(AV_LOG_ERROR);
	avformat_network_init();
	// init varibles
	param = 0;															// param
	systemType = "win32";												// System type
	gpuId = 0;															// GPU ID
	codecName = "h264_nvenc";											// Encoder name
	bitRate = 4000000;													// Encoding bitrate
	outputAddress = "rtmp://127.0.0.1:1935/live/stream";                // Output path
	frameRate = 25;														// frame rate	
	timeBaseNnum = 1;													// The number of each unit of time
	timeBaseDen = 25;													// The denominator of the time unit, typically used to represent the frame rate of a video
	frameRateDen = 1;													// The denominator portion of the frame rate, the number of frames played per second
	gopSize = 25;														// the size of Group of Pictures
	maxBFrames = 0;														// Maximum number of B frames
	pixFmt = AV_PIX_FMT_YUV420P;										// Pixel format
	isRTMP = true;
	// show info
	printCentered("");
	printCentered("HwCodecClass Initialization Complete!");
	printCentered("");
	printCentered("The Initial Parameters Are As Follows");
	printCentered("");
	showPrivate();
}



/**
 * @brief destructor
 * detail destructor
 * params \
 *
 * @return \
 */
HwCodecClass::~HwCodecClass()
{
	printCentered("");
	printCentered("Destructor of HwCodecClass!");
	printCentered("");
	av_buffer_unref(&hw_device_ctx);
	printCentered("Destruct Hw_device_ctx Succ!");
	printCentered("");
	av_packet_free(&packet);
	printCentered("Destruct packet Succ!");
	printCentered("");
	avcodec_free_context(&pCodecCtx);
	printCentered("Destruct pCodecCtx Succ!");
	printCentered("");
	avformat_free_context(formatContext);
	printCentered("Destruct formatContext Succ!");
	avformat_network_deinit();
	printCentered("Avformat_network_deinit Succ!");
	printCentered("");
}





/**
 * @brief execute many frames to be encoded
 * @detail main functions in the class, use it to encode many frames(vector<cv::mat>)
 * param: std::vector<cv::Mat>& frames
 *
 * @return \
 */
void HwCodecClass::executeCvMatVector(std::vector<cv::Mat>& frames)
{
	// init all
	if (isStart == false) {
		this->isStart = true;
		countFrames = 0;
		// set height and width
		printCentered("");
		printCentered("First Time To Execute!");
		printCentered("");
		std::string result = "frame height is: " + std::to_string(frames[0].rows);
		printCentered(result.c_str());
		result = "frame width is: " + std::to_string(frames[0].cols);
		printCentered(result.c_str());
		setResolutionHeight(frames[0].rows);
		setResolutionWidth(frames[0].cols);
		printCentered("Init Output Context!");
		// init_output
		initOutputCtx();
		printCentered("Init HwDevice Context!");
		//set hw_device
		initHwDeviceCtx();
		printCentered("First Init Down!");
		printCentered("");
	}
	// encode
	int error;
	for (const cv::Mat& frame : frames) {
		// convert the cv::Mat to an AVFrame
		AVFrame* avFrame = av_frame_alloc();
		avFrame->format = pCodecCtx->pix_fmt;
		avFrame->width = resolutionWidth;
		avFrame->height = resolutionHeight;
		error = av_frame_get_buffer(avFrame, 0);
		checkError(error, "Error allocating frame buffer");
		struct SwsContext* frameConverter = sws_getContext(resolutionWidth, resolutionHeight, AV_PIX_FMT_BGR24, resolutionWidth, resolutionHeight, pCodecCtx->pix_fmt, SWS_BICUBIC, nullptr, nullptr, nullptr);
		uint8_t* srcData[AV_NUM_DATA_POINTERS] = { frame.data };
		int srcLinesize[AV_NUM_DATA_POINTERS] = { static_cast<int>(frame.step) };
		sws_scale(frameConverter, srcData, srcLinesize, 0, resolutionHeight, avFrame->data, avFrame->linesize);
		sws_freeContext(frameConverter);
		// encode the AVFrame
		avFrame->pts = this->countFrames * (videoStream->time_base.den) / ((videoStream->time_base.num) * frameRate);
		this->countFrames += 1;
		error = avcodec_send_frame(pCodecCtx, avFrame);
		checkError(error, "Error sending frame to video codec");
		while (error >= 0) {
			error = avcodec_receive_packet(pCodecCtx, packet);
			if (error == AVERROR(EAGAIN) || error == AVERROR_EOF) {
				break;
			}
			checkError(error, "Error encoding video frame");
			// write the encoded packet to the output file
			packet->stream_index = videoStream->index;
			error = av_interleaved_write_frame(formatContext, packet);
			checkError(error, "Error writing video packet");
			av_packet_unref(packet);
		}
		av_frame_free(&avFrame);
	}
}




/**
 * @brief executeCvOneByOne
 * @detail main functions in the class, use it to encode many frames(cv::mat), but one by one
 * param: std::vector<cv::Mat>& frames
 *
 * @return \
 */
void HwCodecClass::executeCvOneByOne(cv::Mat& frame) 
{
	// init all
	if (isStart == false) {
		this->isStart = true;
		countFrames = 0;
		// set height and width
		printCentered("");
		printCentered("First Time To Execute!");
		printCentered("");
		std::string result = "frame height is: " + std::to_string(frame.rows);
		printCentered(result.c_str());
		result = "frame width is: " + std::to_string(frame.cols);
		printCentered(result.c_str());
		setResolutionHeight(frame.rows);
		setResolutionWidth(frame.cols);
		printCentered("Init Output Context!");
		// init_output
		initOutputCtx();
		printCentered("Init HwDevice Context!");
		//set hw_device
		initHwDeviceCtx();
		printCentered("First Init Down!");
		printCentered("");
	}
	// encode
	// convert the cv::Mat to an AVFrame
	AVFrame* avFrame = av_frame_alloc();
	avFrame->format = pCodecCtx->pix_fmt;
	avFrame->width = resolutionWidth;
	avFrame->height = resolutionHeight;
	int error = av_frame_get_buffer(avFrame, 0);
	checkError(error, "Error allocating frame buffer");
	struct SwsContext* frameConverter = sws_getContext(resolutionWidth, resolutionHeight, AV_PIX_FMT_BGR24, resolutionWidth, resolutionHeight, pCodecCtx->pix_fmt, SWS_BICUBIC, nullptr, nullptr, nullptr);
	uint8_t* srcData[AV_NUM_DATA_POINTERS] = { frame.data };
	int srcLinesize[AV_NUM_DATA_POINTERS] = { static_cast<int>(frame.step) };
	sws_scale(frameConverter, srcData, srcLinesize, 0, resolutionHeight, avFrame->data, avFrame->linesize);
	sws_freeContext(frameConverter);
	// encode the AVFrame
	avFrame->pts = this->countFrames * (videoStream->time_base.den) / ((videoStream->time_base.num) * frameRate);
	this->countFrames += 1;
	error = avcodec_send_frame(pCodecCtx, avFrame);
	checkError(error, "Error sending frame to video codec");
	while (error >= 0) {
		error = avcodec_receive_packet(pCodecCtx, packet);
		if (error == AVERROR(EAGAIN) || error == AVERROR_EOF) {
			break;
		}
		checkError(error, "Error encoding video frame");
		// write the encoded packet to the output file
		packet->stream_index = videoStream->index;
		error = av_interleaved_write_frame(formatContext, packet);
		checkError(error, "Error writing video packet");
		av_packet_unref(packet);
	}
	av_frame_free(&avFrame);
}



/**
 * @brief getEncodedFile
 * @detail after encode many cv::mat, can use this function to get the file after encoded
 * param: std::vector<cv::Mat>& frames
 *
 * @return \
 */
void HwCodecClass::getEncodedFileDown()
{
	// flush the rest of the packets
	int ret;
	avcodec_send_frame(pCodecCtx, nullptr);
	do
	{
		av_packet_unref(packet);
		ret = avcodec_receive_packet(pCodecCtx, packet);
		if (!ret)
		{
			int error = av_interleaved_write_frame(formatContext, packet);
			checkError(error, "Error writing video packet");
		}
	} while (!ret);
	printCentered("finished get it!");
	printCentered("");
	av_write_trailer(formatContext);
	avformat_close_input(&formatContext);
}



/**
 * @brief checkError function private
 * @detail helper function to check error of FFMPEG
 * param1: int error
 * param2: const std::string& message
 *
 * @return \
 */
inline void HwCodecClass::checkError(int error, const std::string& message)
{
    if (error < 0) {
        std::cerr << message << ": " << std::to_string(error) << std::endl;
        exit(EXIT_FAILURE);
    }
}



/**
 * @brief initHwDeviceCtx private
 * @detail init hw device ctx, choose which gpu to use
 * param: \
 *
 * @return \
 */
void HwCodecClass::initHwDeviceCtx()
{
	hw_device_ctx = nullptr;
	AVHWDeviceType hw_device_type = AV_HWDEVICE_TYPE_CUDA;
	const char* hw_device = std::to_string(gpuId).c_str();  // Specify the index number or name of the GPU device to be used.
	int ret = av_hwdevice_ctx_create(&hw_device_ctx, hw_device_type, hw_device, nullptr, 0);
	if (ret < 0) {
		std::cerr << "Failed to create hardware device context." << std::endl;
		return;
	}
	std::string result = "Using hardware device context";
	printCentered(result.c_str());
	pCodecCtx->hw_device_ctx = av_buffer_ref(hw_device_ctx);
	return;
}



/**
 * @brief initOutputCtx
 * detail Initialize a series of variables that will be used for encoding later.
 * params \
 *
 * @return \
 */
void HwCodecClass::initOutputCtx()
{
	// create output video context
	formatContext = nullptr;
	int error;
	if (this->isRTMP == false) {
		error = avformat_alloc_output_context2(&formatContext, nullptr, nullptr, outputAddress.c_str());
	} else {
		error = avformat_alloc_output_context2(&formatContext, nullptr, "flv", outputAddress.c_str());
	}
	checkError(error, "Error creating output context");
	// create the video stream
	videoStream = avformat_new_stream(formatContext, nullptr);
	if (!videoStream) {
		std::cerr << "Error creating video stream" << std::endl;
		exit(EXIT_FAILURE);
	}
	// create video codec and context
	pCodec = avcodec_find_encoder_by_name(codecName.c_str());
	if (!pCodec) {
		std::cerr << "Error allocating video codec" << std::endl;
		exit(EXIT_FAILURE);
	}
	pCodecCtx = avcodec_alloc_context3(pCodec);
	if (!pCodecCtx) {
		std::cerr << "Error allocating video codec context" << std::endl;
		exit(EXIT_FAILURE);
	}
	// init params
	setPCodecCtxParam();
	if (formatContext->oformat->flags & AVFMT_GLOBALHEADER) {
		pCodecCtx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
	}
	// open
	error = avcodec_open2(pCodecCtx, pCodec, &param);
	checkError(error, "Error opening");
	error = avcodec_parameters_from_context(videoStream->codecpar, pCodecCtx);
	checkError(error, "Error setting video codec parameters");
	// open the output file
	error = avio_open(&formatContext->pb, outputAddress.c_str(), AVIO_FLAG_WRITE);
	checkError(error, "Error opening output file");
	// write the video file header
	error = avformat_write_header(formatContext, nullptr);
	checkError(error, "Error writing video file header");
	// alloc pkt
	packet = av_packet_alloc();
	if (!packet) {
		std::cerr << "Error allocating packet" << std::endl;
		exit(EXIT_FAILURE);
	}
}



/**
 * @brief setPCodecCtxParam function private
 * @detail set the parameters of pCodeCtx
 * param: no
 *
 * @return \
 */
void HwCodecClass::setPCodecCtxParam()
{
	pCodecCtx->bit_rate = bitRate;
	pCodecCtx->pix_fmt = AV_PIX_FMT_YUV420P;
	pCodecCtx->width = resolutionWidth;
	pCodecCtx->height = resolutionHeight;
	pCodecCtx->framerate.num = frameRate;
	pCodecCtx->framerate.den = frameRateDen;
	pCodecCtx->time_base.num = timeBaseNnum;
	pCodecCtx->time_base.den = timeBaseDen;
	pCodecCtx->gop_size = gopSize;
	pCodecCtx->max_b_frames = maxBFrames;
	param = 0;
	av_dict_set(&param, "preset", "medium", 0);
	//av_dict_set(&param, "tune", "zerolatency", 0);
}



/**
 * @brief setGpuId function public
 * @detail setGpuId in order to choose which gpu to use
 * param: int gpuId		which gpu to choose
 *
 * @return \
 */
void HwCodecClass::setGpuId(int gpuId)
{
	this->gpuId = gpuId;
	return;
}



/**
 * @brief setPCodec function public
 * @detail setPCodec in order to find the encoder which we need
 * param: AVCodecID codecId		enum encoderId
 *
 * @return \
 */
void HwCodecClass::setPCodecId(AVCodecID codecId)
{
	this->codecId = codecId;
	return;
}



/**
 * @brief setPCodecName function public
 * @detail setPCodecName in order to find the encoder which we need
 * param: std::string codecName use name to find encoder
 *
 * @return \
 */
void HwCodecClass::setPCodecName(std::string codecName)
{
	this->codecName = codecName;
	return;
}



/**
 * @brief setFrameRate function public
 * @detail setFrameRate in order to set encoded frame rate
 * param: int frameRate
 *
 * @return \
 */
void HwCodecClass::setFrameRate(int frameRate)
{
	this->frameRate = frameRate;
	return;
}



/**
 * @brief setBitRate function public
 * @detail setBitRate in order to set encoded bit rate
 * param: int bitRate
 *
 * @return \
 */
void HwCodecClass::setBitRate(int bitRate)
{
	this->bitRate = bitRate;
	return;
}



/**
 * @brief setResolutionWidth function public
 * @detail setResolutionWidth in order to set encoded resolution width
 * param: int resolutionWidth
 *
 * @return \
 */
void HwCodecClass::setResolutionWidth(int resolutionWidth)
{
	this->resolutionWidth = resolutionWidth;
	return;
}



/**
 * @brief setResolutionHeight function public
 * @detail setResolutionHeight in order to set encoded resolution height
 * param: int resolutionHeight
 *
 * @return \
 */
void HwCodecClass::setResolutionHeight(int resolutionHeight)
{
	this->resolutionHeight = resolutionHeight;
	return;
}



/**
 * @brief setSystemType function public
 * @detail setSystemType in order to choose some functions which can only use in the direct system
 * param: std::string systemType
 *
 * @return \
 */
void HwCodecClass::setSystemType(std::string systemType)
{
	this->systemType = systemType;
	return;
}



/**
 * @brief setTimeBaseNnum function public
 * @detail set the timeBaseNnum
 * param: int timeBaseNnum
 *
 * @return \
 */
void HwCodecClass::setTimeBaseNnum(int timeBaseNnum)
{
	this->timeBaseNnum = timeBaseNnum;
}



/**
 * @brief setTimeBaseDen function public
 * @detail set the timeBaseDen
 * param: int timeBaseDen
 *
 * @return \
 */
void HwCodecClass::setTimeBaseDen(int timeBaseDen)
{
	this->timeBaseDen = timeBaseDen;
}



/**
 * @brief setFrameRateDen function public
 * @detail set the frameRateDen
 * param: int frameRateDen
 *
 * @return \
 */
void HwCodecClass::setFrameRateDen(int frameRateDen)
{
	this->frameRateDen = frameRateDen;
}



/**
 * @brief setGopSize function public
 * @detail set the gopSize
 * param: int gopSize
 *
 * @return \
 */
void HwCodecClass::setGopSize(int gopSize)
{
	this->gopSize = gopSize;
}



/**
 * @brief setMaxBFrames function public
 * @detail set the MaxBFrames
 * param: int maxBFrames
 *
 * @return \
 */
void HwCodecClass::setMaxBFrames(int maxBFrames)
{
	this->maxBFrames = maxBFrames;
}



/**
 * @brief setPixFmt function public
 * @detail set the Pix-Fmt
 * param: AVPixelFormat pixFmt
 *
 * @return \
 */
void HwCodecClass::setPixFmt(AVPixelFormat pixFmt)
{
	this->pixFmt = pixFmt;
}



/**
 * @brief setOutputAddress function public
 * @detail set the output path
 * param: std::string outputAddress
 *
 * @return \
 */
void HwCodecClass::setOutputAddress(std::string outputAddress)
{
	this->outputAddress = outputAddress;
	return;
}

/**
 * @brief setRTMP function public
 * @detail set the rtmp or file
 * param: \
 *
 * @return \
 */
void HwCodecClass::setRTMP(bool isRTMP) {
	this->isRTMP = isRTMP;
	return;
}


/**
 * @brief showPrivate
 * @detail show all varibles
 * param: \
 *
 * @return \
 */
void HwCodecClass::showPrivate()
{
	printCentered(""); 
	printCentered("Show Private Varibles Below");
	printCentered("");
	std::string result = "gpuId is: " + std::to_string(gpuId);
	printCentered(result.c_str());
	result = "codecName is: " + codecName;
	printCentered(result.c_str());
	result = "bitRate is: " + std::to_string(bitRate);
	printCentered(result.c_str());
	result = "outputAddress is: " + outputAddress;
	printCentered(result.c_str());
	result = "frameRate is: " + std::to_string(frameRate);
	printCentered(result.c_str());
	result = "frameRateDen is: " + std::to_string(frameRateDen);
	printCentered(result.c_str());
	result = "timeBaseNnum is: " + std::to_string(timeBaseNnum);
	printCentered(result.c_str());
	result = "timeBaseDen is: " + std::to_string(timeBaseDen);
	printCentered(result.c_str());
	result = "gopSize is: " + std::to_string(gopSize);
	printCentered(result.c_str());
	result = "maxBFrames is: " + std::to_string(maxBFrames);
	printCentered(result.c_str());
	result = "pixFmt is: " + std::to_string(pixFmt);
	printCentered(result.c_str());
	printCentered("");
	if (this->isRTMP == true) {
		printCentered("now is rtmp");
		printCentered("");
	}
	else {
		printCentered("now is file");
		printCentered("");
	}
	printCentered("");
	printCentered("if you want to change these varibles");
	printCentered("you can use set. functions to set this varibles");
	printCentered("");
}





/**
 * @brief Constructor
 * detail Set a series of default values.
 * params \
 *
 * @return \
 */
MatrixBuffer::MatrixBuffer()
{
	encoder = nullptr;
	my_frames.clear();
	continue_flag = true;
	running = true;
}





/**
 * @brief Constructor
 * detail Set a series of default values.
 * params: HwCodecClass *new_encoder
 *
 * @return \
 */
MatrixBuffer::MatrixBuffer(HwCodecClass* new_encoder)
{
	encoder = new_encoder;
	my_frames.clear();
	continue_flag = true;
	running = true;
}





/**
 * @brief destructor
 * detail destructor
 * params \
 *
 * @return \
 */
MatrixBuffer::~MatrixBuffer()
{
	printCentered("");
	printCentered("MatrixBuffer Destructor");
	printCentered("");
}

/**
 * @brief add frame function
 * add one frame to my_frame
 * params cv::Mat& frame
 *
 * @return \
 */
void MatrixBuffer::addCvMat(cv::Mat& frame)
{
	std::unique_lock<std::mutex> lck(mtx);
	my_frames.push_back(frame);
}





/**
 * @brief add frames function
 * detail add frames to my_frame
 * params std::vector<cv::Mat>& frames
 *
 * @return \
 */
void MatrixBuffer::addCvMat(std::vector<cv::Mat>& frames)
{

	std::unique_lock<std::mutex> lck(mtx);
	my_frames.insert(my_frames.end(), frames.begin(), frames.end());
}





/**
 * @brief start encode my_frames
 * detail invoke encoder's function to encode frames
 * params \
 *
 * @return \
 */
void MatrixBuffer::startEncodeAll()
{
	while (running)
	{
		if (!continue_flag) {
			std::unique_lock<std::mutex> lck(mtx);
			cond.wait(lck, [this]() {return continue_flag; });
		}
		if (!my_frames.empty()) {
			std::unique_lock<std::mutex> lck(mtx);
			auto it = my_frames.begin();
			auto frame = *it;
			my_frames.erase(it);
			lck.unlock();
			encoder->executeCvOneByOne(frame);
		}
	}
}





/**
 * @brief stop encode
 * detail set flag to false so that stop encode
 * params \
 *
 * @return \
 */
void MatrixBuffer::stopEncode()
{
	running = false;
	continue_flag = true;
	cond.notify_one();
}




/**
 * @brief pause encode
 * detail set continue_flag to false
 * params \
 *
 * @return \
 */
void MatrixBuffer::pauseEncodeForaWhile()
{
	continue_flag = false;
}





/**
 * @brief restart encode
 * detail set continue_flag to true and notify encode
 * params \
 *
 * @return \
 */
void MatrixBuffer::restartFromPause()
{
	continue_flag = true;
	cond.notify_one();
}





/**
 * @brief bind HwCodecClass
 * detail set endcode to new_encoder
 * params \
 *
 * @return \
 */
void MatrixBuffer::bindEncoder(HwCodecClass* new_encoder)
{
	encoder = new_encoder;
}