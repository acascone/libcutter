//This file adapted from http://sites.google.com/site/drbobbobswebsite/cricut-gcode-interpreter

#include <eigen3/Eigen/Geometry>

#include <errno.h>
#include <cmath>
#include <map>
#include <vector>
#include <iostream>
#include <fstream>
#include <cstdlib>
#include <cstring>
#include <cctype>
#include <stdexcept>
#include "device.hpp"
#include "gcode.hpp"

using namespace std;
using namespace gcode_base;

static enum debug_prio _debug;

void gcode_base::debug_out(enum debug_prio debug_level, const string msg)
{
     if (debug_level <= _debug)
	  std::cout << msg << std::endl;
}

void gcode_base::set_debug(enum debug_prio d)
{
     static const char * const debug_strings[] = {
          "critical",
          "error",
          "warning",
          "information",
          "debug",
          "extra_debug"
     };

     if( d > extra_debug ) {
          _debug = extra_debug;
          debug_out(info, "Debugging level set to maximum");
     } else {
          debug_out(info, "Debugging level set to "+std::string(debug_strings[_debug=d]));
     }
}

line::line(const xy &s, const xy &e, const bool c):
     start(s),
     end(e),
     cut(c)
{
     std::stringstream buf;
     buf<<"New line: start "<<start<<", end "<<end <<" cut " <<( cut ? "true": "false");
     debug_out(extra_debug, buf.str());
}

line::~line()
{
}

xy line::draw(Device::Generic &cutter)
{
     std::stringstream buf;
     buf << ( cut ? "Line" : "Rapid move") << " from "<< start <<" to "<<end,
     debug_out(info, buf.str());

     if (cut)
	  cutter.cut_to(end);
     else
	  cutter.move_to(end);

     buf.seekp(0).clear();
     buf << "Current position: " << end;
     debug_out(debug, buf.str());
     return end;
}

bezier::bezier(const xy &s, const xy &c1, const xy &c2, const xy &e):
     start(s),
     cp1(c1),
     cp2(c2),
     end(e)
{
     std::stringstream buf;
	buf<< "New bezier from "<< start <<" to "<<end;
     debug_out(extra_debug,buf.str());
}

bezier::~bezier()
{
}

xy bezier::draw(Device::Generic &cutter)
{
     std::stringstream buf;

     buf<< "Bezier segment from "<< start <<" to "<<end;
     debug_out(info, buf.str());
     cutter.curve_to(start, cp1, cp2, end);
     buf.seekp(0).clear();
     buf<<"Current position: "<<end;
     debug_out(debug, buf.str());
     return end;
}

double arc::angle_between(const xy &vec1, const xy &vec2)
{
     return atan2(vec2.y(), vec2.x()) - atan2(vec1.y(), vec1.x());
}
double arc::get_arcwidth(const xy & vec1, const xy & vec2)
{
//     printf("vec1: (%f, %f); vec2: (%f, %f)\n",vec1.x, vec1.y,
//	    vec2.x, vec2.y);
     double a1 = atan2(vec2.y(), vec2.x()) - atan2(vec1.y(), vec1.x());
//     printf("Arcwidth: %f", a1/M_PI);

     if (clockwise)
     {
//	  printf(" (clockwise) ");
	  if (a1 > 0)
	       a1 = a1 - 2*M_PI;
     }
     else
     {
//	  printf(" (anticlockwise) ");
	  if (a1 < 0)
	       a1 = a1 + 2*M_PI;
     }
//     printf(" Final: %f\n", a1/M_PI);
     return a1;
}

arc::arc(const xy &c, const xy &t, const xy &cv, const bool cw):
     current(c),
     target(t),
     cvec(cv),
     clockwise(cw),
     k((4.0/3.0)*(sqrt(2.0) - 1.0))
{
     std::stringstream buf;
     buf << "New "<< (clockwise ? "clockwise" : "anticlockwise") << " arc from " << current <<" to "<<target;
     debug_out(extra_debug, buf.str());

     cseg = 0;

     center = current + cvec;

     // target vector and current vector
     auto tvec = target - center;
     cvec = -cvec;
     radius = cvec.norm();

     // We implement this using arcs of at most 90 degrees.
     // For arcs subtending more than 90 degrees we create
     // a list of 90 degree segments and then a sub-90
     // degree segment to complete the process.
     arcwidth = abs(get_arcwidth(cvec, tvec));

     // this is the angle between the start point and the
     // x-axis - we create the segments around the x-axis
     // and origin, then rotate them around the origin and 
     // translate to the defined center point.
     xy x_axis{radius, 0};
     crot = angle_between(x_axis, cvec);

     // we rotate each segment so that the start point of the segment
     // is at the same angle as the current point, then we increment
     // by the arcwidth of the segment.
     double srot = 0;
     double rem = arcwidth;
     while (rem > M_PI_2)
     {
	  // insert a 90 degree segment, rotated 
	  segment(M_PI_2, srot);
	  rem -= M_PI_2;
	  srot += M_PI_2;
     }
     // final arc segment
     segment(rem, srot);
}

arc::~arc()
{
}

xy arc::draw(Device::Generic &cutter)
{
     std::stringstream buf;
     buf << "Arc from "<< current << " to " << target;
     debug_out(info, buf.str());

     xy end;
     for(int i = 0; i < cseg; i++)
     {
	  if(segments[i] != NULL)
	       end = segments[i]->draw(cutter);
     }

     if ((end - target).norm() > 0.000001)
	  debug_out(warn, "Segment end points do not equal arc end points");
     buf.seekp(0).clear();
     buf<< "Current position: "<<target;
     debug_out(debug, buf.str());
     return target;
}

// a single sub-90 degree segment. th0 is the total angle subtended by
// the arc, r is the arc radius, and rot is the rotation required to move
// the arc to its final position
//
// Reference: http://hansmuller-flex.blogspot.com.au/2011/04/approximating-circular-arc-with-cubic.html
// (accessed 2014-12-24)
void arc::segment(double swidth, double rot)
{
     std::stringstream buf;
     buf<<"Arc segment: center "<<center<<", arc width: " <<swidth/M_PI<< ", radius " << radius << ", rotation: " <<rot/M_PI;
     debug_out(debug, buf.str());
     xy pt1, pt2, pt3, pt4;
     double a = swidth/2;

     if (clockwise)
     {
	  // construct the arc segment around the x-axis
	  pt1.x() = radius*cos(a);
	  pt1.y() = radius*sin(a);
	  pt4.x() = pt1.x();
	  pt4.y() = -pt1.y();
	  // get the control points
	  pt3.x() = pt4.x() + k * tan(a) * pt1.y();
	  pt3.y() = pt4.y() + k * tan(a) * pt1.x();
	  pt2.x() = pt3.x();
	  pt2.y() = -pt3.y();
	  // this segment was created with pt1 pointing at a - it
	  // needs to be rotated by -a to point pt1 at the x axis,
	  // then by crot to point it at the current point. It then
	  // needs to be rotated by -rot: i.e. (-a + crot -rot), or
	  // crot - a - rot
	  rot = crot - a - rot;
     }
     else
     {
	  // construct the arc segment around the x-axis
	  pt4.x() = radius*cos(a);
	  pt4.y() = radius*sin(a);
	  pt1.x() = pt4.x();
	  pt1.y() = -pt4.y();
	  // get the control points
	  pt2.x() = pt1.x() + k * tan(a) * pt4.y();
	  pt2.y() = pt1.y() + k * tan(a) * pt4.x();
	  pt3.x() = pt2.x();
	  pt3.y() = -pt2.y();
	  // this time pt1 is pointing at -a, so the same set of
	  // rotations (except for using +rot rather than -rot, since
	  // this is anticlockwise) works out as (-(-a) + crot + rot),
	  // or rot + crot + a
	  rot = rot + crot + a;
     }

     // rotate to the right spot
     Eigen::Rotation2Dd rotation{rot};
     pt1 = rotation * pt1 + center;
     pt2 = rotation * pt2 + center;
     pt3 = rotation * pt3 + center;
     pt4 = rotation * pt4 + center;

     // and put it into production . . .
     segments[cseg++] = new bezier( pt1, pt2, pt3, pt4 );
}

//
// Parsing a line
//
// The g-code format is line-oriented, and consists of one or more
// g-code commands per line. Some commands simply set general state
// configuration, others take options and values. Comments start with
// a ';' character and continue to the end of the line, or are
// contained within paired (but not nested) parentheses.
//
// Commands can start with one of a number of characters, most often G
// and M, with all the important ones being G commands. We'll handle a
// small chunk of the G commands, one or two of the M commands, and a
// couple of other miscellaneous things.
//
// G commands that will be handled are 0 (rapid movement), 1 (line), 2
// (clockwise circular arc), 3 (anticlockwise circular arc), 20 and 21
// (inches or millimeters mode), 90 and 91 (absolute or incremental
// positioning). M commands that will be handled are 0 and 2
// (compulsory stop, and end program). Finally we'll recognise N-code
// line numbers, and we'll obviously parse the XYZ and IJK options
// that are associated with the G commands.
//
// The actual implementation will be a recursive, character oriented
// parser - read a character, and depending on its value call a
// function to process subsequent characters.
//
// Each function will return any remaining substring that hasn't been
// processed - i.e. once it's parsed a command to completion it will
// return the remaining unparsed string.
//

gcode::gcode(Device::Generic & c):
     cutter(c)
{
     filename = string("");
     pen_up = true;
     metric = true;
     absolute = true;
}

gcode::gcode(const std::string & fname, Device::Generic & c):
     cutter(c)
{
     filename = fname;
     pen_up = true;
     metric = true;
     absolute = true;
}

gcode::~gcode()
{
}

void gcode::set_input(const std::string & fname)
{
     filename = fname;
}

void gcode::set_cutter(Device::Generic & c)
{
     cutter = c;
}

double gcode::doc_to_internal(double val)
{
     if(metric)
	  return (val/MM_PER_INCH);
     return val;
}

xy gcode::get_xy(std::map<char,float> & codes)
{
	xy retn = curr_pos;
	if( codes.find('X') != codes.end() ) {
		retn.x() = doc_to_internal(codes['X']);
	}
	if( codes.find('Y') != codes.end() ) {
		retn.y() = doc_to_internal(codes['Y']);
	}
	return retn;
}

xy gcode::get_vector(std::map<char,float> & codes)
{
	return { doc_to_internal(codes['I']), doc_to_internal(codes['J']) };
}

void gcode::process_movement(std::map<char,float> & codes)
{
	// rapid movement to target point
	process_z_code(codes);
	xy target = get_xy(codes);
	line *l = new line(curr_pos, target, false);
	curr_pos = l->draw(cutter);
}

void gcode::process_z_code(std::map<char,float> & codes)
{
     if ( codes.find('Z') != codes.end() )
     {
	  double z = doc_to_internal(codes['Z']);
	  debug_out(debug, "Pen " + std::string((z >= 0) ? "up":"down"));
	  if(z >= 0)
	       raise_pen();
	  else
	       lower_pen();
     }
}

void gcode::process_line(std::map<char,float> & codes)
{
     // cut from curr_pos to target_point

     process_z_code(codes);
     xy target = get_xy(codes);
     line *l = new line(curr_pos, target, true);
     curr_pos = l->draw(cutter);
}

void gcode::process_clockwise_arc(std::map<char,float> & codes)
{
     // cut in a clockwise circular arc from curr_pos to target around
     // a center point defined by the vector (i, j) from the current
     // position

     process_z_code(codes);
     xy target = get_xy(codes);
     xy cvec = get_vector(codes);

     debug_out(debug, "Processing clockwise arc");

     arc *a = new arc(curr_pos, target, cvec, true);
     curr_pos = a->draw(cutter);
}

void gcode::process_anticlockwise_arc(std::map<char,float> & codes)
{
    // cut in an anticlockwise circular arc from curr_pos to target
    // around a center point defined by the vector (i, j) from the
    // current position

    process_z_code(codes);

	 debug_out(debug, "Processing anticlockwise arc");
	 xy target = get_xy(codes);
	 xy cvec = get_vector(codes);
	 arc *a = new arc(curr_pos, target, cvec, false);
	 curr_pos = a->draw(cutter);
}

void gcode::process_g_code(std::map<char,float> & codes)
{
     string val;

     int code = (int)(codes['G']+.5);
     debug_out(debug, "Processing G code: " + std::to_string(code));
     switch(code)
     {
     case 0:
	  // rapid movement to target point
	  process_movement(codes);
	  break;
     case 1:
	  // cut a line from curr_pos to target point
	  process_line(codes);
	  break;
     case 2:
	  // clockwise circular arc from curr_pos to target, around
	  // the center point specified by the vector (i, j)
	  process_clockwise_arc(codes);
	  break;
     case 3:
	  // anticlockwise circular arc, as per case 2
	  process_anticlockwise_arc(codes);
	  break;
     case 20:
	  // input values are in inches
	  debug_out(info, "Switching to imperial units");
	  metric = false;
	  break;
     case 21:
	  // input values are in millimeters
	  debug_out(info, "Switching to metric units");
	  metric = true;
	  break;
     case 90:
	  // values are absolute
	  debug_out(info, "Using absolute coordinates");
	  absolute = true;
	  break;
     case 91:
	  // values are relative to the current point
	  // not supported at the moment, so we do nothing
	  debug_out(info, "Relative coordinates requested but not supported");
	  break;
     default:
	  debug_out(debug, "Unhandled G command: " + std::to_string(code));
	  break;
     }
}

void gcode::process_line_number(std::map<char,float> & codes)
{
     debug_out(debug, "Skipping line number");
}

void gcode::process_misc_code(std::map<char,float> & codes)
{
     int code = static_cast<int>(codes['M']+0.5);
     debug_out(debug, "Processing M code: " + std::to_string(code));

     switch(code)
     {
     case 0:
     case 1:
     case 2:
	  // stop the program
	  debug_out(info, "Program halted");
	  throw false;
	  break;
     default:
	  debug_out(debug, "Unhandled M command " + std::to_string(code));
	  break;
     }

}

std::map<char,float> gcode::parse_gcode( std::string line )
{
std::map<char,float> results;
size_t i;
unsigned paren_count = 0;
for( i = 0; i < line.length(); ++i )
    {
    if( paren_count > 0 )
        {
        if( line[i] == ')')
            paren_count--;
        continue;
        }
    else if( line[i] == '(' )
        {
        paren_count++;
        continue;
        }
    else if( line[i] == ';' )
        {
        break;
        }
    else if( isalpha(line[i]))
        {
        char key;
        float value;
        int len;
        if( 2 == sscanf( line.c_str()+i, "%c%f%n", &key, &value, &len ) )
            {
            results[key]=value;
            i = i + len - 1;
            continue;
            }
        }
    else if( isspace(line[i]) )
        {
        continue;
        }
    std::cerr<<"Did not understand:"<<line<<std::endl;
    }
return results;
}

void gcode::parse_line(string input)
{
     debug_out(extra_debug, "Processing line: "+input);
     std::map<char,float> codes = parse_gcode( input );

     if ( codes.find('G') != codes.end() ) {
          process_g_code(codes);
     } else if ( codes.find('N') != codes.end() ) {
          process_line_number(codes);
     } else if ( codes.find('M') != codes.end() ) {
          process_misc_code(codes);
     } else {
	  debug_out(debug, "Unhandled command " + input);
     }
}

void gcode::parse_file(void)
{
     ifstream infile(filename.c_str());
     string line;

     while(infile)
     {
	  std::getline(infile, line);
	  try
	  {
	       parse_line(line);
	  }
	  catch(string msg)
	  {
	       debug_out(err, msg);
	  }
	  catch(const std::out_of_range& oor)
	  {
	       // sadly, this seems to be the most reliable way to
	       // tell that we've reached the end of the line
	       debug_out(extra_debug, "Got out of range error");
	  }
	  catch(bool completed)
	  {
	       // this flags a stop command
	       if(!completed)
		    return;
	       // otherwise the line is done
	  }
     }
     debug_out(info, "Parse complete");
     return;
}
