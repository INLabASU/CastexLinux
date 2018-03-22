/*
 * SimpleDecoder.h
 *
 *  Created on: Feb 22, 2018
 *      Author: priori
 */

#ifndef OPENH264DECODER_H_
#define OPENH264DECODER_H_

#include <wels/codec_def.h>
#include <wels/codec_app_def.h>
#include <wels/codec_api.h>

#include <opencv2/core/core.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/imgproc/imgproc.hpp>

class OpenH264Decoder {
public:
	OpenH264Decoder();
	virtual ~OpenH264Decoder();

	int SetUp();
	void TearDown();
	int DecodeFrameCV (const unsigned char* src, int size, cv::Mat &frame);

protected:
	ISVCDecoder* dec;
};

#endif /* OPENH264DECODER_H_ */
