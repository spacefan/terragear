// poly_support.cxx -- additional supporting routines for the FGPolygon class
//                     specific to the object building process.
//
// Written by Curtis Olson, started October 1999.
//
// Copyright (C) 1999  Curtis L. Olson  - curt@flightgear.org
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
// Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
//
// $Id$


#include <simgear/compiler.h>
#include <simgear/constants.h>
#include <simgear/math/fg_types.hxx>
#include <simgear/math/point3d.hxx>

#include <Polygon/polygon.hxx>
#include <Triangulate/trieles.hxx>

#define REAL double
extern "C" {
#include <TriangleJRS/triangle.h>
}
#include <TriangleJRS/tri_support.h>

#include "contour_tree.hxx"
#include "poly_support.hxx"
#include "trinodes.hxx"
#include "trisegs.hxx"


// Given a line segment specified by two endpoints p1 and p2, return
// the slope of the line.
static double slope( const Point3D& p0, const Point3D& p1 ) {
    if ( fabs(p0.x() - p1.x()) > FG_EPSILON ) {
	return (p0.y() - p1.y()) / (p0.x() - p1.x());
    } else {
	return 1.0e+999; // really big number
    }
}


// Given a line segment specified by two endpoints p1 and p2, return
// the y value of a point on the line that intersects with the
// verticle line through x.  Return true if an intersection is found,
// false otherwise.
static bool intersects( Point3D p0, Point3D p1, double x, Point3D *result ) {
    // sort the end points
    if ( p0.x() > p1.x() ) {
	Point3D tmp = p0;
	p0 = p1;
	p1 = tmp;
    }
    
    if ( (x < p0.x()) || (x > p1.x()) ) {
	// out of range of line segment, bail right away
	return false;
    }

    // equation of a line through (x0,y0) and (x1,y1):
    // 
    //     y = y1 + (x - x1) * (y0 - y1) / (x0 - x1)

    double y;

    if ( fabs(p0.x() - p1.x()) > FG_EPSILON ) {
	y = p1.y() + (x - p1.x()) * (p0.y() - p1.y()) / (p0.x() - p1.x());
    } else {
	return false;
    }
    result->setx(x);
    result->sety(y);

    if ( p0.y() <= p1.y() ) {
	if ( (p0.y() <= y) && (y <= p1.y()) ) {
	    return true;
	}
    } else {
 	if ( (p0.y() >= y) && (y >= p1.y()) ) {
	    return true;
	}
    }

    return false;
}


// calculate some "arbitrary" point inside the specified contour for
// assigning attribute areas
Point3D calc_point_inside( const FGPolygon& p, const int contour, 
			   const FGTriNodes& trinodes ) {
    Point3D tmp, min, ln, p1, p2, p3, m, result, inside_pt;
    int min_node_index = 0;
    int min_index = 0;
    int p1_index = 0;
    int p2_index = 0;
    int ln_index = 0;

    // 1. find a point on the specified contour, min, with smallest y

    // min.y() starts greater than the biggest possible lat (degrees)
    min.sety( 100.0 );

    point_list c = p.get_contour(contour);
    point_list_iterator current, last;
    current = c.begin();
    last = c.end();

    for ( int i = 0; i < p.contour_size( contour ); ++i ) {
	tmp = p.get_pt( contour, i );
	if ( tmp.y() < min.y() ) {
	    min = tmp;
	    min_index = trinodes.find( min );
	    min_node_index = i;

	    // cout << "min index = " << *current 
	    //      << " value = " << min_y << endl;
	} else {
	    // cout << "  index = " << *current << endl;
	}
    }

    cout << "min node index = " << min_node_index << endl;
    cout << "min index = " << min_index
	 << " value = " << trinodes.get_node( min_index ) 
	 << " == " << min << endl;

    // 2. take midpoint, m, of min with neighbor having lowest
    // fabs(slope)

    if ( min_node_index == 0 ) {
	p1 = c[1];
	p2 = c[c.size() - 1];
    } else if ( min_node_index == (int)(c.size()) - 1 ) {
	p1 = c[0];
	p2 = c[c.size() - 2];
    } else {
	p1 = c[min_node_index - 1];
	p2 = c[min_node_index + 1];
    }
    p1_index = trinodes.find( p1 );
    p2_index = trinodes.find( p2 );

    double s1 = fabs( slope(min, p1) );
    double s2 = fabs( slope(min, p2) );
    if ( s1 < s2  ) {
	ln_index = p1_index;
	ln = p1;
    } else {
	ln_index = p2_index;
	ln = p2;
    }

    FGTriSeg base_leg( min_index, ln_index, 0 );

    m.setx( (min.x() + ln.x()) / 2.0 );
    m.sety( (min.y() + ln.y()) / 2.0 );
    cout << "low mid point = " << m << endl;

    // 3. intersect vertical line through m and all other segments of
    // all other contours of this polygon.  save point, p3, with
    // smallest y > m.y

    p3.sety(100);
    
    for ( int i = 0; i < (int)p.contours(); ++i ) {
	cout << "contour = " << i << " size = " << p.contour_size( i ) << endl;
	for ( int j = 0; j < (int)(p.contour_size( i ) - 1); ++j ) {
	    // cout << "  p1 = " << poly[i][j] << " p2 = " 
	    //      << poly[i][j+1] << endl;
	    p1 = p.get_pt( i, j );
	    p2 = p.get_pt( i, j+1 );
	    p1_index = trinodes.find( p1 );
	    p2_index = trinodes.find( p2 );
	
	    if ( intersects(p1, p2, m.x(), &result) ) {
		cout << "intersection = " << result << endl;
		if ( ( result.y() < p3.y() ) &&
		     ( result.y() > m.y() ) &&
		     ( base_leg != FGTriSeg(p1_index, p2_index, 0) ) ) {
		    p3 = result;
		}
	    }
	}
	// cout << "  p1 = " << poly[i][0] << " p2 = " 
	//      << poly[i][poly[i].size() - 1] << endl;
	p1 = p.get_pt( i, 0 );
	p2 = p.get_pt( i, p.contour_size( i ) - 1 );
	p1_index = trinodes.find( p1 );
	p2_index = trinodes.find( p2 );
	if ( intersects(p1, p2, m.x(), &result) ) {
	    cout << "intersection = " << result << endl;
	    if ( ( result.y() < p3.y() ) &&
		 ( result.y() > m.y() ) &&
		 ( base_leg != FGTriSeg(p1_index, p2_index, 0) ) ) {
		p3 = result;
	    }
	}
    }
    if ( p3.y() < 100 ) {
	cout << "low intersection of other segment = " << p3 << endl;
	inside_pt = Point3D( (m.x() + p3.x()) / 2.0,
			     (m.y() + p3.y()) / 2.0,
			     0.0 );
    } else {
	cout << "Error:  Failed to find a point inside :-(" << endl;
	inside_pt = p3;
    }

    // 4. take midpoint of p2 && m as an arbitrary point inside polygon

    cout << "inside point = " << inside_pt << endl;

    return inside_pt;
}


// basic triangulation of a polygon contour out adding points or
// splitting edges.  If contour >= 0 just tesselate the specified
// contour.
triele_list polygon_tesselate( const FGPolygon poly, const int contour ) {
    // triangle list
    triele_list elelist;
    struct triangulateio in, out, vorout;
    int counter, offset;

    // point list
    double max_x = poly.get_contour(0)[0].x();
    int total_pts = 0;
    for ( int i = 0; i < poly.contours(); ++i ) {
	if ( (contour < 0) || poly.get_hole_flag(i) || (i == contour) ) {
	    total_pts += poly.contour_size( i );
	}
    }

    in.numberofpoints = total_pts;
    in.pointlist = (REAL *) malloc(in.numberofpoints * 2 * sizeof(REAL));

    counter = 0;
    for ( int i = 0; i < poly.contours(); ++i ) {
	if ( (contour < 0) || poly.get_hole_flag(i) || (i == contour) ) {
	    point_list contour = poly.get_contour( i );
	    for ( int j = 0; j < (int)contour.size(); ++j ) {
		in.pointlist[2*counter] = contour[j].x();
		in.pointlist[2*counter + 1] = contour[j].y();
		if ( contour[j].x() > max_x ) {
		    max_x = contour[j].x();
		}
		++counter;
	    }
	}
    }

    in.numberofpointattributes = 1;
    in.pointattributelist = (REAL *) malloc(in.numberofpoints *
					    in.numberofpointattributes *
					    sizeof(REAL));
    counter = 0;
    for ( int i = 0; i < poly.contours(); ++i ) {
	if ( (contour < 0) || poly.get_hole_flag(i) || (i == contour) ) {
	    point_list contour = poly.get_contour( i );
	    for ( int j = 0; j < (int)contour.size(); ++j ) {
		in.pointattributelist[counter] = contour[j].z();
		++counter;
	    }
	}
    }

    in.pointmarkerlist = (int *) malloc(in.numberofpoints * sizeof(int));
    for ( int i = 0; i < in.numberofpoints; ++i) {
	in.pointmarkerlist[i] = 0;
    }

    // triangle list
    in.numberoftriangles = 0;

    // segment list
    in.numberofsegments = total_pts;
    in.segmentlist = (int *) malloc(in.numberofsegments * 2 * sizeof(int));
    in.segmentmarkerlist = (int *) malloc(in.numberofsegments * sizeof(int));

    counter = 0;
    offset = 0;
    for ( int i = 0; i < poly.contours(); ++i ) {
	point_list contour = poly.get_contour( i );
	for ( int j = 0; j < (int)contour.size() - 1; ++j ) {
	    in.segmentlist[counter++] = i + offset;
	    in.segmentlist[counter++] = i + offset + 1;
	    in.segmentmarkerlist[i] = 0;
	}
	in.segmentlist[counter++] = (int)contour.size() + offset - 1;
	in.segmentlist[counter++] = 0 + offset;
	in.segmentmarkerlist[(int)contour.size() - 1] = 0;

	offset += contour.size();
    }

    // hole list
    int hole_count = 0;
    for ( int i = 0; i < poly.contours(); ++i ) {
	if ( poly.get_hole_flag( i ) ) {
	    ++hole_count;
	}
    }

    in.numberofholes = hole_count + 1;
    in.holelist = (REAL *) malloc(in.numberofholes * 2 * sizeof(REAL));
    counter = 0;
    for ( int i = 0; i < poly.contours(); ++i ) {
	if ( poly.get_hole_flag( i ) ) {
	    in.holelist[counter++] = poly.get_point_inside(i).x();
	    in.holelist[counter++] = poly.get_point_inside(i).y();
	}
    }
    // outside of polygon
    in.holelist[counter++] = max_x + 1.0;
    in.holelist[counter++] = 0.0;

    // region list
    in.numberofregions = 0;
    in.regionlist = (REAL *) NULL;

    // prep the output structures
    out.pointlist = (REAL *) NULL;        // Not needed if -N switch used.
    // Not needed if -N switch used or number of point attributes is zero:
    out.pointattributelist = (REAL *) NULL;
    out.pointmarkerlist = (int *) NULL;   // Not needed if -N or -B switch used.
    out.trianglelist = (int *) NULL;      // Not needed if -E switch used.
    // Not needed if -E switch used or number of triangle attributes is zero:
    out.triangleattributelist = (REAL *) NULL;
    out.neighborlist = (int *) NULL;      // Needed only if -n switch used.
    // Needed only if segments are output (-p or -c) and -P not used:
    out.segmentlist = (int *) NULL;
    // Needed only if segments are output (-p or -c) and -P and -B not used:
    out.segmentmarkerlist = (int *) NULL;
    out.edgelist = (int *) NULL;          // Needed only if -e switch used.
    out.edgemarkerlist = (int *) NULL;    // Needed if -e used and -B not used.
  
    vorout.pointlist = (REAL *) NULL;     // Needed only if -v switch used.
    // Needed only if -v switch used and number of attributes is not zero:
    vorout.pointattributelist = (REAL *) NULL;
    vorout.edgelist = (int *) NULL;       // Needed only if -v switch used.
    vorout.normlist = (REAL *) NULL;      // Needed only if -v switch used.
    
    // TEMPORARY
    // write_out_data(&in);

    // Triangulate the points.  Switches are chosen to read and write
    // a PSLG (p), number everything from zero (z), and produce an
    // edge list (e), and a triangle neighbor list (n).
    // no new points on boundary (Y), no internal segment
    // splitting (YY), no quality refinement (q)

    string tri_options;
    tri_options = "pzYYen";
    cout << "Triangulation with options = " << tri_options << endl;

    triangulate( (char *)tri_options.c_str(), &in, &out, &vorout );

    // TEMPORARY
    // write_out_data(&out);

    // now copy the results back into the corresponding FGTriangle
    // structures

    // triangles
    elelist.clear();
    int n1, n2, n3;
    double attribute;
    for ( int i = 0; i < out.numberoftriangles; ++i ) {
	n1 = out.trianglelist[i * 3];
	n2 = out.trianglelist[i * 3 + 1];
	n3 = out.trianglelist[i * 3 + 2];
	if ( out.numberoftriangleattributes > 0 ) {
	    attribute = out.triangleattributelist[i];
	} else {
	    attribute = 0.0;
	}
	// cout << "triangle = " << n1 << " " << n2 << " " << n3 << endl;

	elelist.push_back( FGTriEle( n1, n2, n3, attribute ) );
    }

    // free mem allocated to the "Triangle" structures
    free(in.pointlist);
    free(in.pointattributelist);
    free(in.pointmarkerlist);
    free(in.regionlist);
    free(out.pointlist);
    free(out.pointattributelist);
    free(out.pointmarkerlist);
    free(out.trianglelist);
    free(out.triangleattributelist);
    // free(out.trianglearealist);
    free(out.neighborlist);
    free(out.segmentlist);
    free(out.segmentmarkerlist);
    free(out.edgelist);
    free(out.edgemarkerlist);
    free(vorout.pointlist);
    free(vorout.pointattributelist);
    free(vorout.edgelist);
    free(vorout.normlist);

    return elelist;
}


// basic triangulation of a polygon with out adding points or
// splitting edges and without regard for holes
static triele_list contour_tesselate( FGContourNode *node, const FGPolygon &p,
				      const FGPolygon &hole_polys,
				      const point_list &hole_pts ) {
    // triangle list
    triele_list elelist;
    struct triangulateio in, out, vorout;
    int counter, start, end;

    // list of points
    int contour_num = node->get_contour_num();
    point_list contour = p.get_contour( contour_num );

    double max_x = contour[0].x();

    int total_pts = contour.size();
    for ( int i = 0; i < hole_polys.contours(); ++i ) {
	total_pts += hole_polys.contour_size( i );
    }

    in.numberofpoints = total_pts;
    in.pointlist = (REAL *) malloc(in.numberofpoints * 2 * sizeof(REAL));

    counter = 0;
    for ( int i = 0; i < (int)contour.size(); ++i ) {
	in.pointlist[2*counter] = contour[i].x();
	in.pointlist[2*counter + 1] = contour[i].y();
	if ( contour[i].x() > max_x ) {
	    max_x = contour[i].x();
	}
	++counter;
    }

    for ( int i = 0; i < hole_polys.contours(); ++i ) {
	point_list hole_contour = hole_polys.get_contour( i );
	for ( int j = 0; j < (int)hole_contour.size(); ++j ) {
	    in.pointlist[2*counter] = hole_contour[j].x();
	    in.pointlist[2*counter + 1] = hole_contour[j].y();
	    if ( hole_contour[j].x() > max_x ) {
		    max_x = hole_contour[j].x();
	    }
	    ++counter;
	}
    }

    in.numberofpointattributes = 1;
    in.pointattributelist = (REAL *) malloc(in.numberofpoints *
					    in.numberofpointattributes *
					    sizeof(REAL));
    counter = 0;
    for ( int i = 0; i < (int)contour.size(); ++i ) {
	in.pointattributelist[counter] = contour[i].z();
	++counter;
    }

    for ( int i = 0; i < hole_polys.contours(); ++i ) {
	point_list hole_contour = hole_polys.get_contour( i );
	for ( int j = 0; j < (int)hole_contour.size(); ++j ) {
	    in.pointattributelist[counter] = hole_contour[j].z();
	    ++counter;
	}
    }

    in.pointmarkerlist = (int *) malloc(in.numberofpoints * sizeof(int));
    for ( int i = 0; i < in.numberofpoints; ++i) {
	in.pointmarkerlist[i] = 0;
    }

    // triangle list
    in.numberoftriangles = 0;

    // segment list
    in.numberofsegments = in.numberofpoints;
    in.segmentlist = (int *) malloc(in.numberofsegments * 2 * sizeof(int));
    in.segmentmarkerlist = (int *) malloc(in.numberofsegments * sizeof(int));
    counter = 0;
    start = 0;
    end = contour.size() - 1;
    for ( int i = 0; i < end; ++i ) {
	in.segmentlist[counter++] = i;
	in.segmentlist[counter++] = i + 1;
	in.segmentmarkerlist[i] = 0;
    }
    in.segmentlist[counter++] = end;
    in.segmentlist[counter++] = start;
    in.segmentmarkerlist[contour.size() - 1] = 0;

    for ( int i = 0; i < hole_polys.contours(); ++i ) {
	point_list hole_contour = hole_polys.get_contour( i );
	start = end + 1;
	end = start + hole_contour.size() - 1;
	for ( int j = 0; j < (int)hole_contour.size() - 1; ++j ) {
	    in.segmentlist[counter++] = j + start;
	    in.segmentlist[counter++] = j + start + 1;
	    in.segmentmarkerlist[i] = 0;
	}
	in.segmentlist[counter++] = end;
	in.segmentlist[counter++] = start;
	in.segmentmarkerlist[contour.size() - 1] = 0;
    }

    // hole list
    in.numberofholes = hole_pts.size() + 1;
    in.holelist = (REAL *) malloc(in.numberofholes * 2 * sizeof(REAL));
    // outside of polygon
    counter = 0;
    in.holelist[counter++] = max_x + 1.0;
    in.holelist[counter++] = 0.0;

    for ( int i = 0; i < (int)hole_pts.size(); ++i ) {
	in.holelist[counter++] = hole_pts[i].x();
	in.holelist[counter++] = hole_pts[i].y();
    }
    // region list
    in.numberofregions = 0;
    in.regionlist = (REAL *) NULL;

    // prep the output structures
    out.pointlist = (REAL *) NULL;        // Not needed if -N switch used.
    // Not needed if -N switch used or number of point attributes is zero:
    out.pointattributelist = (REAL *) NULL;
    out.pointmarkerlist = (int *) NULL;   // Not needed if -N or -B switch used.
    out.trianglelist = (int *) NULL;      // Not needed if -E switch used.
    // Not needed if -E switch used or number of triangle attributes is zero:
    out.triangleattributelist = (REAL *) NULL;
    out.neighborlist = (int *) NULL;      // Needed only if -n switch used.
    // Needed only if segments are output (-p or -c) and -P not used:
    out.segmentlist = (int *) NULL;
    // Needed only if segments are output (-p or -c) and -P and -B not used:
    out.segmentmarkerlist = (int *) NULL;
    out.edgelist = (int *) NULL;          // Needed only if -e switch used.
    out.edgemarkerlist = (int *) NULL;    // Needed if -e used and -B not used.
  
    vorout.pointlist = (REAL *) NULL;     // Needed only if -v switch used.
    // Needed only if -v switch used and number of attributes is not zero:
    vorout.pointattributelist = (REAL *) NULL;
    vorout.edgelist = (int *) NULL;       // Needed only if -v switch used.
    vorout.normlist = (REAL *) NULL;      // Needed only if -v switch used.
    
    // TEMPORARY
    write_tri_data(&in);

    // Triangulate the points.  Switches are chosen to read and write
    // a PSLG (p), number everything from zero (z), and produce an
    // edge list (e), and a triangle neighbor list (n).
    // no new points on boundary (Y), no internal segment
    // splitting (YY), no quality refinement (q)

    string tri_options;
    tri_options = "pzYYen";
    cout << "Triangulation with options = " << tri_options << endl;

    triangulate( (char *)tri_options.c_str(), &in, &out, &vorout );

    // TEMPORARY
    // write_tri_data(&out);

    // now copy the results back into the corresponding FGTriangle
    // structures

    // triangles
    elelist.clear();
    int n1, n2, n3;
    double attribute;
    for ( int i = 0; i < out.numberoftriangles; ++i ) {
	n1 = out.trianglelist[i * 3];
	n2 = out.trianglelist[i * 3 + 1];
	n3 = out.trianglelist[i * 3 + 2];
	if ( out.numberoftriangleattributes > 0 ) {
	    attribute = out.triangleattributelist[i];
	} else {
	    attribute = 0.0;
	}
	// cout << "triangle = " << n1 << " " << n2 << " " << n3 << endl;

	elelist.push_back( FGTriEle( n1, n2, n3, attribute ) );
    }

    // free mem allocated to the "Triangle" structures
    free(in.pointlist);
    free(in.pointattributelist);
    free(in.pointmarkerlist);
    free(in.regionlist);
    free(out.pointlist);
    free(out.pointattributelist);
    free(out.pointmarkerlist);
    free(out.trianglelist);
    free(out.triangleattributelist);
    // free(out.trianglearealist);
    free(out.neighborlist);
    free(out.segmentlist);
    free(out.segmentmarkerlist);
    free(out.edgelist);
    free(out.edgemarkerlist);
    free(vorout.pointlist);
    free(vorout.pointattributelist);
    free(vorout.edgelist);
    free(vorout.normlist);

    return elelist;
}


#if 0
// Find a point inside the polygon without regard for holes
static Point3D point_inside_hole( point_list contour ) {

    triele_list elelist = contour_tesselate( contour );
    if ( elelist.size() <= 0 ) {
	cout << "Error polygon triangulated to zero triangles!" << endl;
	exit(-1);
    }

    FGTriEle t = elelist[0];
    Point3D p1 = contour[ t.get_n1() ];
    Point3D p2 = contour[ t.get_n2() ];
    Point3D p3 = contour[ t.get_n3() ];

    Point3D m1 = ( p1 + p2 ) / 2;
    Point3D m2 = ( p1 + p3 ) / 2;

    Point3D center = ( m1 + m2 ) / 2;

    return center;
}
#endif


// Find a point inside a specific polygon contour taking holes into
// consideration
static Point3D point_inside_contour( FGContourNode *node, const FGPolygon &p ) {
    int contour_num;

    FGPolygon hole_polys;
    hole_polys.erase();

    point_list hole_pts;
    hole_pts.clear();

    // build list of hole points
    for ( int i = 0; i < node->get_num_kids(); ++i ) {
	contour_num = node->get_kid(i)->get_contour_num();
	hole_pts.push_back( p.get_point_inside( contour_num ) );
	point_list contour = p.get_contour( contour_num );
	hole_polys.add_contour( contour, 1 );
    }

    triele_list elelist = contour_tesselate( node, p, hole_polys, hole_pts );
    if ( elelist.size() <= 0 ) {
	cout << "Error polygon triangulated to zero triangles!" << endl;
	exit(-1);
    }

#error what is your point list here?

    FGTriEle t = elelist[0];
    contour_num = node->get_contour_num();
    Point3D p1 = p.get_pt( contour_num, t.get_n1() );
    Point3D p2 = p.get_pt( contour_num, t.get_n2() );
    Point3D p3 = p.get_pt( contour_num, t.get_n3() );
    cout << "  " << p1 << endl << "  " << p2 << endl << "  " << p3 << endl;
    Point3D m1 = ( p1 + p2 ) / 2;
    Point3D m2 = ( p1 + p3 ) / 2;

    Point3D center = ( m1 + m2 ) / 2;

    return center;

}


// recurse the contour tree and build up the point inside list for
// each contour/hole
static void calc_point_inside( FGContourNode *node, FGPolygon &p ) {
    for ( int i = 0; i < node->get_num_kids(); ++i ) {
	if ( node->get_kid( i ) != NULL ) {
	    calc_point_inside( node->get_kid( i ), p );
	}
    }

    int contour_num = node->get_contour_num();
    if ( contour_num >= 0 ) {
	Point3D pi = point_inside_contour( node, p );
	cout << endl << "point inside(" << contour_num << ") = " << pi
	     << endl << endl;
	p.set_point_inside( contour_num, pi );
    }
}


static void print_contour_tree( FGContourNode *node, string indent ) {
    cout << indent << node->get_contour_num() << endl;

    indent += "  ";
    for ( int i = 0; i < node->get_num_kids(); ++i ) {
	if ( node->get_kid( i ) != NULL ) {
	    print_contour_tree( node->get_kid( i ), indent );
	}
    }
}


// Build the contour "is inside of" tree
static void build_contour_tree( FGContourNode *node,
				const FGPolygon &p,
				int_list &avail )
{
    cout << "working on contour = " << node->get_contour_num() << endl;
    cout << "  total contours = " << p.contours() << endl;
    FGContourNode *tmp;

    // see if we are building on a hole or note
    bool flag;
    if ( node->get_contour_num() > 0 ) {
	flag = p.get_hole_flag( node->get_contour_num() );
    } else {
	flag = true;
    }

    // add all remaining hole/non-hole contours as children of the
    // current node if they are inside of it.
    for ( int i = 0; i < p.contours(); ++i ) {
	cout << "  testing contour = " << i << endl;
	if ( p.get_hole_flag( i ) != flag ) {
	    // only holes can be children of non-holes and visa versa
	    if ( avail[i] ) {
		// must still be an available contour
		int cur_contour = node->get_contour_num();
		if ( (cur_contour < 0 ) || p.is_inside( cur_contour, i ) ) {
		    // must be inside the parent (or if the parent is
		    // the root, add all available non-holes.
		    cout << "  adding contour = " << i << endl;
		    avail[i] = 0;
		    tmp = new FGContourNode( i );
		    node->add_kid( tmp );
		} else {
		    cout << "  not inside" << endl;
		}
	    } else {
		cout << "  not available" << endl;
	    }
	} else {
	    cout << "  wrong hole/non-hole type" << endl;
	}
    }

    // if any of the children are inside of another child, remove the
    // inside one
    cout << "node now has num kids = " << node->get_num_kids() << endl;

    for ( int i = 0; i < node->get_num_kids(); ++i ) {
	for ( int j = 0; j < node->get_num_kids(); ++j ) {
	    if ( i != j ) {
		if ( p.is_inside( i, j ) ) {
		    // need to remove contour j from the kid list
		    avail[ node->get_kid( i ) -> get_contour_num() ] = 1;
		    node->remove_kid( i );
		    cout << "removing kid " << i << " which is inside of kid "
			 << j << endl;
		}
	    } else {
		// doesn't make sense to check if a contour is inside itself
	    }
	}
    }

    // for each child, extend the contour tree
    for ( int i = 0; i < node->get_num_kids(); ++i ) {
	tmp = node->get_kid( i );
	if ( tmp != NULL ) {
	    build_contour_tree( tmp, p, avail );
	}
    }
}


// calculate some "arbitrary" point inside the specified contour for
// assigning attribute areas
void calc_points_inside( FGPolygon& p ) {
    // first build the contour tree

    // make a list of all still available contours (all of the for
    // starters)
    int_list avail;
    for ( int i = 0; i < p.contours(); ++i ) {
	avail.push_back( 1 );
    }

    // create and initialize the root node
    FGContourNode *ct = new FGContourNode( -1 );

    // recursively build the tree
    build_contour_tree( ct, p, avail );
    print_contour_tree( ct, "" );

    // recurse the tree and build up the point inside list for each
    // contour/hole
    calc_point_inside( ct, p );

#if 0
    // first calculate an inside point for all holes
    cout << "calculating points for poly with contours = " << p.contours()
	 << endl;

    for ( int i = 0; i < p.contours(); ++i ) {
	if ( p.get_hole_flag( i ) ) {
	    cout << "contour " << i << " is a hole" << endl;
	} else {
	    cout << "contour " << i << " is not a hole" << endl;
	}
    }
    for ( int i = 0; i < p.contours(); ++i ) {
	if ( p.get_hole_flag( i ) ) {
	    cout << "  hole = " << i << endl;
	    Point3D hole_pt = point_inside_hole( p.get_contour( i ) );
	    p.set_point_inside( i, hole_pt );
	}
    }

    // next calculate an inside point for all non-hole contours taking
    // into consideration the holes
    for ( int i = 0; i < p.contours(); ++i ) {
	if ( ! p.get_hole_flag( i ) ) {
	    cout << "  enclosing contour = " << i << endl;
	    Point3D inside_pt = point_inside_contour( p, i );
	    p.set_point_inside( i, inside_pt );
	}
    }
#endif

}