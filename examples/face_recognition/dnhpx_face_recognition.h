#pragma once

#ifndef _WIN32
#define __stdcall
#endif

#include "dnhpx_structure.h"

#ifdef __cplusplus
extern "C"
{
#endif	//	__cplusplus


	/**
	 *	\brief set feature extraction library path
	 *		\param[in] szLibPath library path name
	 *	\return int error code defined in THIDErrorDef.h
	 */
	int __stdcall SetFaceRecognitionLibPath(const char *szLibPath);

	/**
	 *	\brief initialize deep face feature extraction sdk	 
	 *	\return int error code defined in THIDErrorDef.h	 
	 */
    int __stdcall InitFaceRecognition(const char *szResName,
        DNHPXFaceRecogHandle*pHandle, int num_threads = 1, bool light_mode = true);

    /**
    *	\brief initialize deep face feature extraction sdk
    *	\return int error code defined in THIDErrorDef.h
    */
    int __stdcall InitOLDFaceRecognition(const char *szParamName,
        const char *szBinName, DNHPXFaceRecogHandle*pHandle,
        int num_threads = 1, bool light_mode = true);
	

	/**
	 *	\brief free deep face feature extraction sdk
	 *	\return int error code defined in THIDErrorDef.h
	 */
	int __stdcall UninitFaceRecognition(DNHPXFaceRecogHandle handle);

	/**
	 *	\brief get deep face feature size in bytes
	 *	\return int face feature size in bytes
	 */
    /*int __stdcall GetDeepFeatSize(BeautyHandle handle);*/
    /**
    *	���ܣ� ��ȡ��ֵ����
    *	���룺BeautyHandle ģ��ָ��
    *         feaPoints ����ؼ���
    *         image_data raw��ʽ��ͼ�����ݣ�����RGB����
    *         width ͼ��Ŀ�
    *         height ͼ��ĸ�
    *         channel ͼ���ͨ����
    *   �����
    *         pFeatures ���ص���ֵ����
    *         fea_dim ���ص�pFeatures�ĳ���
    */
    int __stdcall GetFaceRecognitionFeature(DNHPXFaceRecogHandle handle,
        const float *feaPoints, const unsigned char *image_data, int width, 
        int height, int channel, float **feature, int &fea_dim);

    /**
    *	���ܣ� ��ȡ��������
    *	���룺BeautyHandle ģ��ָ��
    *         feaPoints ����ؼ���
    *         norm_data ��һ���õ�����ͼ�񣬿���ֱ�ӽ�cv::mat�е�image.data����
    *   �����
    *         pFeatures ���ص�����
    *         fea_dim ���ص�pFeatures�ĳ���
    */
    int __stdcall GetFaceRecognitionFeatureRaw(DNHPXFaceRecogHandle handle,
        const unsigned char *norm_data, float **feature, int &fea_dim);
#ifdef __cplusplus
}
#endif
