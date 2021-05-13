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

#include <Elementary.h>
#include <thorvg_capi.h>

#define WIDTH 800
#define HEIGHT 800


/************************************************************************/
/* Capi Test Code                                                       */
/************************************************************************/

static uint32_t* buffer = NULL;
static Tvg_Canvas* canvas = NULL;
static Eo* view = NULL;

void testCapi()
{
    Tvg_Paint *scene1 = tvg_scene_new();
    canvas = tvg_swcanvas_create();
    Tvg_Paint *scene2 = tvg_scene_new();

    Tvg_Paint *shape = tvg_shape_new();
    tvg_paint_set_opacity(shape, 0);

    tvg_shape_append_circle(shape, 100, 100, 50, 50);
    tvg_shape_move_to(shape, 150, 100);

    tvg_shape_set_stroke_color(shape, 255, 0, 0, 255);
    tvg_shape_set_stroke_width(shape, 6);

    tvg_swcanvas_set_target(canvas, buffer, WIDTH, WIDTH, HEIGHT, TVG_COLORSPACE_ARGB8888);
    
    //render 1 frame
    tvg_canvas_clear(canvas, false);
    tvg_scene_clear(scene2);
    tvg_paint_set_opacity(scene2, 255);
    tvg_paint_translate(scene2, 0, 0);

    tvg_shape_set_fill_color(shape, 255, 255, 255, 255);
    tvg_paint_set_opacity(shape, 255);

    tvg_scene_push(scene2, shape);
    tvg_canvas_push(canvas, scene2);

    tvg_canvas_update(canvas);
    tvg_canvas_draw(canvas);
    tvg_canvas_sync(canvas);

    //application close 
    tvg_canvas_clear(canvas, false);
    tvg_canvas_destroy(canvas);

    tvg_paint_del(shape);
    tvg_paint_del(scene2);
    tvg_paint_del(scene1);
}


/************************************************************************/
/* Main Code                                                            */
/************************************************************************/

void win_del(void *data, Evas_Object *o, void *ev)
{
   elm_exit();
}

void resize_cb(void *data, Evas *e, Evas_Object *obj, void *event_info)
{
    int w = 0, h = 0;
    evas_object_geometry_get(obj, NULL, NULL, &w, &h);

    if ((w != WIDTH || h != HEIGHT) && (w != 0 && h != 0))
    {
        evas_object_image_data_set(view, NULL); //prevent evas scale on invalid buffer by rendering thread
        buffer = (uint32_t*) realloc(buffer, sizeof(uint32_t) * w * h);
        tvg_swcanvas_set_target(canvas, buffer, w, w, h, TVG_COLORSPACE_ARGB8888);

        tvg_canvas_update(canvas);
        tvg_canvas_draw(canvas);
        tvg_canvas_sync(canvas);

        evas_object_image_size_set(view, w, h);
        evas_object_image_data_set(view, buffer);
        evas_object_image_pixels_dirty_set(view, EINA_TRUE);
        evas_object_image_data_update_add(view, 0, 0, w, h);
    }
}

int main(int argc, char **argv)
{
    elm_init(argc, argv);
    tvg_engine_init(TVG_ENGINE_SW | TVG_ENGINE_GL, 0);

    buffer = (uint32_t*) malloc(sizeof(uint32_t) * WIDTH * HEIGHT);

    Eo* win = elm_win_util_standard_add(NULL, "ThorVG Test");

    evas_object_smart_callback_add(win, "delete,request", win_del, 0);
    evas_object_event_callback_add(win, EVAS_CALLBACK_RESIZE, resize_cb, 0);

    view = evas_object_image_filled_add(evas_object_evas_get(win));
    evas_object_image_size_set(view, WIDTH, HEIGHT);
    evas_object_image_data_set(view, buffer);
    evas_object_image_pixels_dirty_set(view, EINA_TRUE);
    evas_object_image_data_update_add(view, 0, 0, WIDTH, HEIGHT);
    evas_object_size_hint_weight_set(view, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
    evas_object_show(view);

    elm_win_resize_object_add(win, view);
    evas_object_geometry_set(win, 0, 0, WIDTH, HEIGHT);
    evas_object_show(win);

    testCapi();

    elm_run();
    tvg_engine_term(TVG_ENGINE_SW | TVG_ENGINE_GL);
    elm_shutdown();

    return 0;
}
