/*
 * libcutter - xy cutter control library
 * Copyright (c) 2010 - libcutter Developers
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * Should you need to contact us, the author, you can do so either at
 * http://github.com/vangdfang/libcutter, or by paper mail:
 *
 * libcutter Developers @ Cowtown Computer Congress
 * 3101 Mercier Street #404, Kansas City, MO 64111
 */

#include <stdint.h>
#include <unistd.h>
#include "device_sim.hpp"
#include "types.h"

#include <SDL/SDL.h>
#include <SDL/SDL_gfxPrimitives.h>

#define DPI_X 100
#define DPI_Y 100

#define DEFAULT_SIZE_X 6
#define DEFAULT_SIZE_Y 12

#define WIDTH  ( ( DPI_X ) * ( DEFAULT_SIZE_X ) )
#define HEIGHT ( ( DPI_Y ) * ( DEFAULT_SIZE_Y ) )

#define DEPTH 32

namespace Device
{

    CV_sim::CV_sim()
    {
        running            = false;
        image              = NULL;
        current_position.x() = 0;
        current_position.y() = 0;
        tool_width         = 1;
    }

    CV_sim::CV_sim( const std::string filename )
    {
        output_filename    = filename;
        running            = false;
        image              = NULL;
        current_position.x() = 0;
        current_position.y() = 0;
        tool_width         = 1;
    }

    bool CV_sim::move_to(const xy& aPoint )
    {
        if( !running )
        {
            return false;
        }

        current_position = convert_to_internal( aPoint );
        return true;
    }

    bool CV_sim::cut_to(const xy & aPoint )
    {
        xy external_cur_posn = convert_to_external( current_position );

        double distance = ( external_cur_posn - aPoint ).norm();

        if( !running )
        {
            return false;
        }

        xy next_position = convert_to_internal( aPoint );

        if( image != NULL )
        {
            lineRGBA( image, current_position.x(), current_position.y(), next_position.x(), next_position.y(), 250, 50, 50, 200 );
            SDL_Flip( image );
            usleep( 100000 * distance );
        }

        current_position = next_position;
        return true;
    }

    bool CV_sim::curve_to(const xy & p0, const xy & p1, const xy & p2, const xy & p3 )
    {
        constexpr size_t NUM_SECTIONS_PER_CURVE{ 20 };

        if( !running )
        {
            return false;
        }

        auto coeffC = 3 * ( p1 - p0 );
        auto coeffB = 3 * ( p2 - p1 ) - coeffC;
        auto coeffA = ( p3 - p0 ) - coeffC - coeffB;

        move_to( p0 );
        for( size_t i = 1; i <= NUM_SECTIONS_PER_CURVE; ++i )
        {
            const auto t = static_cast<double>(i) / static_cast<double>(NUM_SECTIONS_PER_CURVE);
            const auto iter = coeffA * t * t * t + coeffB * t * t + coeffC * t + p0;
            cut_to( iter );
        }

        return true;
    }

    bool CV_sim::start()
    {
        if( image == NULL )
        {
            image = SDL_SetVideoMode(WIDTH, HEIGHT, DEPTH, SDL_HWSURFACE);
        }
        running = true;
        return true;
    }

    bool CV_sim::stop()
    {
        int retn;

        if( image != NULL && output_filename.size() > 4 )
        {
            retn = SDL_SaveBMP( image, output_filename.c_str() );
            SDL_Flip( image );
            SDL_FreeSurface( image );
        }
        running = false;
        return retn == 0;
    }

    xy CV_sim::convert_to_internal( const xy & input )
    {
        return input * static_cast<double>(DPI_X);
    }

    xy CV_sim::convert_to_external( const xy & input )
    {
        return input / static_cast<double>(DPI_X);
    }

    xy CV_sim::get_dimensions( void )
    {
        return { DEFAULT_SIZE_X, DEFAULT_SIZE_Y };
    }

    bool CV_sim::set_tool_width( const float temp_tool_width )
    {
        if( temp_tool_width > 0 )
        {
            tool_width = fabs( temp_tool_width ) * DPI_X * DPI_Y / sqrt( DPI_X * DPI_Y ) + .5;
            if( tool_width < 1 )
            {
                tool_width = 1;
            }
            return true;
        }
        return false;
    }

    SDL_Surface * CV_sim::get_image()
    {
        SDL_Surface * new_image = SDL_ConvertSurface( image, image->format, 0);

        ellipseRGBA( new_image, current_position.x(), current_position.y(), 10, 10, 50, 250, 50, 200 );
        aalineRGBA( new_image, current_position.x() + 5, current_position.y() + 5, current_position.x() - 5, current_position.y() - 5, 250, 50, 50, 200 );
        aalineRGBA( new_image, current_position.x() + 5, current_position.y() - 5, current_position.x() - 5, current_position.y() + 5, 250, 50, 50, 200 );

        return new_image;
    }

} /* end namespace */
