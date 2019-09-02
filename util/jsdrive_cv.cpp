/*
 * jsdrive_cv - joystick control for (virtual) cutters
 * Copyright (c) 2010 - libcutter Developers
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 *
 * Should you need to contact us, the author, you can do so either at
 * http://github.com/vangdfang/libcutter, or by paper mail:
 *
 * libcutter Developers @ Cowtown Computer Congress
 * 3101 Mercier Street #404, Kansas City, MO 64111
 *
 * This file was forked from jstest.c, by Vojtech Pavlik: vojtech@suse.cz
 */

#include <sys/ioctl.h>
#include <sys/time.h>
#include <sys/types.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <pthread.h>

#include <linux/joystick.h>

#include "device_sim.hpp"

static xy   pt={3,-4};
static bool tool_down = false;
static bool running = true;

void * thread( void * ptr )
{
    xy startpt={0,0};
    Device::CV_sim c( "output.png" );

    c.stop();
    c.start();

    //set tool width to 1/10th of an inch
    c.set_tool_width( .1 );

    c.move_to(startpt);

    while( running )
    {
        if( tool_down )
        {
            c.cut_to(pt);
        }
        else
        {
            c.move_to(pt);
        }

    }
    c.stop();
    pthread_exit( NULL );
}


int main (int argc, char **argv)
{
    int fd;
    unsigned char axes = 2;
    unsigned char buttons = 2;

    if (argc != 2)
    {
        puts("Usage: jsdrive <joydevice>");
        exit(1);
    }

    if ((fd = open(argv[1], O_RDONLY)) < 0)
    {
        perror(argv[1]);
        exit(3);
    }

    ioctl(fd, JSIOCGAXES, &axes);
    ioctl(fd, JSIOCGBUTTONS, &buttons);

    if (argc == 2 )
    {
        int *axis;
        int *button;
        struct js_event js;

        axis      = (int*)calloc(axes,    sizeof(int)  );
        button    = (int*)calloc(buttons, sizeof(char) );

        pthread_t tid;
        pthread_create( &tid, NULL, thread, NULL );

        while (1)
        {
            struct js_event old_js = js;
            if (read(fd, &js, sizeof(struct js_event)) != sizeof(struct js_event))
            {
                perror("\njsdrive: error reading");
                exit (1);
            }

            old_js = js;
            switch(js.type & ~JS_EVENT_INIT)
            {
                case JS_EVENT_BUTTON:
                    if( js.number == 0 )
                    {
                        tool_down = js.value;
                    }
                    button[js.number] = js.value;
                    break;
                case JS_EVENT_AXIS:
                    axis[js.number] = js.value;
                    break;
            }

            printf("\r");

            pt.x() = (double)(((int) axis[0])+32767) * 6.0 / 65535;
            pt.y() = (double)(((int) axis[1])+32767) * 6.0 / 65535;

            fflush(stdout);
        }
    }

    return -1;
}
