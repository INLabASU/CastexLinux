/*
 * SimpleEncoder.cpp
 *
 *  Created on: Feb 22, 2018
 *      Author: priori
 */

#include <iostream>

#include <stdio.h>
#include "OpenH264Encoder.h"

using namespace std;



OpenH264Encoder::OpenH264Encoder() {

	enc = NULL;

}

OpenH264Encoder::~OpenH264Encoder() {

	enc = NULL;

}

static int InitWithParam(ISVCEncoder* encoder, SEncParamExt* pEncParamExt) {

	SliceModeEnum eSliceMode =
			pEncParamExt->sSpatialLayers[0].sSliceArgument.uiSliceMode;
	bool bBaseParamFlag =
			(SM_SINGLE_SLICE == eSliceMode && !pEncParamExt->bEnableDenoise
					&& pEncParamExt->iSpatialLayerNum == 1
					&& !pEncParamExt->bIsLosslessLink
					&& !pEncParamExt->bEnableLongTermReference
					&& !pEncParamExt->iEntropyCodingModeFlag) ? true : false;

	if (bBaseParamFlag) {

		SEncParamBase param;
		memset(&param, 0, sizeof(SEncParamBase));

		param.iUsageType = pEncParamExt->iUsageType;
		param.fMaxFrameRate = pEncParamExt->fMaxFrameRate;
		param.iPicWidth = pEncParamExt->iPicWidth;
		param.iPicHeight = pEncParamExt->iPicHeight;
		param.iTargetBitrate = 5000000;
		return encoder->Initialize(&param);

	} else {

		SEncParamExt param;
		encoder->GetDefaultParams(&param);

		param.iUsageType = pEncParamExt->iUsageType;
		param.fMaxFrameRate = pEncParamExt->fMaxFrameRate;
		param.iPicWidth = pEncParamExt->iPicWidth;
		param.iPicHeight = pEncParamExt->iPicHeight;
		param.iTargetBitrate = 5000000;
		param.bEnableDenoise = pEncParamExt->bEnableDenoise;
		param.iSpatialLayerNum = pEncParamExt->iSpatialLayerNum;
		param.bIsLosslessLink = pEncParamExt->bIsLosslessLink;
		param.bEnableLongTermReference = pEncParamExt->bEnableLongTermReference;
		param.iEntropyCodingModeFlag =
				pEncParamExt->iEntropyCodingModeFlag ? 1 : 0;
		if (eSliceMode != SM_SINGLE_SLICE && eSliceMode != SM_SIZELIMITED_SLICE)
			param.iMultipleThreadIdc = 2;

		for (int i = 0; i < param.iSpatialLayerNum; i++) {
			param.sSpatialLayers[i].iVideoWidth = pEncParamExt->iPicWidth
					>> (param.iSpatialLayerNum - 1 - i);
			param.sSpatialLayers[i].iVideoHeight = pEncParamExt->iPicHeight
					>> (param.iSpatialLayerNum - 1 - i);
			param.sSpatialLayers[i].fFrameRate = pEncParamExt->fMaxFrameRate;
			param.sSpatialLayers[i].iSpatialBitrate = param.iTargetBitrate;

			param.sSpatialLayers[i].sSliceArgument.uiSliceMode = eSliceMode;
			if (eSliceMode == SM_SIZELIMITED_SLICE) {
				param.sSpatialLayers[i].sSliceArgument.uiSliceSizeConstraint =
						600;
				param.uiMaxNalSize = 1500;
				param.iMultipleThreadIdc = 4;
				param.bUseLoadBalancing = false;
			}
			if (eSliceMode == SM_FIXEDSLCNUM_SLICE) {
				param.sSpatialLayers[i].sSliceArgument.uiSliceNum = 4;
				param.iMultipleThreadIdc = 4;
				param.bUseLoadBalancing = false;
			}
			if (param.iEntropyCodingModeFlag) {
				param.sSpatialLayers[i].uiProfileIdc = PRO_MAIN;
			}
		}

		param.iTargetBitrate *= param.iSpatialLayerNum;
		return encoder->InitializeExt(&param);

	}
}


int OpenH264Encoder::SetUp(float maxFrameRate, int frameWidth, int frameHeight, int targetBitrate) {

	int rv = WelsCreateSVCEncoder(&enc);

	unsigned int uiTraceLevel = WELS_LOG_ERROR;
	enc->SetOption(ENCODER_OPTION_TRACE_LEVEL, &uiTraceLevel);

	//rv = InitWithParam(enc, pEncParamExt);

	SEncParamBase param;
	memset (&param, 0, sizeof (SEncParamBase));
	param.iUsageType = CAMERA_VIDEO_REAL_TIME;
	param.fMaxFrameRate = maxFrameRate;
	param.iPicWidth = frameWidth;
	param.iPicHeight = frameHeight;
	param.iTargetBitrate = targetBitrate;

	enc->Initialize(&param);

	int videoFormat = videoFormatI420;
	enc->SetOption(ENCODER_OPTION_DATAFORMAT, &videoFormat);

	return 0;
}

void OpenH264Encoder::TearDown() {

	if (enc) {
		enc->Uninitialize();
		WelsDestroySVCEncoder(enc);
	}

}

char *frame_type_str[] = {
		"Invalid",
		"IDR",
		"I",
		"P",
		"Skip",
		"IPMixed"
};



void OpenH264Encoder::ForceIFrame() {

	enc->ForceIntraFrame(true);
}

int OpenH264Encoder::EncodeFrameCV(cv::Mat rawFrame, unsigned char **buffer,
		int *sizes, int max_segs, int *frame_size_encoded, int *nr_nalu) {

	int width = rawFrame.cols;
	int height = rawFrame.rows;

	// TODO: check width and height with pEncParamExt at setup

	cv::Mat YUV;
	cv::cvtColor(rawFrame, YUV, CV_RGB2YUV);

	cv::Mat channels[3];

	cv::split(YUV, channels);

	cv::Mat Y = channels[0];
	cv::Mat U = channels[1];
	cv::Mat V = channels[2];

	cv::resize(U, U, cv::Size(width / 2, height / 2));
	cv::resize(V, V, cv::Size(width / 2, height / 2));


	SFrameBSInfo info;
	memset(&info, 0, sizeof(SFrameBSInfo));

	SSourcePicture pic;

	memset(&pic, 0, sizeof(SSourcePicture));
	pic.iPicWidth = width;
	pic.iPicHeight = height;
	pic.iColorFormat = videoFormatI420;
	pic.iStride[0] = pic.iPicWidth;
	pic.iStride[1] = pic.iPicWidth / 2;
	pic.iStride[2] = pic.iPicWidth / 2;
	pic.pData[0] = Y.data;
	pic.pData[1] = U.data;
	pic.pData[2] = V.data;


	int rv = enc->EncodeFrame(&pic, &info);

	int nalu_idx = 0;
	int frame_size = 0;
	if (rv == cmResultSuccess) {

		frame_size = info.iFrameSizeInBytes;

		for (int i = 0; i < info.iLayerNum; i++) {

			PLayerBSInfo layer = &info.sLayerInfo[i];

//			cout << "{layer: " << i << " (" << frame_type_str[layer->eFrameType] << ")";

			int offset = 0;
			for (int j = 0; j < layer->iNalCount; j++) {

//				cout << "<" << layer->pNalLengthInByte[j] << ": ";
//				unsigned char*  pBsBuf = layer->pBsBuf + offset;
//				printf("%02X %02X %02X %02X %02X %02X ",
//						pBsBuf[0], pBsBuf[1], pBsBuf[2], pBsBuf[3], pBsBuf[4], pBsBuf[5]);
//				cout << ">, ";

				if (nalu_idx < max_segs) {
					buffer[nalu_idx] = layer->pBsBuf + offset;
					sizes[nalu_idx] = layer->pNalLengthInByte[j];
					nalu_idx++;
				}
				offset += layer->pNalLengthInByte[i];
			}

//			cout << "}\t";
		}

//		cout << endl;
	}

	*frame_size_encoded = frame_size;
	*nr_nalu = nalu_idx;

	return nalu_idx;
}













