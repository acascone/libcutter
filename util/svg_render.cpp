
#include "svg_render.hpp"

#include <cmath>

using namespace std;

/*--------------------------------------------------
   The arc functions below are:
   Copyright (C) 2000 Eazel, Inc.
   Original Author: Raph Levien <raph@artofcode.com>
   This is adapted from libsvg-cairo under the
   LGPL 2.
--------------------------------------------------*/
void svg_render_state_t::path_arc_segment( const xy & center, double th0, double th1, double rx, double ry, double x_axis_rotation )
{
    const Eigen::Affine2d rotateAndScale{ Eigen::Rotation2Dd( x_axis_rotation * (M_PI / 180.0) ) * Eigen::Scaling( rx, ry ) };
    const double th_half{ 0.5 * (th1 - th0) };
    const double t{ (8.0 / 3.0) * sin (th_half * 0.5) * sin (th_half * 0.5) / sin (th_half) };
    xy pt0{ center + xy{ cos(th0), sin(th0) } };
    xy pt3{ center + xy{ cos(th1), sin(th1) } };
    xy pt1{ pt0 + t * xy{ -sin(th0), cos(th0) } };
    xy pt2{ pt3 - t * xy{ -sin(th1), cos(th1) } };

    pt1 = rotateAndScale * pt1;
    pt2 = rotateAndScale * pt2;
    pt3 = rotateAndScale * pt3;

    curve_to( cur_posn, pt1, pt2, pt3 );
}


bool svg_render_state_t::curve_to( const xy & pta, const xy & ptb, const xy & ptc, const xy & ptd )
{
    const xy bufa{ apply_transform( pta ) };
    const xy bufb{ apply_transform( ptb ) };
    const xy bufc{ apply_transform( ptc ) };
    const xy bufd{ apply_transform( ptd ) };
    //cout<<"    transform curve to:"<<bufa.x<<','<<bufa.y<<'\t'<<bufb.x<<','<<bufb.y<<'\t'<<bufc.x<<','<<bufc.y<<'\t'<<bufd.x<<','<<bufd.y<<endl;
    cur_posn = ptd;
    return device.curve_to( bufa, bufb, bufc, bufd );
}


bool svg_render_state_t::move_to( const xy & pt )
{
    last_moved_to = pt;
    cur_posn      = pt;
    const xy buf{ apply_transform(pt) };
    //cout<<"    transform mov to:"<<buf.x<<','<<buf.y<<endl;
    return device.move_to( buf );
}


/*
svg_render_state_t::svg_render_state_t( Device::Generic & tempdev )
{
memset( transform, 0x00, sizeof( transform ) );
transform[0][0] = 1;
transform[1][1] = 1;
transform[2][2] = 1;
device = tempdev;
}*/

void svg_render_state_t::set_transform( double a, double b, double c, double d, double e, double f )
{
    transform.linear() << a, c, b, d;
    transform.translation() << e, f;
}


xy svg_render_state_t::apply_transform( const xy & pt )
{
    xy buf{ transform * pt };

    buf /= 100;

    //Add a half-inch. This should prevent the mat from fallingout of the device
    //Also, paperspace now reduced by half-inch
    buf.y() += paper_padding;

    return buf;
}


bool svg_render_state_t::cut_to( const xy & pt )
{
    cur_posn = pt;
    const xy buf{ apply_transform( pt ) };
    //cout<<"    transform cut to:"<<buf.x<<','<<buf.y<<endl;
    return device.cut_to( buf );
}

svg_status_t begin_group_callback( void * closure, double opacity )
{
    //    cout<<"Begin group called with opacity="<<opacity<<endl;
    return SVG_STATUS_SUCCESS;
}


svg_status_t end_group_callback( void * closure, double opacity )
{
    //    cout<<"End group called with opacity="<<opacity<<endl;
    return SVG_STATUS_SUCCESS;
}


svg_status_t begin_element_callback( void * ptr )
{
    //    cout<<"Begin element callback called"<<endl;
    return SVG_STATUS_SUCCESS;
}


svg_status_t end_element_callback( void * ptr )
{
    //    cout<<"End element callback called"<<endl;
    return SVG_STATUS_SUCCESS;
}


svg_status_t move_callback( void * ptr, double x, double y )
{
    const xy pt{x,y};
    //cout << "Moving to "<<x<<','<<y<<endl;
    ((svg_render_state_t*)ptr)->move_to( pt );
    return SVG_STATUS_SUCCESS;
}


svg_status_t line_callback( void * ptr, double x, double y )
{
    const xy point{x,y};
    //cout << "Cutting to "<<x<<','<<y<<endl;
    ((svg_render_state_t*)ptr)->cut_to( point );
    return SVG_STATUS_SUCCESS;
}


svg_status_t curve_callback( void * ptr, double x1, double y1, double x2, double y2, double x3, double y3 )
{
    auto state{ static_cast<svg_render_state_t*>(ptr) };
    const xy CP0{state->get_cur_posn()};
    const xy CP1{x1, y1};
    const xy CP2{x2, y2};
    const xy CP3{x3, y3};

    //cout <<"Doing a curve!"<<endl;
    state->curve_to(CP0, CP1, CP2, CP3);
    return SVG_STATUS_SUCCESS;
}


svg_status_t quadratic_curve_callback( void * ptr, double x1, double y1, double x2, double y2 )
{
    auto state{ static_cast<svg_render_state_t*>(ptr) };
    // http://fontforge.github.io/bezier.html
    const xy QP0{ state->get_cur_posn() };
    const xy QP1{ x1, y1 };
    const xy QP2{ x2, y2 };

    const xy CP0{QP0};
    const xy CP1{QP0 + 2.0/3.0 * (QP1-QP0)};
    const xy CP2{QP2 + 2.0/3.0 * (QP1-QP2)};
    const xy CP3{QP2};

    //cout <<"Doing a quadratic curve"<<endl;
    state->curve_to(CP0, CP1, CP2, CP3);
    return SVG_STATUS_SUCCESS;
}


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
double y )
{
    xy cur_posn;
    xy center;
    xy pt0;
    xy pt1;
    double sin_th, cos_th;
    double a00, a01, a10, a11;
    double d, sfactor, sfactor_sq;
    double th0, th1, th_arc;
    int i, n_segs;
    double dx1, dy1, Pr1, Pr2, Px, Py, check;

    //cout<<"Doing an arc at"<<x<<','<<y<<" with rx="<<rx<<" and ry="<<ry<<endl;
    //cout<<"    with large_arc_flag="<<large_arc_flag<<endl;
    //cout<<"    with     sweep_flag="<<    sweep_flag<<endl;

    rx = fabs (rx);
    ry = fabs (ry);

    cur_posn = ((svg_render_state_t*)ptr)->get_cur_posn();

    sin_th = sin (x_axis_rotation * (M_PI / 180.0));
    cos_th = cos (x_axis_rotation * (M_PI / 180.0));

    xy pt{x,y};
    xy delta{ (cur_posn - pt) / 2.0 };
    dx1 =  cos_th * delta.x() + sin_th * delta.y();
    dy1 = -sin_th * delta.x() + cos_th * delta.y();
    Pr1 = rx * rx;
    Pr2 = ry * ry;
    Px = dx1 * dx1;
    Py = dy1 * dy1;
    /* Spec : check if radii are large enough */
    check = Px / Pr1 + Py / Pr2;
    if(check > 1)
    {
        rx = rx * sqrt(check);
        ry = ry * sqrt(check);
    }

    a00 = cos_th / rx;
    a01 = sin_th / rx;
    a10 = -sin_th / ry;
    a11 = cos_th / ry;
    pt0.x() = a00 * cur_posn.x() + a01 * cur_posn.y();
    pt0.y() = a10 * cur_posn.x() + a11 * cur_posn.y();
    pt1.x() = a00 * x + a01 * y;
    pt1.y() = a10 * x + a11 * y;
    /* (x0, y0) is current point in transformed coordinate space.
       (x1, y1) is new point in transformed coordinate space.

       The arc fits a unit-radius circle in this space.
    */
    d = (pt1 - pt0).squaredNorm();
    sfactor_sq = 1.0 / d - 0.25;
    if (sfactor_sq < 0) sfactor_sq = 0;
    sfactor = sqrt (sfactor_sq);
    if (sweep_flag == large_arc_flag) sfactor = -sfactor;
    center.x() = 0.5 * (pt0.x() + pt1.x()) - sfactor * (pt1.y() - pt0.y());
    center.y() = 0.5 * (pt0.y() + pt1.y()) + sfactor * (pt1.x() - pt0.x());
    /* (xc, yc) is center of the circle. */

    th0 = atan2 (pt0.y() - center.y(), pt0.x() - center.x());
    th1 = atan2 (pt1.y() - center.y(), pt1.x() - center.x());

    th_arc = th1 - th0;

    if (th_arc < 0 && sweep_flag)
        th_arc += 2 * M_PI;
    else if (th_arc > 0 && !sweep_flag)
        th_arc -= 2 * M_PI;

    /* XXX: I still need to evaluate the math performed in this
       function. The critical behavior desired is that the arc must be
       approximated within an arbitrary error tolerance, (which the
       user should be able to specify as well). I don't yet know the
       bounds of the error from the following computation of
       n_segs. Plus the "+ 0.001" looks just plain fishy. -cworth */
    n_segs = ceil (fabs (th_arc / (M_PI * 0.5 + 0.001)));

    for (i = 0; i < n_segs; i++)
    {
        ((svg_render_state_t*)ptr)->path_arc_segment ( center,
            th0 + i * th_arc / n_segs,
            th0 + (i + 1) * th_arc / n_segs,
            rx, ry, x_axis_rotation);
    }

    return SVG_STATUS_SUCCESS;
}


svg_status_t close_path_callback( void * ptr )
{
    //cout <<"Closing path"<<endl;

    ((svg_render_state_t*)ptr)->cut_to( ((svg_render_state_t*)ptr)->get_last_moved_to() );
    return SVG_STATUS_SUCCESS;
}


svg_status_t set_color_callback( void * ptr, const svg_color_t * color )
{
    //    cout<<"Setting color"<<endl;
    return SVG_STATUS_SUCCESS;
}


svg_status_t set_stroke_width_callback( void * ptr, svg_length_t * width )
{
    //    cout << "Setting stroke width " << endl;
    return SVG_STATUS_SUCCESS;
}


svg_status_t set_fill_opacity_callback( void * ptr, double fill_opacity )
{
    //    cout <<"Should be setting fill opacity="<<fill_opacity<<endl;
    return SVG_STATUS_SUCCESS;
}


svg_status_t set_fill_paint_callback( void * ptr, const svg_paint_t * paint )
{
    //    cout << "Ignoring fill paint"<<endl;
    return SVG_STATUS_SUCCESS;
}


svg_status_t set_fill_rule_callback( void * ptr, const svg_fill_rule_t fill_rule )
{
    //    cout << "Ignoring fill rule"<<endl;
    return SVG_STATUS_SUCCESS;
}


svg_status_t set_viewport_dimension_callback( void * ptr, svg_length_t * width, svg_length_t * height )
{
    //cout<<"Setting viewport dimensions"<<endl;
    return SVG_STATUS_SUCCESS;
}


svg_status_t apply_view_box_callback( void * ptr, svg_view_box_t view_box, svg_length_t *width, svg_length_t * height )
{
    //cout<<"Applying view box"<<endl;
    return SVG_STATUS_SUCCESS;
}

svg_status_t transform_callback( void * ptr, double a, double b, double c, double d, double e, double f)
{
    //cout <<"Some sort of transform"<<endl;
    //cout <<"    a="<<a<<endl;
    //cout <<"    b="<<b<<endl;
    //cout <<"    c="<<c<<endl;
    //cout <<"    d="<<d<<endl;
    //cout <<"    e="<<e<<endl;
    //cout <<"    f="<<f<<endl;
    ((svg_render_state_t*)ptr)->set_transform(a,b,c,d,e,f);
    return SVG_STATUS_SUCCESS;
}


svg_status_t set_opacity_callback( void * ptr, double opacity )
{
    //    cout <<"Setting opacity:"<<opacity<<endl;
    return SVG_STATUS_SUCCESS;
}


svg_status_t set_font_family_callback( void * ptr, const char * family )
{
    //    cout <<"set font to:"<<family<<endl;
    return SVG_STATUS_SUCCESS;
}


svg_status_t set_font_size_callback( void * ptr, double sz )
{
    //    cout<<"Set font size:"<<sz<<endl;
    return SVG_STATUS_SUCCESS;
}


svg_status_t set_font_style_callback( void * ptr, svg_font_style_t font_style )
{
    //    cout<<"Set font style"<<endl;
    return SVG_STATUS_SUCCESS;
}


svg_status_t set_font_weight_callback( void * ptr, unsigned int font_weight )
{
    //    cout <<"Set font weight:"<<font_weight<<endl;
    return SVG_STATUS_SUCCESS;
}


svg_status_t set_stroke_dash_array_callback( void * ptr, double * dash_array, int num_dashes )
{
    //    cout<<"Setting dash array to "<<num_dashes<<" dashes"<<endl;
    return SVG_STATUS_SUCCESS;
}


svg_status_t set_stroke_dash_offset_callback( void * ptr, svg_length_t * offset )
{
    //    cout <<"Setting dash offset"<<endl;
    return SVG_STATUS_SUCCESS;
}


svg_status_t set_stroke_line_cap_callback( void * ptr, svg_stroke_line_cap_t line_cap )
{
    //    cout <<"Setting stroke line cap"<<endl;
    return SVG_STATUS_SUCCESS;
}


svg_status_t set_stroke_line_join_callback( void * ptr, svg_stroke_line_join_t line_join )
{
    //    cout << "Setting stroke line join"<<endl;
    return SVG_STATUS_SUCCESS;
}


svg_status_t set_stroke_miter_limit_callback( void * ptr, double limit )
{
    //    cout <<"Setting stroke mitre limit to "<<limit<<endl;
    return SVG_STATUS_SUCCESS;
}


svg_status_t set_stroke_opacity_callback( void * ptr, double stroke_opacity )
{
    //    cout <<"Setting stroke opacity to "<<stroke_opacity<<endl;
    return SVG_STATUS_SUCCESS;
}


svg_status_t set_stroke_paint_callback( void * ptr, const svg_paint_t * paint )
{
    //    cout <<"Setting stroke paint"<<endl;
    return SVG_STATUS_SUCCESS;
}


svg_status_t set_text_anchor_callback( void * ptr, svg_text_anchor_t text_anchor )
{
    //    cout <<"Setting text anchor"<<endl;
    return SVG_STATUS_SUCCESS;
}


svg_status_t render_line_callback( void * ptr, svg_length_t * x1, svg_length_t * y1, svg_length_t * x2, svg_length_t * y2 )
{
    //cout<<"Rendering line from "<< x1->value<<','<< y1->value<<" to "<< x2->value<<','<< y2->value<<endl;
    return SVG_STATUS_SUCCESS;
}


svg_status_t render_path_callback( void * ptr )
{
    //cout<<"Rendering path"<<endl;
    return SVG_STATUS_SUCCESS;
}


svg_status_t render_ellipse_callback( void * ptr, svg_length_t * cx, svg_length_t * cy, svg_length_t * rx, svg_length_t * ry )
{
    //Draw an ellipse out of 4 beziers.
    xy control_pts[12];

    //cout <<"Rendering ellipse"<<endl;
    //cout <<"    cx=" << cx->value<<endl;
    //cout <<"    cy=" << cy->value<<endl;
    //cout <<"    rx=" << rx->value<<endl;
    //cout <<"    ry=" << ry->value<<endl;

    for( int i = 0; i < 12; ++i )
    {
        control_pts[i].x() = cx->value;
        control_pts[i].y() = cy->value;
    }

    #define KAPPA .55228475

    //Right
    control_pts[ 0].x() += rx->value;
    control_pts[ 0].y() += 0.0;

    control_pts[ 1].x() += rx->value;
    control_pts[ 1].y() += ry->value * KAPPA;

    control_pts[ 2].x() += rx->value * KAPPA;
    control_pts[ 2].y() += ry->value;

    //Top
    control_pts[ 3].x() -= 0.0;
    control_pts[ 3].y() += ry->value;

    control_pts[ 4].x() -= rx->value * KAPPA;
    control_pts[ 4].y() += ry->value;

    control_pts[ 5].x() -= rx->value;
    control_pts[ 5].y() += ry->value * KAPPA;

    //Left
    control_pts[ 6].x() -= rx->value;
    control_pts[ 6].y() -= 0.0;

    control_pts[ 7].x() -= rx->value;
    control_pts[ 7].y() -= ry->value * KAPPA;

    control_pts[ 8].x() -= rx->value * KAPPA;
    control_pts[ 8].y() -= ry->value;

    //Bottom
    control_pts[ 9].x() += 0.0;
    control_pts[ 9].y() -= ry->value;

    control_pts[10].x() += rx->value * KAPPA;
    control_pts[10].y() -= ry->value;

    control_pts[11].x() += rx->value;
    control_pts[11].y() -= ry->value * KAPPA;

    ((svg_render_state_t*)ptr)->curve_to(
        control_pts[ 0],
        control_pts[ 1],
        control_pts[ 2],
        control_pts[ 3]);

    ((svg_render_state_t*)ptr)->curve_to(
        control_pts[ 3],
        control_pts[ 4],
        control_pts[ 5],
        control_pts[ 6]);

    ((svg_render_state_t*)ptr)->curve_to(
        control_pts[ 6],
        control_pts[ 7],
        control_pts[ 8],
        control_pts[ 9]);

    ((svg_render_state_t*)ptr)->curve_to(
        control_pts[ 9],
        control_pts[10],
        control_pts[11],
        control_pts[ 0]);

    return SVG_STATUS_SUCCESS;
}


svg_status_t render_rect_callback( void * ptr,
svg_length_t * x_len,
svg_length_t * y_len,
svg_length_t * width_len,
svg_length_t * height_len,
svg_length_t * rx_len,
svg_length_t * ry_len )
{
    xy point;
    double x      = x_len->value;
    double y      = y_len->value;
    double rx     = rx_len->value;
    double ry     = ry_len->value;
    double width  = width_len->value;
    double height = height_len->value;

    if( rx > width / 2 )
    {
        rx = width / 2;
    }

    if( ry > height / 2 )
    {
        ry = height / 2;
    }

    //cout <<"Rendering rect"<<endl;
    //cout <<"         x=" << x<<endl;
    //cout <<"         y=" << y<<endl;
    //cout <<"        rx=" << rx<<endl;
    //cout <<"        ry=" << ry<<endl;
    //cout <<"     width=" << width<<endl;
    //cout <<"    height=" << height<<endl;

    //    point.x = x->value + ((svg_render_state_t*)ptr)->get_last_moved_to().x;
    //    point.y = y->value + ((svg_render_state_t*)ptr)->get_last_moved_to().y;
    //    point.x = ((svg_render_state_t*)ptr)->get_last_moved_to().x;
    //    point.y = ((svg_render_state_t*)ptr)->get_last_moved_to().y;

    if( rx > 0 || ry > 0 )
    {
        point.x() = x + rx;
        point.y() = y;
        ((svg_render_state_t*)ptr)->move_to( point );

        point.x() = x + width - rx;
        ((svg_render_state_t*)ptr)->cut_to( point );

        arc_callback( ptr, rx, ry, 0, 0, 1, x + width, y + ry );

        point.x() = x + width;
        point.y() = y + height - ry;
        ((svg_render_state_t*)ptr)->cut_to( point );

        arc_callback( ptr, rx, ry, 0, 0, 1, x + width - rx, y + height );

        point.x() = x + rx;
        point.y() = y + height;
        ((svg_render_state_t*)ptr)->cut_to( point );

        arc_callback( ptr, rx, ry, 0, 0, 1, x, y + height - ry );

        point.x() = x;
        point.y() = y + ry;
        ((svg_render_state_t*)ptr)->cut_to( point );

        arc_callback( ptr, rx, ry, 0, 0, 1, x + rx, y );
    }
    else
    {
        point.x() = x;
        point.y() = y;
        ((svg_render_state_t*)ptr)->move_to( point );

        point.x() += width;
        ((svg_render_state_t*)ptr)->cut_to( point );

        point.y() += height;
        ((svg_render_state_t*)ptr)->cut_to( point );

        point.x() -= width;
        ((svg_render_state_t*)ptr)->cut_to( point );
    }
    close_path_callback( ptr );
    return SVG_STATUS_SUCCESS;
}


svg_status_t render_text_callback( void * ptr, svg_length_t * x, svg_length_t * y, const char * utf8 )
{
    //cout<<"Rendering text:"<<utf8<<endl;
    return SVG_STATUS_SUCCESS;
}


svg_status_t render_image_callback( void * ptr, unsigned char * data, unsigned int data_width, unsigned int data_height, svg_length_t *x, svg_length_t * y, svg_length_t * width, svg_length_t * height )
{
    //cout<<"Rendering image"<<endl;
    return SVG_STATUS_SUCCESS;
}
