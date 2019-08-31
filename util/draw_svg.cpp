#include <iostream>
#include <unistd.h>

#include "device_c.hpp"
#include "svg_render.hpp"

#include "keys.h"

int main(int numArgs, char * args[] )
{
    svg_t * svg;
    svg_length_t width;
    svg_length_t height;
    svg_render_engine_t engine;

    if( numArgs != 3 )
    {
        std::cout<<"Usage: "<<args[0]<<" svgfile.svg iodevice"<<std::endl;
        return 4;
    }

    Device::C c( args[2] );
    c.stop();
    c.start();

    ckey_type move_key={MOVE_KEY_0, MOVE_KEY_1, MOVE_KEY_2, MOVE_KEY_3 };
    c.set_move_key(move_key);

    ckey_type line_key={LINE_KEY_0, LINE_KEY_1, LINE_KEY_2, LINE_KEY_3 };
    c.set_line_key(line_key);

    ckey_type curve_key={CURVE_KEY_0, CURVE_KEY_1, CURVE_KEY_2, CURVE_KEY_3 };
    c.set_curve_key(curve_key);


    svg_render_state_t state(c, 0.5);

    //For debugging
    memset( (void*)&engine, 0xAD, sizeof( engine ) );

    engine.render_image           = render_image_callback;
    engine.render_text            = render_text_callback;
    engine.render_ellipse         = render_ellipse_callback;
    engine.render_path            = render_path_callback;
    engine.render_line            = render_line_callback;
    engine.render_rect            = render_rect_callback;

    engine.set_text_anchor        = set_text_anchor_callback;
    engine.set_stroke_width       = set_stroke_width_callback;
    engine.set_stroke_opacity     = set_stroke_opacity_callback;
    engine.set_stroke_paint       = set_stroke_paint_callback;
    engine.set_stroke_dash_array  = set_stroke_dash_array_callback;
    engine.set_stroke_dash_offset = set_stroke_dash_offset_callback;
    engine.set_stroke_line_cap    = set_stroke_line_cap_callback;
    engine.set_stroke_line_join   = set_stroke_line_join_callback;
    engine.set_stroke_miter_limit = set_stroke_miter_limit_callback;
    engine.set_font_family        = set_font_family_callback;
    engine.set_font_size          = set_font_size_callback;
    engine.set_font_style         = set_font_style_callback;
    engine.set_font_weight        = set_font_weight_callback;
    engine.set_opacity            = set_opacity_callback;
    engine.transform              = transform_callback;
    engine.apply_view_box         = apply_view_box_callback;
    engine.set_viewport_dimension = set_viewport_dimension_callback;
    engine.set_fill_paint         = set_fill_paint_callback;
    engine.set_fill_rule          = set_fill_rule_callback;
    engine.set_fill_opacity       = set_fill_opacity_callback;
    engine.set_color              = set_color_callback;
    engine.begin_group            = begin_group_callback;
    engine.begin_element          = begin_element_callback;
    engine.end_element            = end_element_callback;
    engine.end_group              = end_group_callback;
    engine.move_to                = move_callback;
    engine.line_to                = line_callback;
    engine.curve_to               = curve_callback;
    engine.quadratic_curve_to     = quadratic_curve_callback;
    engine.arc_to                 = arc_callback;
    engine.close_path             = close_path_callback;

    svg_create( &svg );
    svg_parse( svg, args[1] );

    svg_get_size( svg, &width, &height );
    std::cout << "SVG: "<< width.value << "x" << height.value << std::endl;

    svg_render( svg, &engine, (void*)&state );

    svg_destroy( svg );

    sleep(1);
    c.stop();
}
