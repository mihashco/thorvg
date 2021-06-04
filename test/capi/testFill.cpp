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

#define TEST_R 120
#define TEST_G 154
#define TEST_B 180
#define TEST_A 100

TEST_CASE("CAPI: set_fill_color_result", "[capi-fill]")
{   
    Tvg_Paint *paint = NULL;
    paint = tvg_shape_new();
    
    REQUIRE(paint != NULL);
    REQUIRE(tvg_shape_set_fill_color(paint, TEST_R, TEST_G, TEST_B, TEST_A) == TVG_RESULT_SUCCESS);
}

TEST_CASE("CAPI: get_fill_color", "[capi-fill]")
{   
    Tvg_Paint *paint = NULL;
    paint = tvg_shape_new();

    uint8_t r = 0, g = 0, b = 0, a = 0;
    
    REQUIRE(paint != NULL);
    REQUIRE(tvg_shape_set_fill_color(paint, TEST_R, TEST_G, TEST_B, TEST_A) == TVG_RESULT_SUCCESS);

    REQUIRE(tvg_shape_get_fill_color(paint, &r, &g, &b, &a) == TVG_RESULT_SUCCESS);
    REQUIRE(r == TEST_R);
    REQUIRE(g == TEST_G);
    REQUIRE(b == TEST_B);
    REQUIRE(a == TEST_A);
}

TEST_CASE("CAPI: set_fill_rule_even_odd", "[capi-fill]")
{   
    Tvg_Paint *paint = NULL;
    paint = tvg_shape_new();
    
    REQUIRE(paint != NULL);
    REQUIRE(tvg_shape_set_fill_rule(paint, TVG_FILL_RULE_EVEN_ODD) == TVG_RESULT_SUCCESS);
}

TEST_CASE("CAPI: set_fill_rule_winding", "[capi-fill]")
{   
    Tvg_Paint *paint = NULL;
    paint = tvg_shape_new();
    
    REQUIRE(paint != NULL);
    REQUIRE(tvg_shape_set_fill_rule(paint, TVG_FILL_RULE_WINDING) == TVG_RESULT_SUCCESS);
}

TEST_CASE("CAPI: get_fill_rule_even_odd", "[capi-fill]")
{   
    Tvg_Paint *paint = NULL;
    Tvg_Fill_Rule rule = TVG_FILL_RULE_WINDING;

    paint = tvg_shape_new();
    
    REQUIRE(paint != NULL);
    REQUIRE(tvg_shape_set_fill_rule(paint, TVG_FILL_RULE_EVEN_ODD) == TVG_RESULT_SUCCESS);
    REQUIRE(tvg_shape_get_fill_rule(paint, &rule) == TVG_RESULT_SUCCESS);
    REQUIRE(rule == TVG_FILL_RULE_EVEN_ODD);
}

TEST_CASE("CAPI: get_fill_winding", "[capi-fill]")
{   
    Tvg_Paint *paint = NULL;
    Tvg_Fill_Rule rule = TVG_FILL_RULE_EVEN_ODD;

    paint = tvg_shape_new();
    
    REQUIRE(paint != NULL);
    REQUIRE(tvg_shape_set_fill_rule(paint, TVG_FILL_RULE_WINDING) == TVG_RESULT_SUCCESS);
    REQUIRE(tvg_shape_get_fill_rule(paint, &rule) == TVG_RESULT_SUCCESS);
    REQUIRE(rule == TVG_FILL_RULE_WINDING);
}