// Tencent is pleased to support the open source community by making ncnn available.
//
// Copyright (C) 2017 THL A29 Limited, a Tencent company. All rights reserved.
//
// Licensed under the BSD 3-Clause License (the "License"); you may not use this file except
// in compliance with the License. You may obtain a copy of the License at
//
// https://opensource.org/licenses/BSD-3-Clause
//
// Unless required by applicable law or agreed to in writing, software distributed
// under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
// CONDITIONS OF ANY KIND, either express or implied. See the License for the
// specific language governing permissions and limitations under the License.

#include "deconvolution_arm.h"
#include "layer_type.h"

#if __ARM_NEON
#include <arm_neon.h>
#include "neon_mathfun.h"
#endif // __ARM_NEON

namespace ncnn {

#include "deconvolution_4x4.h"
#include "deconvolution_3x3.h"

DEFINE_LAYER_CREATOR(Deconvolution_arm)

Deconvolution_arm::Deconvolution_arm()
{
#if __ARM_NEON
    support_packing = true;
#endif // __ARM_NEON

    activation = 0;
}

int Deconvolution_arm::create_pipeline(const Option& opt)
{
    if (activation_type == 1)
    {
        activation = ncnn::create_layer(ncnn::LayerType::ReLU);

        ncnn::ParamDict pd;
        activation->load_param(pd);
    }
    else if (activation_type == 2)
    {
        activation = ncnn::create_layer(ncnn::LayerType::ReLU);

        ncnn::ParamDict pd;
        pd.set(0, activation_params[0]);// slope
        activation->load_param(pd);
    }
    else if (activation_type == 3)
    {
        activation = ncnn::create_layer(ncnn::LayerType::Clip);

        ncnn::ParamDict pd;
        pd.set(0, activation_params[0]);// min
        pd.set(1, activation_params[1]);// max
        activation->load_param(pd);
    }
    else if (activation_type == 4)
    {
        activation = ncnn::create_layer(ncnn::LayerType::Sigmoid);

        ncnn::ParamDict pd;
        activation->load_param(pd);
    }

    if (activation)
    {
        Option opt_cpu = opt;
        opt_cpu.use_vulkan_compute = false;
        activation->create_pipeline(opt_cpu);
    }

    const int maxk = kernel_w * kernel_h;
    int num_input = weight_data_size / maxk / num_output;

#if __ARM_NEON
    if (opt.use_packing_layout)
    {

    Mat weight_data_transposed(weight_data.w);
    {
        float* pt = weight_data_transposed;
        const float* p = weight_data;

        for (int i=0; i<num_input*num_output; i++)
        {
            for (int k=0; k<maxk; k++)
            {
                pt[maxk-1 - k] = p[k];
            }

            p += maxk;
            pt += maxk;
        }
    }

    // pack4
    if (num_input % 4 == 0 && num_output % 4 == 0)
    {
        // src = kw-kh-inch-outch
        // dst = 4a-4b-kw-kh-inch/4a-outch/4b
        {
            Mat weight_data_r2 = weight_data_transposed.reshape(maxk, num_input, num_output);

            weight_data_pack4.create(maxk, num_input/4, num_output/4, (size_t)4*16, 16);

            for (int q=0; q+3<num_output; q+=4)
            {
                const Mat k0 = weight_data_r2.channel(q);
                const Mat k1 = weight_data_r2.channel(q+1);
                const Mat k2 = weight_data_r2.channel(q+2);
                const Mat k3 = weight_data_r2.channel(q+3);

                Mat g0 = weight_data_pack4.channel(q/4);

                for (int p=0; p+3<num_input; p+=4)
                {
                    const float* k00 = k0.row(p);
                    const float* k01 = k0.row(p+1);
                    const float* k02 = k0.row(p+2);
                    const float* k03 = k0.row(p+3);

                    const float* k10 = k1.row(p);
                    const float* k11 = k1.row(p+1);
                    const float* k12 = k1.row(p+2);
                    const float* k13 = k1.row(p+3);

                    const float* k20 = k2.row(p);
                    const float* k21 = k2.row(p+1);
                    const float* k22 = k2.row(p+2);
                    const float* k23 = k2.row(p+3);

                    const float* k30 = k3.row(p);
                    const float* k31 = k3.row(p+1);
                    const float* k32 = k3.row(p+2);
                    const float* k33 = k3.row(p+3);

                    float* g00 = g0.row(p/4);

                    for (int k=0; k<maxk; k++)
                    {
                        g00[0] = k00[k];
                        g00[1] = k10[k];
                        g00[2] = k20[k];
                        g00[3] = k30[k];

                        g00[4] = k01[k];
                        g00[5] = k11[k];
                        g00[6] = k21[k];
                        g00[7] = k31[k];

                        g00[8] = k02[k];
                        g00[9] = k12[k];
                        g00[10] = k22[k];
                        g00[11] = k32[k];

                        g00[12] = k03[k];
                        g00[13] = k13[k];
                        g00[14] = k23[k];
                        g00[15] = k33[k];

                        g00 += 16;
                    }
                }
            }
        }
    }

    // pack1to4
    if (num_input % 4 != 0 && num_output % 4 == 0)
    {
        // src = kw-kh-inch-outch
        // dst = 4b-kw-kh-inch-outch/4b
        {
            Mat weight_data_r2 = weight_data_transposed.reshape(maxk, num_input, num_output);

            weight_data_pack1to4.create(maxk, num_input, num_output/4, (size_t)4*4, 4);

            for (int q=0; q+3<num_output; q+=4)
            {
                const Mat k0 = weight_data_r2.channel(q);
                const Mat k1 = weight_data_r2.channel(q+1);
                const Mat k2 = weight_data_r2.channel(q+2);
                const Mat k3 = weight_data_r2.channel(q+3);

                Mat g0 = weight_data_pack1to4.channel(q/4);

                for (int p=0; p<num_input; p++)
                {
                    const float* k00 = k0.row(p);
                    const float* k10 = k1.row(p);
                    const float* k20 = k2.row(p);
                    const float* k30 = k3.row(p);

                    float* g00 = g0.row(p);

                    for (int k=0; k<maxk; k++)
                    {
                        g00[0] = k00[k];
                        g00[1] = k10[k];
                        g00[2] = k20[k];
                        g00[3] = k30[k];

                        g00 += 4;
                    }
                }
            }
        }
    }

    // pack4to1
    if (num_input % 4 == 0 && num_output % 4 != 0)
    {
        // src = kw-kh-inch-outch
        // dst = 4a-kw-kh-inch/4a-outch
        {
            Mat weight_data_r2 = weight_data_transposed.reshape(maxk, num_input, num_output);

            weight_data_pack4to1.create(maxk, num_input/4, num_output, (size_t)4*4, 4);

            for (int q=0; q<num_output; q++)
            {
                const Mat k0 = weight_data_r2.channel(q);
                Mat g0 = weight_data_pack4to1.channel(q);

                for (int p=0; p+3<num_input; p+=4)
                {
                    const float* k00 = k0.row(p);
                    const float* k01 = k0.row(p+1);
                    const float* k02 = k0.row(p+2);
                    const float* k03 = k0.row(p+3);

                    float* g00 = g0.row(p/4);

                    for (int k=0; k<maxk; k++)
                    {
                        g00[0] = k00[k];
                        g00[1] = k01[k];
                        g00[2] = k02[k];
                        g00[3] = k03[k];

                        g00 += 4;
                    }
                }
            }
        }
    }

    } // opt.use_packing_layout
#endif // __ARM_NEON

    return 0;
}

int Deconvolution_arm::destroy_pipeline(const Option& opt)
{
    if (activation)
    {
        Option opt_cpu = opt;
        opt_cpu.use_vulkan_compute = false;
        activation->destroy_pipeline(opt_cpu);
        delete activation;
        activation = 0;
    }

    return 0;
}

int Deconvolution_arm::forward(const Mat& bottom_blob, Mat& top_blob, const Option& opt) const
{
    // deconvolv with NxN kernel
    // value = value + bias

#if __ARM_NEON
    if (opt.use_packing_layout)
    {

    int w = bottom_blob.w;
    int h = bottom_blob.h;
    int channels = bottom_blob.c;
    size_t elemsize = bottom_blob.elemsize;
    int elempack = bottom_blob.elempack;

//     fprintf(stderr, "Deconvolution input %d x %d  pad = %d %d  ksize=%d %d  stride=%d %d\n", w, h, pad_w, pad_h, kernel_w, kernel_h, stride_w, stride_h);

    const int kernel_extent_w = dilation_w * (kernel_w - 1) + 1;
    const int kernel_extent_h = dilation_h * (kernel_h - 1) + 1;

    int outw = (w - 1) * stride_w + kernel_extent_w;
    int outh = (h - 1) * stride_h + kernel_extent_h;
    int out_elempack = num_output % 4 == 0 ? 4 : 1;
    size_t out_elemsize = elemsize / elempack * out_elempack;

    Mat top_blob_bordered;
    if (pad_left > 0 || pad_right > 0 || pad_top > 0 || pad_bottom > 0)
    {
        top_blob_bordered.create(outw, outh, num_output / out_elempack, out_elemsize, out_elempack, opt.workspace_allocator);
        if (top_blob_bordered.empty())
            return -100;
    }
    else
    {
        top_blob_bordered = top_blob;
        top_blob_bordered.create(outw, outh, num_output / out_elempack, out_elemsize, out_elempack, opt.blob_allocator);
        if (top_blob_bordered.empty())
            return -100;
    }

    const int maxk = kernel_w * kernel_h;

    if (elempack == 4 && out_elempack == 4)
    {
        // num_output
        #pragma omp parallel for num_threads(opt.num_threads)
        for (int p=0; p<num_output / out_elempack; p++)
        {
            float* outptr = top_blob_bordered.channel(p);

            for (int i = 0; i < outh; i++)
            {
                for (int j = 0; j < outw; j++)
                {
                    float32x4_t _sum = vdupq_n_f32(0.f);

                    if (bias_term)
                    {
                        _sum = vld1q_f32(((const float*)bias_data) + p * 4);
                    }

                    const float* kptr = (const float*)weight_data_pack4 + maxk * channels * p * 16;

                    // channels
                    for (int q=0; q<channels; q++)
                    {
                        const Mat m = bottom_blob.channel(q);

                        for (int y = 0; y < kernel_h; y++)
                        {
                            int sys = (i + y * dilation_h - (kernel_extent_h - 1));
                            if (sys % stride_h != 0)
                                continue;

                            int sy = sys / stride_h;
                            if (sy < 0 || sy >= h)
                                continue;

                            for (int x = 0; x < kernel_w; x++)
                            {
                                int sxs = (j + x * dilation_w - (kernel_extent_w - 1));
                                if (sxs % stride_w != 0)
                                    continue;

                                int sx = sxs / stride_w;
                                if (sx < 0 || sx >= w)
                                    continue;

                                const float* sptr = m.row(sy) + sx * 4;

                                float32x4_t _val = vld1q_f32( sptr );

                                int k = y * kernel_w + x;

                                float32x4_t _w0 = vld1q_f32( kptr + k * 16 );
                                float32x4_t _w1 = vld1q_f32( kptr + k * 16 + 4 );
                                float32x4_t _w2 = vld1q_f32( kptr + k * 16 + 8 );
                                float32x4_t _w3 = vld1q_f32( kptr + k * 16 + 12 );

#if __aarch64__
                                _sum = vmlaq_laneq_f32(_sum, _w0, _val, 0);
                                _sum = vmlaq_laneq_f32(_sum, _w1, _val, 1);
                                _sum = vmlaq_laneq_f32(_sum, _w2, _val, 2);
                                _sum = vmlaq_laneq_f32(_sum, _w3, _val, 3);
#else
                                _sum = vmlaq_lane_f32(_sum, _w0, vget_low_f32(_val), 0);
                                _sum = vmlaq_lane_f32(_sum, _w1, vget_low_f32(_val), 1);
                                _sum = vmlaq_lane_f32(_sum, _w2, vget_high_f32(_val), 0);
                                _sum = vmlaq_lane_f32(_sum, _w3, vget_high_f32(_val), 1);
#endif
                            }
                        }

                        kptr += maxk * 16;
                    }

                    if (activation_type == 1)
                    {
                        float32x4_t _zero = vdupq_n_f32(0.f);
                        _sum = vmaxq_f32(_sum, _zero);
                    }
                    else if (activation_type == 2)
                    {
                        float32x4_t _zero = vdupq_n_f32(0.f);
                        float32x4_t _slope = vdupq_n_f32(activation_params[0]);
                        uint32x4_t _lemask = vcleq_f32(_sum, _zero);
                        float32x4_t _ps = vmulq_f32(_sum, _slope);
                        _sum = vbslq_f32(_lemask, _ps, _sum);
                    }
                    else if (activation_type == 3)
                    {
                        float32x4_t _min = vdupq_n_f32(activation_params[0]);
                        float32x4_t _max = vdupq_n_f32(activation_params[1]);
                        _sum = vmaxq_f32(_sum, _min);
                        _sum = vminq_f32(_sum, _max);
                    }
                    else if (activation_type == 4)
                    {
                        float32x4_t _one = vdupq_n_f32(1.f);
                        _sum = vnegq_f32(_sum);
                        _sum = exp_ps(_sum);
                        _sum = vaddq_f32(_sum, _one);
                        float32x4_t _outp = vrecpeq_f32(_sum);
                        _outp = vmulq_f32(vrecpsq_f32(_sum, _outp), _outp);
//                         _outp = vmulq_f32(vrecpsq_f32(_sum, _outp), _outp);
                        _sum = _outp;
                    }

                    vst1q_f32(outptr + j * 4, _sum);
                }

                outptr += outw * 4;
            }
        }
    }

    if (elempack == 1 && out_elempack == 4)
    {
        // num_output
        #pragma omp parallel for num_threads(opt.num_threads)
        for (int p=0; p<num_output / out_elempack; p++)
        {
            float* outptr = top_blob_bordered.channel(p);

            for (int i = 0; i < outh; i++)
            {
                for (int j = 0; j < outw; j++)
                {
                    float32x4_t _sum = vdupq_n_f32(0.f);

                    if (bias_term)
                    {
                        _sum = vld1q_f32(((const float*)bias_data) + p * 4);
                    }

                    const float* kptr = (const float*)weight_data_pack1to4 + maxk * channels * p * 4;

                    // channels
                    for (int q=0; q<channels; q++)
                    {
                        const Mat m = bottom_blob.channel(q);

                        for (int y = 0; y < kernel_h; y++)
                        {
                            int sys = (i + y * dilation_h - (kernel_extent_h - 1));
                            if (sys % stride_h != 0)
                                continue;

                            int sy = sys / stride_h;
                            if (sy < 0 || sy >= h)
                                continue;

                            const float* sptr = m.row(sy);

                            for (int x = 0; x < kernel_w; x++)
                            {
                                int sxs = (j + x * dilation_w - (kernel_extent_w - 1));
                                if (sxs % stride_w != 0)
                                    continue;

                                int sx = sxs / stride_w;
                                if (sx < 0 || sx >= w)
                                    continue;

                                float32x4_t _val = vdupq_n_f32( sptr[ sx ] );

                                int k = y * kernel_w + x;

                                float32x4_t _w = vld1q_f32( kptr + k * 4 );

                                _sum = vmlaq_f32(_sum, _val, _w);
                            }
                        }

                        kptr += maxk * 4;
                    }

                    if (activation_type == 1)
                    {
                        float32x4_t _zero = vdupq_n_f32(0.f);
                        _sum = vmaxq_f32(_sum, _zero);
                    }
                    else if (activation_type == 2)
                    {
                        float32x4_t _zero = vdupq_n_f32(0.f);
                        float32x4_t _slope = vdupq_n_f32(activation_params[0]);
                        uint32x4_t _lemask = vcleq_f32(_sum, _zero);
                        float32x4_t _ps = vmulq_f32(_sum, _slope);
                        _sum = vbslq_f32(_lemask, _ps, _sum);
                    }
                    else if (activation_type == 3)
                    {
                        float32x4_t _min = vdupq_n_f32(activation_params[0]);
                        float32x4_t _max = vdupq_n_f32(activation_params[1]);
                        _sum = vmaxq_f32(_sum, _min);
                        _sum = vminq_f32(_sum, _max);
                    }
                    else if (activation_type == 4)
                    {
                        float32x4_t _one = vdupq_n_f32(1.f);
                        _sum = vnegq_f32(_sum);
                        _sum = exp_ps(_sum);
                        _sum = vaddq_f32(_sum, _one);
                        float32x4_t _outp = vrecpeq_f32(_sum);
                        _outp = vmulq_f32(vrecpsq_f32(_sum, _outp), _outp);
//                         _outp = vmulq_f32(vrecpsq_f32(_sum, _outp), _outp);
                        _sum = _outp;
                    }

                    vst1q_f32(outptr + j * 4, _sum);
                }

                outptr += outw * 4;
            }
        }
    }

    if (elempack == 4 && out_elempack == 1)
    {
        // num_output
        #pragma omp parallel for num_threads(opt.num_threads)
        for (int p=0; p<num_output / out_elempack; p++)
        {
            float* outptr = top_blob_bordered.channel(p);

            for (int i = 0; i < outh; i++)
            {
                for (int j = 0; j < outw; j++)
                {
                    float sum = 0.f;

                    if (bias_term)
                    {
                        sum = bias_data[p];
                    }

                    const float* kptr = (const float*)weight_data_pack4to1 + maxk * channels * p * 4;

                    // channels
                    for (int q=0; q<channels; q++)
                    {
                        const Mat m = bottom_blob.channel(q);

                        for (int y = 0; y < kernel_h; y++)
                        {
                            int sys = (i + y * dilation_h - (kernel_extent_h - 1));
                            if (sys % stride_h != 0)
                                continue;

                            int sy = sys / stride_h;
                            if (sy < 0 || sy >= h)
                                continue;

                            for (int x = 0; x < kernel_w; x++)
                            {
                                int sxs = (j + x * dilation_w - (kernel_extent_w - 1));
                                if (sxs % stride_w != 0)
                                    continue;

                                int sx = sxs / stride_w;
                                if (sx < 0 || sx >= w)
                                    continue;

                                const float* sptr = m.row(sy) + sx * 4;

                                float32x4_t _val = vld1q_f32( sptr );

                                int k = y * kernel_w + x;

                                float32x4_t _w = vld1q_f32( kptr + k * 4 );

                                float32x4_t _s4 = vmulq_f32(_val, _w);
#if __aarch64__
                                sum += vaddvq_f32(_s4); // dot
#else
                                float32x2_t _ss = vadd_f32(vget_low_f32(_s4), vget_high_f32(_s4));
                                _ss = vpadd_f32(_ss, _ss);
                                sum += vget_lane_f32(_ss, 0);
#endif
                            }
                        }

                        kptr += maxk * 4;
                    }

                    if (activation_type == 1)
                    {
                        sum = std::max(sum, 0.f);
                    }
                    else if (activation_type == 2)
                    {
                        float slope = activation_params[0];
                        sum = sum > 0.f ? sum : sum * slope;
                    }
                    else if (activation_type == 3)
                    {
                        float min = activation_params[0];
                        float max = activation_params[1];
                        if (sum < min)
                            sum = min;
                        if (sum > max)
                            sum = max;
                    }
                    else if (activation_type == 4)
                    {
                        sum = 1.f / (1.f + exp(-sum));
                    }

                    outptr[j] = sum;
                }

                outptr += outw;
            }
        }
    }

    if (pad_left > 0 || pad_right > 0 || pad_top > 0 || pad_bottom > 0)
    {
        copy_cut_border(top_blob_bordered, top_blob, pad_top, pad_bottom, pad_left, pad_right, opt);
        if (top_blob.empty())
            return -100;

        outw = top_blob.w;
        outh = top_blob.h;
    }
    else
    {
        top_blob = top_blob_bordered;
    }

    return 0;

    } // opt.use_packing_layout
#endif // __ARM_NEON

    if (kernel_w != kernel_h || stride_w != stride_h)
    {
        return Deconvolution::forward(bottom_blob, top_blob, opt);
    }

    const int kernel_size = kernel_w;
    const int stride = stride_w;

    if ((kernel_size != 3 && kernel_size != 4) || stride > 2 || dilation_w != 1 || dilation_h != 1)
    {
        return Deconvolution::forward(bottom_blob, top_blob, opt);
    }

    typedef void (*deconv_func)(const Mat&, Mat&, const Mat&, const Mat&, const Option&);

    // kernel_size x stride
    deconv_func deconv_func_table[2][2] =
    {
        {
            deconv3x3s1_neon,
            deconv3x3s2_neon
        },  // kernel_size = 3
        {
            deconv4x4s1_neon,
            deconv4x4s2_neon
        }   // kernel_size = 4
    };

    deconv_func deconv = deconv_func_table[kernel_size-3][stride-1];
    if (!deconv)
    {
        return Deconvolution::forward(bottom_blob, top_blob, opt);
    }

    int w = bottom_blob.w;
    int h = bottom_blob.h;
    size_t elemsize = bottom_blob.elemsize;

    int outw = (w - 1) * stride + kernel_size;
    int outh = (h - 1) * stride + kernel_size;

    Mat top_blob_bordered;
    if (pad_left > 0 || pad_right > 0 || pad_top > 0 || pad_bottom > 0)
    {
        top_blob_bordered.create(outw, outh, num_output, elemsize, opt.workspace_allocator);
        if (top_blob_bordered.empty())
            return -100;
    }
    else
    {
        top_blob_bordered = top_blob;
        top_blob_bordered.create(outw, outh, num_output, elemsize, opt.blob_allocator);
        if (top_blob_bordered.empty())
            return -100;
    }

    deconv(bottom_blob, top_blob_bordered, weight_data, bias_data, opt);

    if (pad_left > 0 || pad_right > 0 || pad_top > 0 || pad_bottom > 0)
    {
        copy_cut_border(top_blob_bordered, top_blob, pad_top, pad_bottom, pad_left, pad_right, opt);
        if (top_blob.empty())
            return -100;

        outw = top_blob.w;
        outh = top_blob.h;
    }
    else
    {
        top_blob = top_blob_bordered;
    }

    if (activation)
    {
        activation->forward_inplace(top_blob, opt);
    }

    return 0;
}

} // namespace ncnn
