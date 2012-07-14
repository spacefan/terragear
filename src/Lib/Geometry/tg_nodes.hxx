#ifndef _TG_NODES_HXX
#define _TG_NODES_HXX


#ifndef __cplusplus                                                          
# error This library requires C++
#endif                                   

#include <simgear/compiler.h>
#include <Geometry/point3d.hxx>
#include <simgear/math/sg_types.hxx>

#define FG_PROXIMITY_EPSILON 0.000001
#define FG_COURSE_EPSILON 0.0001

typedef std::vector < unsigned int > TGIdxList;   

class TGNode {
public:
    TGNode( Point3D p ) {
        position    = p;
        normal      = Point3D();
        

        fixed_position  = false;        // no matter what - don't move x, y, or z (likely a hole around an airport generated ny genapts)
        fixed_normal    = false;        // no matter what - don't modify the normal - likely on a normal generated on a shared edge 

        faces.clear();
        neighbors.clear();
    }

    inline void SetFixedPosition( bool fix )        
    { 
        if (!fixed_position) {
            fixed_position = fix;
        }
    }

    inline bool GetFixedPosition( void ) const      { return fixed_position; }
    inline void SetFixedNormal( bool fix )          { fixed_normal = fix; }
    inline bool GetFixedNormal( void ) const        { return fixed_normal; }

    inline void    SetPosition( const Point3D& p )  
    { 
        if (!fixed_position) {
            position = p; 
        }
    }

    inline void    SetElevation( double z )
    { 
        if (!fixed_position) {
            position.setelev( z ); 
        }
    }

    inline Point3D GetPosition( void ) const        { return position; }
    inline void    SetNormal( const Point3D& n )    { normal = n; }
    inline Point3D GetNormal( void ) const          { return normal; }

private:
    Point3D     position;
    Point3D     normal;

    bool        fixed_position;
    bool        fixed_normal;

    TGIdxList   faces;
    TGIdxList   neighbors;
};
typedef std::vector < TGNode > node_list;
typedef node_list::iterator node_list_iterator;
typedef node_list::const_iterator const_node_list_iterator;


/* This class handles ALL of the nodes in a tile : 3d nodes in elevation data, 2d nodes generated from landclass, etc) */

class TGNodes {
public:

    // Constructor and destructor
    TGNodes( void )     {}
    ~TGNodes( void )    {}

    // delete all the data out of node_list
    inline void clear() { tg_node_list.clear(); }

    // Add a point to the point list if it doesn't already exist.
    // Returns the index (starting at zero) of the point in the list.
    int unique_add( const Point3D& p );

    // Add a point to the point list if it doesn't already exist
    // (checking all three dimensions.)  Returns the index (starting
    // at zero) of the point in the list.
    int unique_add_fixed_elevation( const Point3D& p );
    node_list get_fixed_elevation_nodes( void ) const;


    // Add the point with no uniqueness checking
    int simple_add( const Point3D& p );

    // Add a point to the point list if it doesn't already exist.
    // Returns the index (starting at zero) of the point in the list.
    // Use a course proximity check
    int course_add( const Point3D& p );

    // Find the index of the specified point (compair to the same
    // tolerance as unique_add().  Returns -1 if not found.
    int find( const Point3D& p ) const;

//    bool LookupFixedElevation( Point3D p, double* z );
    void SetElevation( int idx, double z )  { tg_node_list[idx].SetElevation( z ); }

     // return the master node list
    inline node_list& get_node_list() { return tg_node_list; }
    inline const node_list& get_node_list() const { return tg_node_list; }

    // return a point list of geodetic nodes
    point_list get_geod_nodes() const;

    // return a point list of wgs84 nodes
    std::vector< SGVec3d > get_wgs84_nodes_as_SGVec3d() const;
    point_list get_wgs84_nodes_as_Point3d() const;

    // return a point list of normals
    point_list get_normals() const;

    // return the ith point
    inline TGNode get_node( int i ) const { return tg_node_list[i]; }

    // return the size of the node list
    inline size_t size() const { return tg_node_list.size(); }

private:
    node_list tg_node_list;

    // return true of the two points are "close enough" as defined by
    // FG_PROXIMITY_EPSILON
    bool close_enough_2d( const Point3D& p1, const Point3D& p2 ) const;

    // return true of the two points are "close enough" as defined by
    // FG_PROXIMITY_EPSILON
    bool close_enough_3d( const Point3D& p1, const Point3D& p2 ) const;

    // return true of the two points are "close enough" as defined by
    // FG_COURSE_EPSILON
    bool course_close_enough( const Point3D& p1, const Point3D& p2 );
};


// return true of the two points are "close enough" as defined by
// FG_PROXIMITY_EPSILON checking just x and y dimensions
inline bool TGNodes::close_enough_2d( const Point3D& p1, const Point3D& p2 )
    const
{
    if ( ( fabs(p1.x() - p2.x()) < FG_PROXIMITY_EPSILON ) && 
         ( fabs(p1.y() - p2.y()) < FG_PROXIMITY_EPSILON ) ) {
        return true;
    } else {
        return false;
    }
}


// return true of the two points are "close enough" as defined by
// FG_PROXIMITY_EPSILON check all three dimensions
inline bool TGNodes::close_enough_3d( const Point3D& p1, const Point3D& p2 )
    const
{
    if ( ( fabs(p1.x() - p2.x()) < FG_PROXIMITY_EPSILON ) &&
         ( fabs(p1.y() - p2.y()) < FG_PROXIMITY_EPSILON ) &&
         ( fabs(p1.z() - p2.z()) < FG_PROXIMITY_EPSILON ) ) {
        return true;
    } else {
        return false;
    }
}


// return true of the two points are "close enough" as defined by
// FG_COURSE_EPSILON
inline bool TGNodes::course_close_enough( const Point3D& p1, const Point3D& p2 )
{
    if ( ( fabs(p1.x() - p2.x()) < FG_COURSE_EPSILON ) &&
         ( fabs(p1.y() - p2.y()) < FG_COURSE_EPSILON ) ) {
        return true;
    } else {
        return false;
    }
}

#endif // _TG_NODES_HXX
