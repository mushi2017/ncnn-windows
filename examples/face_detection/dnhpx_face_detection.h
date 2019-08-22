#ifndef _FACE_DETECT_ENGINE_HPP_
#define _FACE_DETECT_ENGINE_HPP_

#ifndef _WIN32
#define __stdcall
#endif

#include "dnhpx_utility.h"

#ifdef __cplusplus
extern "C" {
#endif
    // Set Module Path
    int __stdcall DNHPXSetFaceDetectLibPath(const char* szLibPath);

    // Initialized
    int __stdcall DNHPXInitFaceDetect(DNHPXFaceDetHandle* pHandle, const char* model_name = NULL);

    // Detect max face
    int __stdcall DNHPXMaxFaceDetect(DNHPXFaceDetHandle handle, const unsigned char* image_data,
        int image_width, int image_height, DNHPXFaceRect& face_box, 
        const int min_size = 40, const int num_threads = 4);

    // Detect all face
    int __stdcall DNHPXFaceDetect(DNHPXFaceDetHandle handle, const unsigned char* image_data,
        int image_width, int image_height, std::vector<DNHPXFaceRect> &face_box, 
        const int min_size = 40, 
        const int num_threads = 4);

    // Release
    int __stdcall DNHPXUninitFaceDetect(DNHPXFaceDetHandle handle);

    /*
     *	DNHPXFaceBuffering������������ĥƤ����
     *	�������:
     *		input_image[in]: ����ͼ��
     *		face_box[in]: �����Ӧͼ���������Ϣ
     *		output_image[out]: ���ͼ��
     *      param[in]: �˲�����������ʹ��Ĭ��ֵ
     *	����ֵ: int (�μ�dnhpx_error_code.h�е�error code����)
     */
    int __stdcall DNHPXFaceBuffering(const unsigned char* input_image,
        int image_width, int image_height, std::vector<DNHPXFaceRect>& face_box,
        unsigned char* output_image, dnhpx::FaceBufferingParam param = dnhpx::FaceBufferingParam());

#ifdef __cplusplus
}
#endif

#endif
