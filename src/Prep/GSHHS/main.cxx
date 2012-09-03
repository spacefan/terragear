// main.cxx -- process shapefiles and extract polygon outlines,
//             clipping against and sorting them into the revelant
//             tiles.
//
// Written by Curtis Olson, started February 1999.
//
// Copyright (C) 1999  Curtis L. Olson  - http://www.flightgear.org/~curt
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA.
//
// $Id: main.cxx,v 1.13 2004-11-19 22:25:51 curt Exp $
 

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <simgear/compiler.h>

#include <iostream>
#include <string>

#include <zlib.h>

#include <simgear/debug/logstream.hxx>
#include <simgear/misc/sg_path.hxx>

#include <Polygon/index.hxx>
#include <Polygon/polygon.hxx>

#include "gshhs.h"
#include "gshhs_split.hxx"

using std:: string ;
using std:: cout ;
using std:: endl ;

// hackity, hackity, hack ... cough cough
#ifdef i386
#  define FLIP
#endif


int main( int argc, char **argv ) {
    struct GSHHS h;
    struct GSHHS_POINT p;
    TGPolygon shape;
    double w, e, s, n, area, lon, lat, last_lon, last_lat;
    int k, max_east = 270000000;
    char source;

    sglog().setLogLevels( SG_ALL, SG_DEBUG );

    if ( argc < 4 ) {
	SG_LOG( SG_GENERAL, SG_ALERT, "Usage: " << argv[0] 
		<< " <gshhs_file> <work_dir> <level> [ area_type ]" );
	exit(-1);
    }

    // make work directory
    string work_dir = argv[2];

    SGPath sgp( work_dir );
    sgp.append( "dummy" );
    sgp.create_dir( 0755 );

    // get the specified data level
    int target_level = atoi(argv[3]);
    if ( target_level < 1 || target_level > 4) {
	SG_LOG( SG_GENERAL, SG_ALERT, argv[0] << 
		": level must be 1 (land), 2 (lake), 3 (island), or 4 (pond)");
	exit(-1);
    }

    // allow us to override the area type from the command line.  All
    // polygons in the processed shape file will be assigned this area
    // type
    string force_area_type = "";
    if ( argc == 5 ) {
        force_area_type = argv[4];
    }
	
    // initialize persistant polygon counter
    string counter_file = work_dir + "/../poly_counter";
    poly_index_init( counter_file );

    string path = argv[2];

    gzFile fp;
    if ( (fp = gzopen (argv[1], "rb")) == NULL ) {
        SG_LOG( SG_GENERAL, SG_ALERT, "Cannot open file: " << argv[1] );
        exit(-1);
    }

    SG_LOG( SG_GENERAL, SG_DEBUG, "Opening " << argv[1] << " for reading." );

    while ( gzread(fp, (void *)&h, (unsigned)sizeof(struct GSHHS)) == 
	    (unsigned)sizeof(struct GSHHS) )
    {

	shape.erase();

#ifdef FLIP
	h.id = swabi4 ((unsigned int)h.id);
	h.n = swabi4 ((unsigned int)h.n);
	h.level = swabi4 ((unsigned int)h.level);
	h.west = swabi4 ((unsigned int)h.west);
	h.east = swabi4 ((unsigned int)h.east);
	h.south = swabi4 ((unsigned int)h.south);
	h.north = swabi4 ((unsigned int)h.north);
	h.area = swabi4 ((unsigned int)h.area);
	h.greenwich = swabi2 ((unsigned int)h.greenwich);
	h.source = swabi2 ((unsigned int)h.source);
#endif

	w = h.west  * 1.0e-6;
	e = h.east  * 1.0e-6;
	s = h.south * 1.0e-6;
	n = h.north * 1.0e-6;
	source = (h.source == 1) ? 'W' : 'C';
	area = 0.1 * h.area;

	printf( "P %6d%8d%2d%2c%13.3f%10.5f%10.5f%10.5f%10.5f\n", 
		h.id, h.n, h.level, source, area, w, e, s, n );

	cout << "Loading shape" << endl;

	last_lon = last_lat = -2000.0;

	for ( k = 0; k < h.n; k++ ) {
	    if ( gzread(fp, (void *)&p, (unsigned)sizeof(struct GSHHS_POINT)) !=
		 (unsigned)sizeof(struct GSHHS_POINT) )
	    {
		SG_LOG( SG_GENERAL, SG_ALERT, "Error reading file for polygon "
			<< h.id << " point " << k );
		exit(-1);
	    }
#ifdef FLIP
	    p.x = swabi4 ((unsigned int)p.x);
	    p.y = swabi4 ((unsigned int)p.y);
#endif
	    lon = (h.greenwich && p.x > max_east) ? 
		p.x * 1.0e-6 - 360.0 : 
		p.x * 1.0e-6;
	    lat = p.y * 1.0e-6;
	    // printf ("%10.5lf%10.5lf\n", lon, lat);
	    if ( (last_lon != lon) || (last_lat != lat) ) {
		// test special case for antartica polygon (record ==
		// #34 in the gshhs full data set)
		if ( (fabs(lon - last_lon) > 180) && (last_lon >= -360.0) ) {
		    cout << "SPECIAL HANDLING ANTARTICA LOOP AROUND!" << endl;
		    cout << "k = " << k << "  lon = " << lon 
			 << "  last_lon = " << last_lon << endl;
		    shape.add_node( 0, Point3D(last_lon, -180.0, 0) );
		    shape.add_node( 0, Point3D(lon, -180.0, 0) );
		}
		shape.add_node( 0, Point3D(lon, lat, 0) );
		last_lon = lon;
		last_lat = lat;
	    } else {
		cout << "Skipping a duplicate node!" << endl;
	    }
	}
	max_east = 180000000;	/* Only Eurasiafrica needs 270 */

	if ( h.level != target_level ) {
	    continue;
	}

	// if ( h.id != 2 ) {
	//     continue;
	// }

	SG_LOG( SG_GENERAL, SG_INFO, "  record = " << h.id << " size = " <<
		shape.contour_size( 0 ) );

	FILE *fp = fopen("junk", "w");
	for ( int i = 0; i < shape.contour_size( 0 ); ++i ) {
	    fprintf( fp, "%.6f %.6f\n", 
		     shape.get_pt( 0, i ).x(), shape.get_pt( 0, i ).y() );
	}
	fclose(fp);

	// cout << "press return to continue: ";
	// string response;
	// cin >> response;
	// cout << "waiting 2 seconds ..." << endl;
	// sleep(2);

	if ( shape.contour_size( 0 ) > 16000 ) {
	    // use my crude line clipping routine to whittle down the
	    // huge polygons into something more managable

	    if ( force_area_type.length() > 0 ) {
		gshhs_split_polygon(path, force_area_type, shape, s, n);
	    } else {
		gshhs_split_polygon(path, "Default", shape, s, n);
	    }
	} else {
	    // small enough to feed to clipper directly
	    if ( force_area_type.length() > 0 ) {
		split_and_shift_chunk(path, force_area_type, shape);
	    } else {
		split_and_shift_chunk(path, "Default", shape);
	    }
	}

    }

    return 0;
}


