// usgs.hxx -- manage USGS mapping
//
// Written by Ralf Gerlich, started December 2009
//
// Copyright (C) 2009  Ralf Gerlich  - ralf.gerlich <at> custom-scenery <dot> org
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


#ifndef _USGS_HXX
#define _USGS_HXX

#include <string>

#include <simgear/compiler.h>

#include "priorities.hxx"

int load_usgs_map( const std::string& filename );
unsigned int translateUSGSCover( unsigned int usgs_value );

#endif // _USGS_HXX
