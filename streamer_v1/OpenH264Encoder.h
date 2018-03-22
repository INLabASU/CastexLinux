/*
 * H264Encoder.h
 *
 *  Created on: Feb 22, 2018
 *      Author: priori
 */

#ifndef OPENH264ENCODER_H_
#define OPENH264ENCODER_H_

#include <wels/codec_def.h>
#include <wels/codec_app_def.h>
#include <wels/codec_api.h>

#include <opencv2/core/core.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/imgproc/imgproc.hpp>

class OpenH264Encoder {
public:

	OpenH264Encoder();
	virtual ~OpenH264Encoder();

	int SetUp(float maxFrameRate, int frameWidth, int frameHeight, int targetBitrate);
	void TearDown();
	void ForceIFrame();
	int EncodeFrameCV(cv::Mat rawFrame, unsigned char **buffer, int *sizes, int max_segs,
			int *frame_size_encoded, int *nr_nalu);

protected:
	ISVCEncoder* enc;

};

#endif /* OPENH264ENCODER_H_ */
