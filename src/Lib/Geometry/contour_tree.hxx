// contour_tree.hxx -- routines for building a contour tree showing
//                     which contours are inside if which contours
//
// Written by Curtis Olson, started June 2000.
//
// Copyright (C) 2000  Curtis L. Olson  - curt@flightgear.org
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


#ifndef _CONTOUR_TREE_HXX
#define _CONTOUR_TREE_HXX


#ifndef __cplusplus                                                          
# error This library requires C++
#endif                                   


#include <simgear/compiler.h>

#include <vector>

FG_USING_STD(vector);


// forward declaration
class FGContourNode;

typedef vector < FGContourNode * > contour_kids;
typedef contour_kids::iterator contour_kids_iterator;
typedef contour_kids::const_iterator const_contour_kids_iterator;


// a simple class for building a contour tree for a polygon.  The
// contour tree shows the hierarchy of which contour is inside which
// contour.

class FGContourNode {

private:

    int contour_num;		// -1 for the root node
    contour_kids kids;

public:

    FGContourNode();
    FGContourNode( int n );

    ~FGContourNode();

    inline int get_contour_num() const { return contour_num; }
    inline void set_contour_num( int n ) { contour_num = n; }

    inline int get_num_kids() const { return kids.size(); }
    inline FGContourNode *get_kid( int n ) const { return kids[n]; }
    inline void add_kid( FGContourNode *kid ) { kids.push_back( kid ); }
    inline void remove_kid( int n ) {
	cout << "kids[" << n << "] = " << kids[n] << endl;
	delete kids[n];
	kids[n] = NULL;
    }
};


#endif // _CONTOUR_TREE_HXX

