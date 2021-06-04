/*
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

#include <stdint.h>
#include <thorvg_capi.h>
#include "catch.hpp"

#define WIDTH  100
#define HEIGHT 100

TEST_CASE("CAPI: radial_gradient_new", "[capi-radial-gradient]")
{
    Tvg_Gradient *gradient = NULL;
    gradient = tvg_radial_gradient_new();
    REQUIRE(gradient != NULL);
}

TEST_CASE("CAPI: radial_gradient_set_result", "[capi-radial-gradient]")
{
    Tvg_Gradient *gradient = NULL;
    gradient = tvg_radial_gradient_new();
    REQUIRE(gradient != NULL);
    REQUIRE(tvg_radial_gradient_set(gradient, 10.0, 10.0, 30.0) == TVG_RESULT_SUCCESS);
}

TEST_CASE("CAPI: radial_gradient_get", "[capi-radial-gradient]")
{
    Tvg_Gradient *gradient = NULL;

    float cx = 0.0, cy = 0.0, radius = 0.0;

    gradient = tvg_radial_gradient_new();
    REQUIRE(gradient != NULL);
    REQUIRE(tvg_radial_gradient_set(gradient, 10.0, 10.0, 30.0) == TVG_RESULT_SUCCESS);

    tvg_radial_gradient_get(gradient, &cx, &cy, &radius);
    REQUIRE(cx == 10.0);
    REQUIRE(cy == 10.0);
    REQUIRE(radius == 30.0);
}

TEST_CASE("CAPI: shape_set_radial_gradient_valid", "[capi-radial-gradient]")
{
    Tvg_Paint *shape = NULL;
    Tvg_Gradient *gradient = NULL;

    shape = tvg_shape_new();
    REQUIRE(shape != NULL);

    gradient = tvg_radial_gradient_new();
    REQUIRE(gradient != NULL);
    REQUIRE(tvg_radial_gradient_set(gradient, 10.0, 10.0, 30.0) == TVG_RESULT_SUCCESS);
    REQUIRE(tvg_shape_set_radial_gradient(shape, gradient) == TVG_RESULT_SUCCESS);
}

TEST_CASE("CAPI: shape_set_radial_gradient_invalid_1", "[capi-radial-gradient]")
{
    Tvg_Paint *shape = NULL;
    Tvg_Gradient *gradient = NULL;

    gradient = tvg_radial_gradient_new();
    REQUIRE(gradient != NULL);
    REQUIRE(tvg_shape_set_radial_gradient(shape, gradient) == TVG_RESULT_INVALID_ARGUMENT);
}

TEST_CASE("CAPI: shape_set_radial_gradient_invalid_2", "[capi-radial-gradient]")
{
    Tvg_Paint *shape = NULL;
    Tvg_Gradient *gradient = NULL;

    shape = tvg_shape_new();
    REQUIRE(shape != NULL);
    REQUIRE(tvg_shape_set_radial_gradient(shape, gradient) == TVG_RESULT_INVALID_ARGUMENT);
}

TEST_CASE("CAPI: shape_set_radial_gradient_invalid_3", "[capi-radial-gradient]")
{
    Tvg_Paint *shape = NULL;
    Tvg_Gradient *gradient = NULL;

    shape = tvg_shape_new();
    REQUIRE(shape != NULL);

    gradient = tvg_linear_gradient_new();
    REQUIRE(gradient != NULL);

    REQUIRE(tvg_shape_set_radial_gradient(shape, gradient) == TVG_RESULT_INVALID_ARGUMENT);
}

TEST_CASE("CAPI: radial_gradient_set_color_stops_invalid_1", "[capi-radial-gradient]")
{
    Tvg_Paint *shape = NULL;
    Tvg_Gradient *gradient = NULL;

    shape = tvg_shape_new();
    REQUIRE(shape != NULL);

    gradient = tvg_radial_gradient_new();
    REQUIRE(gradient != NULL);

    REQUIRE(tvg_gradient_set_color_stops(gradient, NULL, 0) == TVG_RESULT_INVALID_ARGUMENT);
}

TEST_CASE("CAPI: radial_gradient_set_color_stops_invalid_2", "[capi-radial-gradient]")
{
    Tvg_Paint *shape = NULL;
    Tvg_Gradient *gradient = NULL;

    Tvg_Color_Stop color_stops[2] =
    {
        {.offset=0.0, .r=0, .g=0,   .b=0, .a=255},
        {.offset=1,   .r=0, .g=255, .b=0, .a=255},
    };

    shape = tvg_shape_new();
    REQUIRE(shape != NULL);

    gradient = tvg_radial_gradient_new();
    REQUIRE(gradient != NULL);

    REQUIRE(tvg_gradient_set_color_stops(gradient, color_stops, 1) == TVG_RESULT_INVALID_ARGUMENT);
}

TEST_CASE("CAPI: radial_gradient_set_color_stops_valid_result", "[capi-radial-gradient]")
{
    Tvg_Paint *shape = NULL;
    Tvg_Gradient *gradient = NULL;

    Tvg_Color_Stop color_stops[2] =
    {
        {.offset=0.0, .r=0, .g=0,   .b=0, .a=255},
        {.offset=1,   .r=0, .g=255, .b=0, .a=255},
    };

    shape = tvg_shape_new();
    REQUIRE(shape != NULL);

    gradient = tvg_radial_gradient_new();
    REQUIRE(gradient != NULL);

    REQUIRE(tvg_gradient_set_color_stops(gradient, color_stops, 2) == TVG_RESULT_SUCCESS);
}

TEST_CASE("CAPI: radial_gradient_get_color_stops", "[capi-radial-gradient]")
{
    Tvg_Paint *shape = NULL;
    Tvg_Gradient *gradient = NULL;
    Tvg_Color_Stop color_stops[2] =
    {
        {.offset=0.0, .r=0, .g=0,   .b=0, .a=255},
        {.offset=1,   .r=0, .g=255, .b=0, .a=255},
    };

    const Tvg_Color_Stop *color_stops_ret = NULL;
    uint32_t color_stops_count_ret = 0;

    shape = tvg_shape_new();
    REQUIRE(shape != NULL);

    gradient = tvg_radial_gradient_new();
    REQUIRE(gradient != NULL);

    REQUIRE(tvg_gradient_set_color_stops(gradient, color_stops, 2) == TVG_RESULT_SUCCESS);

    REQUIRE(tvg_gradient_get_color_stops(gradient, &color_stops_ret, &color_stops_count_ret) == TVG_RESULT_SUCCESS);
    REQUIRE(color_stops_count_ret == 2);
}

TEST_CASE("CAPI: radial_gradient_set_spread", "[capi-radial-gradient]")
{
    Tvg_Gradient *gradient = NULL;

    gradient = tvg_radial_gradient_new();
    REQUIRE(gradient != NULL);

    REQUIRE(tvg_gradient_set_spread(gradient, TVG_STROKE_FILL_PAD) == TVG_RESULT_SUCCESS);
}

TEST_CASE("CAPI: radial_gradient_get_spread", "[capi-radial-gradient]")
{
    Tvg_Gradient *gradient = NULL;
    Tvg_Stroke_Fill fill = TVG_STROKE_FILL_REPEAT;

    gradient = tvg_radial_gradient_new();
    REQUIRE(gradient != NULL);

    REQUIRE(tvg_gradient_set_spread(gradient, TVG_STROKE_FILL_PAD) == TVG_RESULT_SUCCESS);
    tvg_gradient_get_spread(gradient, &fill);

    REQUIRE(fill == TVG_STROKE_FILL_PAD);
}