// construct.cxx -- Class to manage the primary data used in the
//                  construction process
//
// Written by Curtis Olson, started May 1999.
//
// Copyright (C) 1999  Curtis L. Olson  - http://www.flightgear.org/~curt
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License as
// published by the Free Software Foundation; either version 2 of the
// License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful, but
// WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
// General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA.
//
// $Id: construct.cxx,v 1.4 2004-11-19 22:25:49 curt Exp $

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <simgear/debug/logstream.hxx>

#include "tgconstruct.hxx"
#include "usgs.hxx"

using std::string;

// If we don't offset land use squares by some amount, then we can get
// land use square boundaries coinciding with tile boundaries.
//
// This can foul up our edge matching scheme because a subsequently
// generated adjacent tile might be forced to have edge nodes not
// found in the first tile and not on the shared edge.  This can lead
// to gaps.  If we put skirts around everything that might hide the
// problem.
static const double cover_size          = 1.0 / 120.0;
static const double half_cover_size     = cover_size * 0.5;
static const double quarter_cover_size  = cover_size * 0.25;

// make the area specified area, look up the land cover type, and add
// it to polys
void TGConstruct::make_area( const LandCover &cover, TGPolygon *polys,
                       double x1, double y1, double x2, double y2,
                       double half_dx, double half_dy )
{
    const double fudge = 0.0001;  // (0.0001 degrees =~ 10 meters)

    AreaType area = get_landcover_type( cover,
                                   x1 + half_dx, y1 + half_dy,
                                   x2 - x1, y2 - y1 );

    if ( area != get_default_area_type() ) {
        // Create a square polygon and merge it into the list.
        TGPolygon poly;
        poly.erase();
        poly.add_node(0, Point3D(x1 - fudge, y1 - fudge, 0.0));
        poly.add_node(0, Point3D(x1 - fudge, y2 + fudge, 0.0));
        poly.add_node(0, Point3D(x2 + fudge, y2 + fudge, 0.0));
        poly.add_node(0, Point3D(x2 + fudge, y1 - fudge, 0.0));

        if ( measure_roughness( poly ) < 1.0 ) {
            // Add this poly to the area accumulator
            if ( polys[area].contours() > 0 ) {
                polys[area] = tgPolygonUnion( polys[area], poly );
            } else {
                polys[area] = poly;
            }
        }
    }
}

// Come up with a "rough" metric for the roughness of the terrain
// coverted by a polygon
double TGConstruct::measure_roughness( TGPolygon &poly ) {
    int i;
    unsigned int j;

    // find the elevation range
    double max_z = -9999.0;
    double min_z = 9999.0;

    for ( i = 0; i < poly.contours(); ++i ) {
        point_list points = poly.get_contour( i );
        for ( j = 0; j < points.size(); ++j ) {
            double z;
            z = array.altitude_from_grid( points[j].x() * 3600.0,
                                          points[j].y() * 3600.0 );
            if ( z < -9000 ) {
                z = array.closest_nonvoid_elev( points[j].x() * 3600.0,
                                                points[j].y() * 3600.0 );
            }

            if ( z < min_z ) {
                min_z = z;
            }
            if ( z > max_z ) {
                max_z = z;
            }
        }
    }

    double diff = max_z - min_z;

    // 50m difference in polygon elevation range yields a roughness
    // metric of 1.0.  Less than 1.0 is relatively flat.  More than
    // 1.0 is relatively rough.

    SG_LOG(SG_GENERAL, SG_ALERT, "roughness = " << diff / 50.0 );

    return diff / 50.0;
}

AreaType TGConstruct::get_landcover_type (const LandCover &cover, double xpos, double ypos, double dx, double dy)
{
    // Look up the land cover for the square
    int cover_value = cover.getValue(xpos, ypos);
    AreaType area = translateUSGSCover(cover_value);

    if ( area != get_default_area_type() ) {
        // Non-default area is fine.
        return area;
    } else {
        // If we're stuck with the default area, try to borrow from a
        // neighbour.
        for (double x = xpos - dx; x <= xpos + dx; x += dx) {
            for (double y = ypos - dy; y < ypos + dx; y += dy) {
                if (x != xpos || y != ypos) {
                    cover_value = cover.getValue(x, y);
                    area = translateUSGSCover(cover_value);
                    if (area != get_default_area_type() ) {
                        return area;
                    }
                }
            }
        }
    }

    // OK, give up and return default
    return get_default_area_type();
}


// Generate polygons from land-cover raster.  Horizontally- or
// vertically-adjacent polygons will be merged automatically.
int TGConstruct::load_landcover()
{
    int count = 0;

    try {

        LandCover cover(get_cover());
        TGPolygon polys[TG_MAX_AREA_TYPES];
        TGPolygon poly;     // working polygon

        // Get the lower left (SW) corner of the tile
        double base_lon = bucket.get_center_lon()
            - 0.5 * bucket.get_width()
            - quarter_cover_size;
        double base_lat = bucket.get_center_lat()
            - 0.5 * bucket.get_height()
            - quarter_cover_size;

        SG_LOG(SG_GENERAL, SG_ALERT, "raster land cover: tile at " << base_lon << ',' << base_lat);

        double max_lon = bucket.get_center_lon()
            + 0.5 * bucket.get_width();
        double max_lat = bucket.get_center_lat()
            + 0.5 * bucket.get_height();

        SG_LOG(SG_GENERAL, SG_ALERT, "raster land cover: extends to " << max_lon << ',' << max_lat);

        double x1 = base_lon;
        double y1 = base_lat;
        double x2 = x1 + cover_size;
        double y2 = y1 + cover_size;

        while ( x1 < max_lon ) {
            while ( y1 < max_lat ) {
                make_area( cover, polys, x1, y1, x2, y2, half_cover_size, half_cover_size );

                y1 = y2;
                y2 += cover_size;
            }

            x1 = x2;
            x2 += cover_size;
            y1 = base_lat;
            y2 = y1 + cover_size;
        }

        // Now that we're finished looking up land cover, we have a list
        // of lists of polygons, one (possibly-empty) list for each area
        // type.  Add the remaining polygons to the clipper.
        for ( int i = 0; i < TG_MAX_AREA_TYPES; i++ ) {
            if ( polys[i].contours() ) {
                // TODO : REMOVE add_poly
                add_poly( i, polys[i], get_area_name((AreaType)i ));
                count++;
            }
        }
    } catch ( string e ) {
        SG_LOG(SG_GENERAL, SG_ALERT, "Died with exception: " << e);
        exit(-1);
    }

    // Return the number of polygons actually read.
    return count;
}