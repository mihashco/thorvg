﻿/*
 * Copyright (c) 2020-2021 Samsung Electronics Co., Ltd. All rights reserved.

 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:

 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.

 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */
#include "tvgSwCommon.h"
#include "tvgRender.h"
#include <iostream>
#include <float.h>
#include <math.h>

#ifdef THORVG_AVX_VECTOR_SUPPORT
    #include <immintrin.h>
#endif

/************************************************************************/
/* Internal Class Implementation                                        */
/************************************************************************/

static uint32_t _colorAlpha(uint32_t c)
{
    return (c >> 24);
}


static uint32_t _abgrJoin(uint8_t r, uint8_t g, uint8_t b, uint8_t a)
{
    return (a << 24 | b << 16 | g << 8 | r);
}


static uint32_t _argbJoin(uint8_t r, uint8_t g, uint8_t b, uint8_t a)
{
    return (a << 24 | r << 16 | g << 8 | b);
}


static bool _inverse(const Matrix* transform, Matrix* invM)
{
    // computes the inverse of a matrix m
    auto det = transform->e11 * (transform->e22 * transform->e33 - transform->e32 * transform->e23) -
               transform->e12 * (transform->e21 * transform->e33 - transform->e23 * transform->e31) +
               transform->e13 * (transform->e21 * transform->e32 - transform->e22 * transform->e31);

    if (fabsf(det) < FLT_EPSILON) return false;

    auto invDet = 1 / det;

    invM->e11 = (transform->e22 * transform->e33 - transform->e32 * transform->e23) * invDet;
    invM->e12 = (transform->e13 * transform->e32 - transform->e12 * transform->e33) * invDet;
    invM->e13 = (transform->e12 * transform->e23 - transform->e13 * transform->e22) * invDet;
    invM->e21 = (transform->e23 * transform->e31 - transform->e21 * transform->e33) * invDet;
    invM->e22 = (transform->e11 * transform->e33 - transform->e13 * transform->e31) * invDet;
    invM->e23 = (transform->e21 * transform->e13 - transform->e11 * transform->e23) * invDet;
    invM->e31 = (transform->e21 * transform->e32 - transform->e31 * transform->e22) * invDet;
    invM->e32 = (transform->e31 * transform->e12 - transform->e11 * transform->e32) * invDet;
    invM->e33 = (transform->e11 * transform->e22 - transform->e21 * transform->e12) * invDet;

    return true;
}


static bool _identify(const Matrix* transform)
{
    if (transform) {
        if (transform->e11 != 1.0f || transform->e12 != 0.0f || transform->e13 != 0.0f ||
            transform->e21 != 0.0f || transform->e22 != 1.0f || transform->e23 != 0.0f ||
            transform->e31 != 0.0f || transform->e32 != 0.0f || transform->e33 != 1.0f) {
            return false;
        }
    }

    return true;
}


static bool _translucent(const SwSurface* surface, uint8_t a)
{
    if (a < 255) return true;
    if (!surface->compositor || surface->compositor->method == CompositeMethod::None) return false;
    return true;
}


/************************************************************************/
/* Rect                                                                 */
/************************************************************************/
#ifdef THORVG_AVX_VECTOR_SUPPORT
static bool _translucentRect(SwSurface* surface, const SwBBox& region, uint32_t c) {
    auto buffer = surface->buffer + (region.min.y * surface->stride) + region.min.x;
    auto h = static_cast<uint32_t>(region.max.y - region.min.y);
    auto w = static_cast<uint32_t>(region.max.x - region.min.x);

    __m128i vColor = _mm_set1_epi32(c);
    __m128i vInvAlpha = _mm_set1_epi32(255 - (c >> 24));
    uint32_t ialpha = 255 - _colorAlpha(c);

    uint32_t iterations = w / 4;
    uint32_t avxFilled = iterations * 4;
    uint32_t left = w - avxFilled;

    if (avxFilled > 0) {
        for (uint32_t y = 0; y < h; ++y) {
            __m128i_u *dest = (__m128i_u *) &buffer[y * surface->stride];
            //Calculate how many columns will be filled by avx
            for (uint32_t x = 0; x < avxFilled; x += 4) {
                *dest = _mm_add_epi32(vColor, ALPHA_BLEND_128(*dest, vInvAlpha));
                dest++;
            }
        }
    }

    if (left > 0) {
        for (uint32_t y = 0; y < h; ++y) {
            //Fill leftovers
            auto dst = &buffer[y * surface->stride];
            for (uint32_t x = avxFilled; x < w; ++x) {
                dst[x] = c + ALPHA_BLEND(dst[x], ialpha);
            }
        }
    }

    return true;
}
#else
static bool _translucentRect(SwSurface* surface, const SwBBox& region, uint32_t color)
{
    auto buffer = surface->buffer + (region.min.y * surface->stride) + region.min.x;
    auto h = static_cast<uint32_t>(region.max.y - region.min.y);
    auto w = static_cast<uint32_t>(region.max.x - region.min.x);
    auto ialpha = 255 - surface->blender.alpha(color);

    for (uint32_t y = 0; y < h; ++y) {
        auto dst = &buffer[y * surface->stride];
        for (uint32_t x = 0; x < w; ++x) {
            dst[x] = color + ALPHA_BLEND(dst[x], ialpha);
        }
    }
    return true;
}
#endif

static bool _translucentRectAlphaMask(SwSurface* surface, const SwBBox& region, uint32_t color)
{
    auto buffer = surface->buffer + (region.min.y * surface->stride) + region.min.x;
    auto h = static_cast<uint32_t>(region.max.y - region.min.y);
    auto w = static_cast<uint32_t>(region.max.x - region.min.x);

#ifdef THORVG_LOG_ENABLED
    cout <<"SW_ENGINE: Rectangle Alpha Mask Composition" << endl;
#endif

    auto cbuffer = surface->compositor->image.data + (region.min.y * surface->stride) + region.min.x;   //compositor buffer

    for (uint32_t y = 0; y < h; ++y) {
        auto dst = &buffer[y * surface->stride];
        auto cmp = &cbuffer[y * surface->stride];
        for (uint32_t x = 0; x < w; ++x) {
            auto tmp = ALPHA_BLEND(color, surface->blender.alpha(*cmp));
            dst[x] = tmp + ALPHA_BLEND(dst[x], 255 - surface->blender.alpha(tmp));
            ++cmp;
        }
    }
    return true;
}

static bool _translucentRectInvAlphaMask(SwSurface* surface, const SwBBox& region, uint32_t color)
{
    printf("[%s:%d][%s]\n", __FILE__, __LINE__, __func__);
    auto buffer = surface->buffer + (region.min.y * surface->stride) + region.min.x;
    auto h = static_cast<uint32_t>(region.max.y - region.min.y);
    auto w = static_cast<uint32_t>(region.max.x - region.min.x);

#ifdef THORVG_LOG_ENABLED
    cout <<"SW_ENGINE: Rectangle Inverse Alpha Mask Composition" << endl;
#endif

    auto cbuffer = surface->compositor->image.data + (region.min.y * surface->stride) + region.min.x;   //compositor buffer

    for (uint32_t y = 0; y < h; ++y) {
        auto dst = &buffer[y * surface->stride];
        auto cmp = &cbuffer[y * surface->stride];
        for (uint32_t x = 0; x < w; ++x) {
            auto ialpha = 255 - surface->blender.alpha(*cmp);
            auto tmp = ALPHA_BLEND(color, ialpha);
            dst[x] = tmp + ALPHA_BLEND(dst[x], 255 - surface->blender.alpha(tmp));
            ++cmp;
        }
    }
    return true;
}

static bool _rasterTranslucentRect(SwSurface* surface, const SwBBox& region, uint32_t color)
{
    if (surface->compositor) {
        if (surface->compositor->method == CompositeMethod::AlphaMask) {
            return _translucentRectAlphaMask(surface, region, color);
        }
        if (surface->compositor->method == CompositeMethod::InvAlphaMask) {
            return _translucentRectInvAlphaMask(surface, region, color);
        }
    }
    return _translucentRect(surface, region, color);
}

static bool _rasterSolidRect(SwSurface* surface, const SwBBox& region, uint32_t color)
{
    auto buffer = surface->buffer + (region.min.y * surface->stride);
    auto w = static_cast<uint32_t>(region.max.x - region.min.x);
    auto h = static_cast<uint32_t>(region.max.y - region.min.y);

    for (uint32_t y = 0; y < h; ++y) {
        rasterRGBA32(buffer + y * surface->stride, color, region.min.x, w);
    }
    return true;
}


/************************************************************************/
/* Rle                                                                  */
/************************************************************************/
static bool _translucentRle(SwSurface* surface, const SwRleData* rle, uint32_t color)
{
    #ifndef THORVG_AVX_VECTOR_SUPPORT
    auto span = rle->spans;
    uint32_t src;

    for (uint32_t i = 0; i < rle->size; ++i) {
        auto dst = &surface->buffer[span->y * surface->stride + span->x];
        if (span->coverage < 255) src = ALPHA_BLEND(color, span->coverage);
        else src = color;
        auto ialpha = 255 - surface->blender.alpha(src);
        for (uint32_t x = 0; x < span->len; ++x) {
            dst[x] = src + ALPHA_BLEND(dst[x], ialpha);
        }
        ++span;
    }
    return true;
    #else
    auto span = rle->spans;
    uint32_t src;

    for (uint32_t i = 0; i < rle->size; ++i) {
        //destination
        __m128i_u *dest = (__m128i_u *) &surface->buffer[span->y * surface->stride + span->x];

        if (span->coverage < 255) src = ALPHA_BLEND(color, span->coverage);
        else src = color;

        __m128i vInvAlpha = _mm_set1_epi32(255 - (src >> 24));

        uint32_t ialpha = 255 - _colorAlpha(src);
        uint32_t iterations = span->len / 4;
        uint32_t avxFilled = iterations * 4;
        uint32_t left = span->len - avxFilled;

        if (avxFilled) {
            for (uint32_t x = 0; x < span->len; x += 4) {
                ALPHA_BLEND_128(*dest, vInvAlpha);
            }
        }

        if (left) {
            auto dst = &surface->buffer[span->y * surface->stride + span->x];
            for (uint32_t x = avxFilled; x < span->len; ++x) {
                dst[x] = src + ALPHA_BLEND(dst[x], ialpha);
            }
        }

        ++span;
    }
    return true;
    #endif
}

static bool _translucentRleAlphaMask(SwSurface* surface, const SwRleData* rle, uint32_t color)
{
#ifdef THORVG_LOG_ENABLED
    cout <<"SW_ENGINE: Rle Alpha Mask Composition" << endl;
#endif
    auto span = rle->spans;
    uint32_t src;
    auto tbuffer = static_cast<uint32_t*>(alloca(sizeof(uint32_t) * surface->w));  //temp buffer for intermediate processing
    auto cbuffer = surface->compositor->image.data;

    for (uint32_t i = 0; i < rle->size; ++i) {
        auto dst = &surface->buffer[span->y * surface->stride + span->x];
        auto cmp = &cbuffer[span->y * surface->stride + span->x];
        auto tmp = tbuffer;
        if (span->coverage < 255) src = ALPHA_BLEND(color, span->coverage);
        else src = color;
        for (uint32_t x = 0; x < span->len; ++x) {
            *tmp = ALPHA_BLEND(src, surface->blender.alpha(*cmp));
            dst[x] = *tmp + ALPHA_BLEND(dst[x], 255 - surface->blender.alpha(*tmp));
            ++tmp;
            ++cmp;
        }
        ++span;
    }
    return true;
}

static bool _translucentRleInvAlphaMask(SwSurface* surface, SwRleData* rle, uint32_t color)
{
    printf("[%s:%d][%s]\n", __FILE__, __LINE__, __func__);
#ifdef THORVG_LOG_ENABLED
    cout <<"SW_ENGINE: Rle Inverse Alpha Mask Composition" << endl;
#endif
    auto span = rle->spans;
    uint32_t src;
    auto tbuffer = static_cast<uint32_t*>(alloca(sizeof(uint32_t) * surface->w));  //temp buffer for intermediate processing
    auto cbuffer = surface->compositor->image.data;

    for (uint32_t i = 0; i < rle->size; ++i) {
        auto dst = &surface->buffer[span->y * surface->stride + span->x];
        auto cmp = &cbuffer[span->y * surface->stride + span->x];
        auto tmp = tbuffer;
        if (span->coverage < 255) src = ALPHA_BLEND(color, span->coverage);
        else src = color;
        for (uint32_t x = 0; x < span->len; ++x) {
            auto ialpha = 255 - surface->blender.alpha(*cmp);
            *tmp = ALPHA_BLEND(src, ialpha);
            dst[x] = *tmp + ALPHA_BLEND(dst[x], 255 - surface->blender.alpha(*tmp));
            ++tmp;
            ++cmp;
        }
        ++span;
    }
    return true;
}

static bool _rasterTranslucentRle(SwSurface* surface, SwRleData* rle, uint32_t color)
{
    if (!rle) return false;

    if (surface->compositor) {
        if (surface->compositor->method == CompositeMethod::AlphaMask) {
            return _translucentRleAlphaMask(surface, rle, color);
        }
        if (surface->compositor->method == CompositeMethod::InvAlphaMask) {
            return _translucentRleInvAlphaMask(surface, rle, color);
        }
    }
    return _translucentRle(surface, rle, color);
}

static bool _rasterSolidRle(SwSurface* surface, const SwRleData* rle, uint32_t color)
{
    if (!rle) return false;

    auto span = rle->spans;

    for (uint32_t i = 0; i < rle->size; ++i) {
        if (span->coverage == 255) {
            rasterRGBA32(surface->buffer + span->y * surface->stride, color, span->x, span->len);
        } else {
#ifndef THORVG_AVX_VECTOR_SUPPORT
            auto dst = &surface->buffer[span->y * surface->stride + span->x];
            auto src = ALPHA_BLEND(color, span->coverage);
            auto ialpha = 255 - span->coverage;
            for (uint32_t i = 0; i < span->len; ++i) {
                dst[i] = src + ALPHA_BLEND(dst[i], ialpha);
            }
#else
            __m128i_u *dest = (__m128i_u *) &surface->buffer[span->y * surface->stride + span->x];

            uint32_t src = ALPHA_BLEND(color, span->coverage);
            uint32_t ialpha = 255 - span->coverage;
            uint32_t iterations = span->len / 4;
            uint32_t avxFilled = iterations * 4;
            uint32_t left = span->len - avxFilled;

             __m128i vInvAlpha = _mm_set1_epi32(255 - (src >> 24));

            if (avxFilled) {
                for (uint32_t x = 0; x < span->len; x += 4) {
                    ALPHA_BLEND_128(*dest, vInvAlpha);
                }
            }

            if (left > 0) {
                auto dst = &surface->buffer[span->y * surface->stride + span->x];
                for (uint32_t i = avxFilled; i < span->len; ++i) {
                    dst[i] = src + ALPHA_BLEND(dst[i], ialpha);
                }
            }
#endif
        }
        ++span;
    }
    return true;
}


/************************************************************************/
/* Image                                                                */
/************************************************************************/

static bool _rasterTranslucentImageRle(SwSurface* surface, const SwRleData* rle, uint32_t *img, uint32_t w, uint32_t h, uint32_t opacity)
{
    auto span = rle->spans;

    for (uint32_t i = 0; i < rle->size; ++i, ++span) {
        auto dst = &surface->buffer[span->y * surface->stride + span->x];
        auto src = img + span->x + span->y * w;    //TODO: need to use image's stride
        auto alpha = ALPHA_MULTIPLY(span->coverage, opacity);
        for (uint32_t x = 0; x < span->len; ++x, ++dst, ++src) {
            *src = ALPHA_BLEND(*src, alpha);
            *dst = *src + ALPHA_BLEND(*dst, 255 - surface->blender.alpha(*src));
        }
    }
    return true;
}


static bool _rasterTranslucentImageRle(SwSurface* surface, const SwRleData* rle, uint32_t *img, uint32_t w, uint32_t h, uint32_t opacity, const Matrix* invTransform)
{
    auto span = rle->spans;

    for (uint32_t i = 0; i < rle->size; ++i, ++span) {
        auto ey1 = span->y * invTransform->e12 + invTransform->e13;
        auto ey2 = span->y * invTransform->e22 + invTransform->e23;
        auto dst = &surface->buffer[span->y * surface->stride + span->x];
        auto alpha = ALPHA_MULTIPLY(span->coverage, opacity);
        for (uint32_t x = 0; x < span->len; ++x, ++dst) {
            auto rX = static_cast<uint32_t>(roundf((span->x + x) * invTransform->e11 + ey1));
            auto rY = static_cast<uint32_t>(roundf((span->x + x) * invTransform->e21 + ey2));
            if (rX >= w || rY >= h) continue;
            auto src = ALPHA_BLEND(img[rY * w + rX], alpha);     //TODO: need to use image's stride
            *dst = src + ALPHA_BLEND(*dst, 255 - surface->blender.alpha(src));
        }
    }
    return true;
}


static bool _rasterImageRle(SwSurface* surface, SwRleData* rle, uint32_t *img, uint32_t w, uint32_t h)
{
    auto span = rle->spans;

    for (uint32_t i = 0; i < rle->size; ++i, ++span) {
        auto dst = &surface->buffer[span->y * surface->stride + span->x];
        auto src = img + span->x + span->y * w;    //TODO: need to use image's stride
        for (uint32_t x = 0; x < span->len; ++x, ++dst, ++src) {
            *src = ALPHA_BLEND(*src, span->coverage);
            *dst = *src + ALPHA_BLEND(*dst, 255 - surface->blender.alpha(*src));
        }
    }
    return true;
}


static bool _rasterImageRle(SwSurface* surface, SwRleData* rle, uint32_t *img, uint32_t w, uint32_t h, const Matrix* invTransform)
{
    auto span = rle->spans;

    for (uint32_t i = 0; i < rle->size; ++i, ++span) {
        auto ey1 = span->y * invTransform->e12 + invTransform->e13;
        auto ey2 = span->y * invTransform->e22 + invTransform->e23;
        auto dst = &surface->buffer[span->y * surface->stride + span->x];
        for (uint32_t x = 0; x < span->len; ++x, ++dst) {
            auto rX = static_cast<uint32_t>(roundf((span->x + x) * invTransform->e11 + ey1));
            auto rY = static_cast<uint32_t>(roundf((span->x + x) * invTransform->e21 + ey2));
            if (rX >= w || rY >= h) continue;
            auto src = ALPHA_BLEND(img[rY * w + rX], span->coverage);    //TODO: need to use image's stride
            *dst = src + ALPHA_BLEND(*dst, 255 - surface->blender.alpha(src));
        }
    }
    return true;
}


static bool _translucentImage(SwSurface* surface, const uint32_t *img, uint32_t w, TVG_UNUSED uint32_t h, uint32_t opacity, const SwBBox& region, const Matrix* invTransform)
{
    auto dbuffer = &surface->buffer[region.min.y * surface->stride + region.min.x];

    for (auto y = region.min.y; y < region.max.y; ++y) {
        auto dst = dbuffer;
        auto ey1 = y * invTransform->e12 + invTransform->e13;
        auto ey2 = y * invTransform->e22 + invTransform->e23;
        for (auto x = region.min.x; x < region.max.x; ++x, ++dst) {
            auto rX = static_cast<uint32_t>(roundf(x * invTransform->e11 + ey1));
            auto rY = static_cast<uint32_t>(roundf(x * invTransform->e21 + ey2));
            if (rX >= w || rY >= h) continue;
            auto src = ALPHA_BLEND(img[rX + (rY * w)], opacity);    //TODO: need to use image's stride
            *dst = src + ALPHA_BLEND(*dst, 255 - surface->blender.alpha(src));
        }
        dbuffer += surface->stride;
    }
    return true;
}


static bool _translucentImageAlphaMask(SwSurface* surface, const uint32_t *img, uint32_t w, TVG_UNUSED uint32_t h, uint32_t opacity, const SwBBox& region, const Matrix* invTransform)
{
#ifdef THORVG_LOG_ENABLED
    cout <<"SW_ENGINE: Transformed Image Alpha Mask Composition" << endl;
#endif
    auto dbuffer = &surface->buffer[region.min.y * surface->stride + region.min.x];
    auto cbuffer = &surface->compositor->image.data[region.min.y * surface->stride + region.min.x];

    for (auto y = region.min.y; y < region.max.y; ++y) {
        auto dst = dbuffer;
        auto cmp = cbuffer;
        float ey1 = y * invTransform->e12 + invTransform->e13;
        float ey2 = y * invTransform->e22 + invTransform->e23;
        for (auto x = region.min.x; x < region.max.x; ++x, ++dst, ++cmp) {
            auto rX = static_cast<uint32_t>(roundf(x * invTransform->e11 + ey1));
            auto rY = static_cast<uint32_t>(roundf(x * invTransform->e21 + ey2));
            if (rX >= w || rY >= h) continue;
            auto tmp = ALPHA_BLEND(img[rX + (rY * w)], ALPHA_MULTIPLY(opacity, surface->blender.alpha(*cmp)));  //TODO: need to use image's stride
            *dst = tmp + ALPHA_BLEND(*dst, 255 - surface->blender.alpha(tmp));
        }
        dbuffer += surface->stride;
        cbuffer += surface->stride;
    }
    return true;
}

static bool _translucentImageInvAlphaMask(SwSurface* surface, const uint32_t *img, uint32_t w, uint32_t h, uint32_t opacity, const SwBBox& region, const Matrix* invTransform)
{
#ifdef THORVG_LOG_ENABLED
    cout <<"SW_ENGINE: Transformed Image Inverse Alpha Mask Composition" << endl;
#endif
    auto dbuffer = &surface->buffer[region.min.y * surface->stride + region.min.x];
    auto cbuffer = &surface->compositor->image.data[region.min.y * surface->stride + region.min.x];

    for (auto y = region.min.y; y < region.max.y; ++y) {
        auto dst = dbuffer;
        auto cmp = cbuffer;
        float ey1 = y * invTransform->e12 + invTransform->e13;
        float ey2 = y * invTransform->e22 + invTransform->e23;
        for (auto x = region.min.x; x < region.max.x; ++x, ++dst, ++cmp) {
            auto rX = static_cast<uint32_t>(roundf(x * invTransform->e11 + ey1));
            auto rY = static_cast<uint32_t>(roundf(x * invTransform->e21 + ey2));
            if (rX >= w || rY >= h) continue;
            auto ialpha = 255 - surface->blender.alpha(*cmp);
            auto tmp = ALPHA_BLEND(img[rX + (rY * w)], ALPHA_MULTIPLY(opacity, ialpha));  //TODO: need to use image's stride
            *dst = tmp + ALPHA_BLEND(*dst, 255 - surface->blender.alpha(tmp));
        }
        dbuffer += surface->stride;
        cbuffer += surface->stride;
    }
    return true;
}

static bool _rasterTranslucentImage(SwSurface* surface, const uint32_t *img, uint32_t w, uint32_t h, uint32_t opacity, const SwBBox& region, const Matrix* invTransform)
{
    if (surface->compositor) {
        if (surface->compositor->method == CompositeMethod::AlphaMask) {
            return _translucentImageAlphaMask(surface, img, w, h, opacity, region, invTransform);
        }
        if (surface->compositor->method == CompositeMethod::InvAlphaMask) {
            return _translucentImageInvAlphaMask(surface, img, w, h, opacity, region, invTransform);
        }
    }
    return _translucentImage(surface, img, w, h, opacity, region, invTransform);
}


static bool _translucentImage(SwSurface* surface, uint32_t *img, uint32_t w, uint32_t h, uint32_t opacity, const SwBBox& region)
{
    auto dbuffer = &surface->buffer[region.min.y * surface->stride + region.min.x];
    auto sbuffer = img + region.min.x + region.min.y * w;    //TODO: need to use image's stride

    for (auto y = region.min.y; y < region.max.y; ++y) {
        auto dst = dbuffer;
        auto src = sbuffer;
        for (auto x = region.min.x; x < region.max.x; ++x, ++dst, ++src) {
            auto p = ALPHA_BLEND(*src, opacity);
            *dst = p + ALPHA_BLEND(*dst, 255 - surface->blender.alpha(p));
        }
        dbuffer += surface->stride;
        sbuffer += w;    //TODO: need to use image's stride
    }
    return true;
}


static bool _translucentImageAlphaMask(SwSurface* surface, uint32_t *img, uint32_t w, uint32_t h, uint32_t opacity, const SwBBox& region)
{
    auto buffer = surface->buffer + (region.min.y * surface->stride) + region.min.x;
    auto h2 = static_cast<uint32_t>(region.max.y - region.min.y);
    auto w2 = static_cast<uint32_t>(region.max.x - region.min.x);

#ifdef THORVG_LOG_ENABLED
    cout <<"SW_ENGINE: Image Alpha Mask Composition" << endl;
#endif

    auto sbuffer = img + (region.min.y * w) + region.min.x;
    auto cbuffer = surface->compositor->image.data + (region.min.y * surface->stride) + region.min.x;   //compositor buffer

    for (uint32_t y = 0; y < h2; ++y) {
        auto dst = buffer;
        auto cmp = cbuffer;
        auto src = sbuffer;
        for (uint32_t x = 0; x < w2; ++x, ++dst, ++src, ++cmp) {
            auto tmp = ALPHA_BLEND(*src, ALPHA_MULTIPLY(opacity, surface->blender.alpha(*cmp)));
            *dst = tmp + ALPHA_BLEND(*dst, 255 - surface->blender.alpha(tmp));
        }
        buffer += surface->stride;
        cbuffer += surface->stride;
        sbuffer += w;   //TODO: need to use image's stride
    }
    return true;
}


static bool _translucentImageInvAlphaMask(SwSurface* surface, uint32_t *img, uint32_t w, uint32_t h, uint32_t opacity, const SwBBox& region)
{
    auto buffer = surface->buffer + (region.min.y * surface->stride) + region.min.x;
    auto h2 = static_cast<uint32_t>(region.max.y - region.min.y);
    auto w2 = static_cast<uint32_t>(region.max.x - region.min.x);

#ifdef THORVG_LOG_ENABLED
    cout <<"SW_ENGINE: Image Inverse Alpha Mask Composition" << endl;
#endif

    auto sbuffer = img + (region.min.y * w) + region.min.x;
    auto cbuffer = surface->compositor->image.data + (region.min.y * surface->stride) + region.min.x;   //compositor buffer

    for (uint32_t y = 0; y < h2; ++y) {
        auto dst = buffer;
        auto cmp = cbuffer;
        auto src = sbuffer;
        for (uint32_t x = 0; x < w2; ++x, ++dst, ++src, ++cmp) {
            auto ialpha = 255 - surface->blender.alpha(*cmp);
            auto tmp = ALPHA_BLEND(*src, ALPHA_MULTIPLY(opacity, ialpha));
            *dst = tmp + ALPHA_BLEND(*dst, 255 - surface->blender.alpha(tmp));
        }
        buffer += surface->stride;
        cbuffer += surface->stride;
        sbuffer += w;   //TODO: need to use image's stride
    }
    return true;
}

static bool _rasterTranslucentImage(SwSurface* surface, uint32_t *img, uint32_t w, uint32_t h, uint32_t opacity, const SwBBox& region)
{
    if (surface->compositor) {
        if (surface->compositor->method == CompositeMethod::AlphaMask) {
            return _translucentImageAlphaMask(surface, img, w, h, opacity, region);
        }
        if (surface->compositor->method == CompositeMethod::InvAlphaMask) {
            return _translucentImageInvAlphaMask(surface, img, w, h, opacity, region);
        }
    }
    return _translucentImage(surface, img, w, h, opacity, region);
}


static bool _rasterImage(SwSurface* surface, uint32_t *img, uint32_t w, TVG_UNUSED uint32_t h, const SwBBox& region)
{
    auto dbuffer = &surface->buffer[region.min.y * surface->stride + region.min.x];
    auto sbuffer = img + region.min.x + region.min.y * w;   //TODO: need to use image's stride

    for (auto y = region.min.y; y < region.max.y; ++y) {
        auto dst = dbuffer;
        auto src = sbuffer;
        for (auto x = region.min.x; x < region.max.x; x++, dst++, src++) {
            *dst = *src + ALPHA_BLEND(*dst, 255 - surface->blender.alpha(*src));
        }
        dbuffer += surface->stride;
        sbuffer += w;    //TODO: need to use image's stride
    }
    return true;
}


static bool _rasterImage(SwSurface* surface, const uint32_t *img, uint32_t w, uint32_t h, const SwBBox& region, const Matrix* invTransform)
{
    for (auto y = region.min.y; y < region.max.y; ++y) {
        auto dst = &surface->buffer[y * surface->stride + region.min.x];
        auto ey1 = y * invTransform->e12 + invTransform->e13;
        auto ey2 = y * invTransform->e22 + invTransform->e23;
        for (auto x = region.min.x; x < region.max.x; ++x, ++dst) {
            auto rX = static_cast<uint32_t>(roundf(x * invTransform->e11 + ey1));
            auto rY = static_cast<uint32_t>(roundf(x * invTransform->e21 + ey2));
            if (rX >= w || rY >= h) continue;
            auto src = img[rX + (rY * w)];    //TODO: need to use image's stride
            *dst = src + ALPHA_BLEND(*dst, 255 - surface->blender.alpha(src));
        }
    }
    return true;
}


/************************************************************************/
/* Gradient                                                             */
/************************************************************************/

static bool _rasterLinearGradientRect(SwSurface* surface, const SwBBox& region, const SwFill* fill)
{
    if (!fill || fill->linear.len < FLT_EPSILON) return false;

    auto buffer = surface->buffer + (region.min.y * surface->stride) + region.min.x;
    auto h = static_cast<uint32_t>(region.max.y - region.min.y);
    auto w = static_cast<uint32_t>(region.max.x - region.min.x);

    //Translucent Gradient
    if (fill->translucent) {

        auto tmpBuf = static_cast<uint32_t*>(alloca(surface->w * sizeof(uint32_t)));
        if (!tmpBuf) return false;

        for (uint32_t y = 0; y < h; ++y) {
            auto dst = &buffer[y * surface->stride];
            fillFetchLinear(fill, tmpBuf, region.min.y + y, region.min.x, 0, w);
            for (uint32_t x = 0; x < w; ++x) {
                dst[x] = tmpBuf[x] + ALPHA_BLEND(dst[x], 255 - surface->blender.alpha(tmpBuf[x]));
            }
        }
    //Opaque Gradient
    } else {
        for (uint32_t y = 0; y < h; ++y) {
            fillFetchLinear(fill, buffer + y * surface->stride, region.min.y + y, region.min.x, 0, w);
        }
    }
    return true;
}


static bool _rasterRadialGradientRect(SwSurface* surface, const SwBBox& region, const SwFill* fill)
{
    if (!fill || fill->radial.a < FLT_EPSILON) return false;

    auto buffer = surface->buffer + (region.min.y * surface->stride) + region.min.x;
    auto h = static_cast<uint32_t>(region.max.y - region.min.y);
    auto w = static_cast<uint32_t>(region.max.x - region.min.x);

    //Translucent Gradient
    if (fill->translucent) {

        auto tmpBuf = static_cast<uint32_t*>(alloca(surface->w * sizeof(uint32_t)));
        if (!tmpBuf) return false;

        for (uint32_t y = 0; y < h; ++y) {
            auto dst = &buffer[y * surface->stride];
            fillFetchRadial(fill, tmpBuf, region.min.y + y, region.min.x, w);
            for (uint32_t x = 0; x < w; ++x) {
                dst[x] = tmpBuf[x] + ALPHA_BLEND(dst[x], 255 - surface->blender.alpha(tmpBuf[x]));
            }
        }
    //Opaque Gradient
    } else {
        for (uint32_t y = 0; y < h; ++y) {
            auto dst = &buffer[y * surface->stride];
            fillFetchRadial(fill, dst, region.min.y + y, region.min.x, w);
        }
    }
    return true;
}


static bool _rasterLinearGradientRle(SwSurface* surface, const SwRleData* rle, const SwFill* fill)
{
    if (!rle || !fill || fill->linear.len < FLT_EPSILON) return false;

    auto buf = static_cast<uint32_t*>(alloca(surface->w * sizeof(uint32_t)));
    if (!buf) return false;

    auto span = rle->spans;

    //Translucent Gradient
    if (fill->translucent) {
        for (uint32_t i = 0; i < rle->size; ++i) {
            auto dst = &surface->buffer[span->y * surface->stride + span->x];
            fillFetchLinear(fill, buf, span->y, span->x, 0, span->len);
            if (span->coverage == 255) {
                for (uint32_t i = 0; i < span->len; ++i) {
                    dst[i] = buf[i] + ALPHA_BLEND(dst[i], 255 - surface->blender.alpha(buf[i]));
                }
            } else {
                for (uint32_t i = 0; i < span->len; ++i) {
                    auto tmp = ALPHA_BLEND(buf[i], span->coverage);
                    dst[i] = tmp + ALPHA_BLEND(dst[i], 255 - surface->blender.alpha(tmp));
                }
            }
            ++span;
        }
    //Opaque Gradient
    } else {
        for (uint32_t i = 0; i < rle->size; ++i) {
            if (span->coverage == 255) {
                fillFetchLinear(fill, surface->buffer + span->y * surface->stride, span->y, span->x, span->x, span->len);
            } else {
                auto dst = &surface->buffer[span->y * surface->stride + span->x];
                fillFetchLinear(fill, buf, span->y, span->x, 0, span->len);
                auto ialpha = 255 - span->coverage;
                for (uint32_t i = 0; i < span->len; ++i) {
                    dst[i] = ALPHA_BLEND(buf[i], span->coverage) + ALPHA_BLEND(dst[i], ialpha);
                }
            }
            ++span;
        }
    }
    return true;
}


static bool _rasterRadialGradientRle(SwSurface* surface, const SwRleData* rle, const SwFill* fill)
{
    if (!rle || !fill || fill->radial.a < FLT_EPSILON) return false;

    auto buf = static_cast<uint32_t*>(alloca(surface->w * sizeof(uint32_t)));
    if (!buf) return false;

    auto span = rle->spans;

    //Translucent Gradient
    if (fill->translucent) {
        for (uint32_t i = 0; i < rle->size; ++i) {
            auto dst = &surface->buffer[span->y * surface->stride + span->x];
            fillFetchRadial(fill, buf, span->y, span->x, span->len);
            if (span->coverage == 255) {
                for (uint32_t i = 0; i < span->len; ++i) {
                    dst[i] = buf[i] + ALPHA_BLEND(dst[i], 255 - surface->blender.alpha(buf[i]));
                }
            } else {
                for (uint32_t i = 0; i < span->len; ++i) {
                    auto tmp = ALPHA_BLEND(buf[i], span->coverage);
                    dst[i] = tmp + ALPHA_BLEND(dst[i], 255 - surface->blender.alpha(tmp));
                }
            }
            ++span;
        }
    //Opaque Gradient
    } else {
        for (uint32_t i = 0; i < rle->size; ++i) {
            auto dst = &surface->buffer[span->y * surface->stride + span->x];
            if (span->coverage == 255) {
                fillFetchRadial(fill, dst, span->y, span->x, span->len);
            } else {
                fillFetchRadial(fill, buf, span->y, span->x, span->len);
                auto ialpha = 255 - span->coverage;
                for (uint32_t i = 0; i < span->len; ++i) {
                    dst[i] = ALPHA_BLEND(buf[i], span->coverage) + ALPHA_BLEND(dst[i], ialpha);
                }
            }
            ++span;
        }
    }
    return true;
}


/************************************************************************/
/* External Class Implementation                                        */
/************************************************************************/

bool rasterCompositor(SwSurface* surface)
{
    if (surface->cs == SwCanvas::ABGR8888) {
        surface->blender.alpha = _colorAlpha;
        surface->blender.join = _abgrJoin;
    } else if (surface->cs == SwCanvas::ARGB8888) {
        surface->blender.alpha = _colorAlpha;
        surface->blender.join = _argbJoin;
    } else {
        //What Color Space ???
        return false;
    }

    return true;
}


bool rasterGradientShape(SwSurface* surface, SwShape* shape, unsigned id)
{
    //Fast Track
    if (shape->rect) {
        if (id == FILL_ID_LINEAR) return _rasterLinearGradientRect(surface, shape->bbox, shape->fill);
        return _rasterRadialGradientRect(surface, shape->bbox, shape->fill);
    } else {
        if (id == FILL_ID_LINEAR) return _rasterLinearGradientRle(surface, shape->rle, shape->fill);
        return _rasterRadialGradientRle(surface, shape->rle, shape->fill);
    }
    return false;
}


bool rasterSolidShape(SwSurface* surface, SwShape* shape, uint8_t r, uint8_t g, uint8_t b, uint8_t a)
{
    r = ALPHA_MULTIPLY(r, a);
    g = ALPHA_MULTIPLY(g, a);
    b = ALPHA_MULTIPLY(b, a);

    auto color = surface->blender.join(r, g, b, a);
    auto translucent = _translucent(surface, a);

    //Fast Track
    if (shape->rect) {
        if (translucent) return _rasterTranslucentRect(surface, shape->bbox, color);
        return _rasterSolidRect(surface, shape->bbox, color);
    }
    if (translucent) {
        return _rasterTranslucentRle(surface, shape->rle, color);
    }
    return _rasterSolidRle(surface, shape->rle, color);
}


bool rasterStroke(SwSurface* surface, SwShape* shape, uint8_t r, uint8_t g, uint8_t b, uint8_t a)
{
    r = ALPHA_MULTIPLY(r, a);
    g = ALPHA_MULTIPLY(g, a);
    b = ALPHA_MULTIPLY(b, a);

    auto color = surface->blender.join(r, g, b, a);
    auto translucent = _translucent(surface, a);

    if (translucent) return _rasterTranslucentRle(surface, shape->strokeRle, color);
    return _rasterSolidRle(surface, shape->strokeRle, color);
}


bool rasterGradientStroke(SwSurface* surface, SwShape* shape, unsigned id)
{
    if (id == FILL_ID_LINEAR) return _rasterLinearGradientRle(surface, shape->strokeRle, shape->stroke->fill);
    return _rasterRadialGradientRle(surface, shape->strokeRle, shape->stroke->fill);

    return false;
}


bool rasterClear(SwSurface* surface)
{
    if (!surface || !surface->buffer || surface->stride <= 0 || surface->w <= 0 || surface->h <= 0) return false;

    if (surface->w == surface->stride) {
        rasterRGBA32(surface->buffer, 0x00000000, 0, surface->w * surface->h);
    } else {
        for (uint32_t i = 0; i < surface->h; i++) {
            rasterRGBA32(surface->buffer + surface->stride * i, 0x00000000, 0, surface->w);
        }
    }
    return true;
}


bool rasterImage(SwSurface* surface, SwImage* image, const Matrix* transform, const SwBBox& bbox, uint32_t opacity)
{
    Matrix invTransform;

    if (transform) {
        if (!_inverse(transform, &invTransform)) return false;
    }
    else invTransform = {1, 0, 0, 0, 1, 0, 0, 0, 1};

    auto translucent = _translucent(surface, opacity);

    if (image->rle) {
        //Fast track
        if (_identify(transform)) {
            //OPTIMIZE ME: Support non transformed image. Only shifted image can use these routines.
            if (translucent) return _rasterTranslucentImageRle(surface, image->rle, image->data, image->w, image->h, opacity);
            return _rasterImageRle(surface, image->rle, image->data, image->w, image->h);
        } else {
            if (translucent) return _rasterTranslucentImageRle(surface, image->rle, image->data, image->w, image->h, opacity, &invTransform);
            return _rasterImageRle(surface, image->rle, image->data, image->w, image->h, &invTransform);
        }
    }
    else {
        //Fast track
        if (_identify(transform)) {
            //OPTIMIZE ME: Support non transformed image. Only shifted image can use these routines.
            if (translucent) return _rasterTranslucentImage(surface, image->data, image->w, image->h, opacity, bbox);
            else return _rasterImage(surface, image->data, image->w, image->h, bbox);
        } else {
            if (translucent) return _rasterTranslucentImage(surface, image->data, image->w, image->h, opacity, bbox, &invTransform);
            else return _rasterImage(surface, image->data, image->w, image->h, bbox, &invTransform);
        }
    }
}
