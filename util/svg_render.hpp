#pragma once

#include "device.hpp"
#include "types.h"

#include <svg.h>

class svg_render_state_t
{
    public:
        svg_render_state_t( Device::Generic & tempdev, double padding ) : device( tempdev ), paper_padding( padding )
        {
            set_transform(1,0,0,0,1,0);
        }

        bool move_to( const xy & pt );
        bool cut_to(  const xy & pt );
        bool curve_to( const xy & pta, const xy & ptb, const xy & ptc, const xy & ptd );

        void set_transform( double a, double b, double c, double d, double e, double f );

        xy get_cur_posn( void ){ return cur_posn; }
        xy get_last_moved_to( void ){ return last_moved_to; }
        void path_arc_segment( const xy & center, double th0, double th1, double rx, double ry, double x_axis_rotation );
    private:
        double transform[3][3];
        xy last_moved_to;
        xy cur_posn;
        Device::Generic & device;
        const double paper_padding;
        xy apply_transform( const xy & pt );
};

svg_status_t begin_group_callback( void * closure, double opacity );
svg_status_t end_group_callback( void * closure, double opacity );
svg_status_t begin_element_callback( void * ptr );
svg_status_t end_element_callback( void * ptr );
svg_status_t move_callback( void * ptr, double x, double y );
svg_status_t line_callback( void * ptr, double x, double y );
svg_status_t curve_callback( void * ptr, double x1, double y1, double x2, double y2, double x3, double y3 );
svg_status_t quadratic_curve_callback( void * ptr, double x1, double y1, double x2, double y2 );
/**
 * _svg_cairo_path_arc_to: Add an arc to the given path
 *
 * rx: Radius in x direction (before rotation).
 * ry: Radius in y direction (before rotation).
 * x_axis_rotation: Rotation angle for axes.
 * large_arc_flag: 0 for arc length <= 180, 1 for arc >= 180.
 * sweep: 0 for "negative angle", 1 for "positive angle".
 * x: New x coordinate.
 * y: New y coordinate.
 *
 **/
svg_status_t arc_callback( void * ptr,
double rx,
double ry,
double x_axis_rotation,
int large_arc_flag,
int sweep_flag,
double x,
double y );
svg_status_t close_path_callback( void * ptr );
svg_status_t set_color_callback( void * ptr, const svg_color_t * color );
svg_status_t set_stroke_width_callback( void * ptr, svg_length_t * width );
svg_status_t set_fill_opacity_callback( void * ptr, double fill_opacity );
svg_status_t set_fill_paint_callback( void * ptr, const svg_paint_t * paint );
svg_status_t set_fill_rule_callback( void * ptr, const svg_fill_rule_t fill_rule );
svg_status_t set_viewport_dimension_callback( void * ptr, svg_length_t * width, svg_length_t * height );
svg_status_t apply_view_box_callback( void * ptr, svg_view_box_t view_box, svg_length_t *width, svg_length_t * height );
svg_status_t transform_callback( void * ptr, double a, double b, double c, double d, double e, double f);
svg_status_t set_opacity_callback( void * ptr, double opacity );
svg_status_t set_font_family_callback( void * ptr, const char * family );
svg_status_t set_font_size_callback( void * ptr, double sz );
svg_status_t set_font_style_callback( void * ptr, svg_font_style_t font_style );
svg_status_t set_font_weight_callback( void * ptr, unsigned int font_weight );
svg_status_t set_stroke_dash_array_callback( void * ptr, double * dash_array, int num_dashes );
svg_status_t set_stroke_dash_offset_callback( void * ptr, svg_length_t * offset );
svg_status_t set_stroke_line_cap_callback( void * ptr, svg_stroke_line_cap_t line_cap );
svg_status_t set_stroke_line_join_callback( void * ptr, svg_stroke_line_join_t line_join );
svg_status_t set_stroke_miter_limit_callback( void * ptr, double limit );
svg_status_t set_stroke_opacity_callback( void * ptr, double stroke_opacity );
svg_status_t set_stroke_paint_callback( void * ptr, const svg_paint_t * paint );
svg_status_t set_text_anchor_callback( void * ptr, svg_text_anchor_t text_anchor );
svg_status_t render_line_callback( void * ptr, svg_length_t * x1, svg_length_t * y1, svg_length_t * x2, svg_length_t * y2 );
svg_status_t render_path_callback( void * ptr );
svg_status_t render_ellipse_callback( void * ptr, svg_length_t * cx, svg_length_t * cy, svg_length_t * rx, svg_length_t * ry );
svg_status_t render_rect_callback( void * ptr,
svg_length_t * x_len,
svg_length_t * y_len,
svg_length_t * width_len,
svg_length_t * height_len,
svg_length_t * rx_len,
svg_length_t * ry_len );
svg_status_t render_text_callback( void * ptr, svg_length_t * x, svg_length_t * y, const char * utf8 );
svg_status_t render_image_callback( void * ptr, unsigned char * data, unsigned int data_width, unsigned int data_height, svg_length_t *x, svg_length_t * y, svg_length_t * width, svg_length_t * height );
