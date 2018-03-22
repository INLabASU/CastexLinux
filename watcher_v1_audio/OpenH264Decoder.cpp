/*
 * SimpleDecoder.cpp
 *
 *  Created on: Feb 22, 2018
 *      Author: priori
 */

#include <iostream>

#include <stdio.h>
#include "OpenH264Decoder.h"

using namespace std;


OpenH264Decoder::OpenH264Decoder() {

	dec = NULL;

}

OpenH264Decoder::~OpenH264Decoder() {

	dec = NULL;
}

int OpenH264Decoder::SetUp() {

	int rv = WelsCreateDecoder(&dec);

	if (!dec) {
		return rv;
	}

	SDecodingParam decParam;
	memset(&decParam, 0, sizeof(SDecodingParam));
	decParam.uiTargetDqLayer = UCHAR_MAX;
	decParam.eEcActiveIdc = ERROR_CON_SLICE_COPY;
	decParam.sVideoProperty.eVideoBsType = VIDEO_BITSTREAM_AVC;

	rv = dec->Initialize(&decParam);

	return rv;
}

void OpenH264Decoder::TearDown() {

	if (dec != NULL) {
		dec->Uninitialize();
		WelsDestroyDecoder(dec);
	}
	dec = NULL;

}

int OpenH264Decoder::DecodeFrameCV(const unsigned char* src, int size,
		cv::Mat &frame) {

	uint8_t* data[3];
	SBufferInfo bufInfo;
	memset(data, 0, sizeof(data));
	memset(&bufInfo, 0, sizeof(SBufferInfo));

	DECODING_STATE rv = dec->DecodeFrame2(src, size, data, &bufInfo);

	if (bufInfo.iBufferStatus == 1) {

		int width = bufInfo.UsrData.sSystemBuffer.iWidth;
		int height = bufInfo.UsrData.sSystemBuffer.iHeight;
		int stride0 = bufInfo.UsrData.sSystemBuffer.iStride[0];
		int stride1 = bufInfo.UsrData.sSystemBuffer.iStride[1];

		cv::Mat dfY(height, width, CV_8UC1, data[0], stride0);
		cv::Mat dfU(height / 2, width / 2, CV_8UC1, data[1], stride1);
		cv::Mat dfV(height / 2, width / 2, CV_8UC1, data[2], stride1);

		cv::resize(dfU, dfU, cv::Size(width, height));
		cv::resize(dfV, dfV, cv::Size(width, height));

		vector<cv::Mat> vec;
		vec.push_back(dfY);
		vec.push_back(dfU);
		vec.push_back(dfV);

		cv::Mat df;
		merge(vec, df);

		cvtColor(df, df, CV_YUV2RGB);

		frame = df;

//		cout << width << ", " << height << ", " << stride0 << ", " << stride1
//				<< endl;

		return 1;
	} else {

		printf("decoder: 0x%X\n", rv);
	}

	return 0;

}














