#ifndef __TG_MESH_TRIANGULATION_HXX__
#define __TG_MESH_TRIANGULATION_HXX__

#include "tg_mesh.hxx"

// forward declarations
class tgMesh;
class tgMeshArrangement;


// Shared edge definitions and search trees
typedef enum {
    NORTH_EDGE,
    SOUTH_EDGE,
    EAST_EDGE,
    WEST_EDGE
} edgeType;

#if 0
// search tree for nodes one the that are moved to match neighbor tiles
typedef CGAL::Search_traits_2<meshTriKernel>                                                                                            moveNodeBaseTraits;
typedef boost::tuple<meshTriPoint,meshTriPoint, int>                                                                                    moveNodeData;
typedef CGAL::Search_traits_adapter<moveNodeData,CGAL::Nth_of_tuple_property_map<0, moveNodeData>,moveNodeBaseTraits>                   moveNodeTraits;
typedef CGAL::Fuzzy_sphere<moveNodeTraits>                                                                                              moveNodeFuzzyCir;
typedef CGAL::Kd_tree<moveNodeTraits>                                                                                                   moveNodeTree;
#endif

class movedNode {
public:
    movedNode( meshTriVertexHandle vh, meshTriPoint op, meshTriPoint np ) : oldPositionHandle(vh), oldPosition(op), newPosition(np) {}

    meshTriVertexHandle     oldPositionHandle;
    meshTriPoint            oldPosition;
    meshTriPoint            newPosition;
};

// search tree for shared nodes between two tiles ( along with the membership of which tile node came from )
typedef enum {
    NODE_CURRENT,
    NODE_NEIGHBOR,
    NODE_BOTH
} nodeMembership;

// the node membership search tree contains the lookup point, membership info, and an index into vertexInfo if a member of the current tile (NODE_CURRENT or NODE_BOTH)
// index is -1 if NODE_NEIGHBOR
typedef CGAL::Search_traits_2<meshTriKernel>                                                                                            nodeMembershipTraitsBase;
typedef boost::tuple<meshTriPoint,nodeMembership,int>                                                                                   nodeMembershipData;
typedef CGAL::Search_traits_adapter<nodeMembershipData,CGAL::Nth_of_tuple_property_map<0, nodeMembershipData>,nodeMembershipTraitsBase> nodeMembershipTraits;
typedef CGAL::Orthogonal_k_neighbor_search<nodeMembershipTraits>                                                                        nodeMembershipSearch;
typedef nodeMembershipSearch::Tree                                                                                                      nodeMembershipTree;

// search tree for nodes finding closest node to a position
//typedef CGAL::Search_traits_2<meshTriKernel>                                                                                            edgeNodeTraits;
//typedef CGAL::Orthogonal_k_neighbor_search<edgeNodeTraits>                                                                              edgeNodeSearch;
//typedef edgeNodeSearch::Tree                                                                                                            edgeNodeTree;

// search tree for finding nodes on a shared edge ( just position )
//typedef CGAL::Search_traits_2<meshTriKernel>                                                                                            findNodeTraits;
//typedef CGAL::Fuzzy_iso_box<findNodeTraits>                                                                                             findNodeFuzzyBox;
//typedef CGAL::Kd_tree<findNodeTraits>                                                                                                   findNodeTree;

typedef CGAL::Search_traits_2<meshTriKernel>                                                                                            findVertexInfoTraitsBase;
typedef boost::tuple<meshTriPoint,int>                                                                                                  findVertexInfoData;
typedef CGAL::Search_traits_adapter<findVertexInfoData,CGAL::Nth_of_tuple_property_map<0, findVertexInfoData>,findVertexInfoTraitsBase> findVertexInfoTraits;
typedef CGAL::Fuzzy_iso_box<findVertexInfoTraits>                                                                                       findVertexInfoFuzzyBox;
typedef CGAL::Kd_tree<findVertexInfoTraits>                                                                                             findVertexInfoTree;


// search tree for finding nodes on a shared edge ( position and vertex handle in triangulation ) 
typedef CGAL::Search_traits_2<meshTriKernel>                                                                                            findVertexTraitsBase;
typedef boost::tuple<meshTriPoint,meshTriVertexHandle>                                                                                  findVertexData;
typedef CGAL::Search_traits_adapter<findVertexData,CGAL::Nth_of_tuple_property_map<0, findVertexData>,findVertexTraitsBase>             findVertexTraits;
typedef CGAL::Fuzzy_iso_box<findVertexTraits>                                                                                           findVertexFuzzyBox;
typedef CGAL::Kd_tree<findVertexTraits>                                                                                                 findVertexTree;

class tgMeshTriangulation
{
public:
    tgMeshTriangulation( tgMesh* m ) { mesh = m; }

    void constrainedTriangulateWithEdgeModification( const tgMeshArrangement& arr );
    void constrainedTriangulateWithoutEdgeModification( const std::vector<movedNode>& movedPoints, const std::vector<meshTriPoint>& addedPoints );

    void clearDomains(void);
    void markDomains( const tgMeshArrangement& arr );
    void markDomains(meshTriFaceHandle start, meshArrFaceConstHandle face, std::list<meshTriEdge>& border );

    void clear( void ) {
        meshTriangulation.clear();
        vertexInfo.clear();
        faceInfo.clear();
    }

    // 2d triangulation shared edge matching - save edges
    void saveSharedEdgeNodes( const std::string& path ) const;

    // 2d triangulation shared edge matching - match current and neighbot nodes
    void matchNodes( edgeType edge, std::vector<meshVertexInfo>& current, std::vector<meshVertexInfo>& neighbor, std::vector<meshTriPoint>& addedNodes, std::vector<movedNode>& movedNodes );

    bool loadTriangulation( const std::string& path, const SGBucket& bucket );


    void saveSharedEdgeFaces( const std::string& path ) const;
    void saveIncidentFaces( const std::string& path, const char* layer, const std::vector<const meshVertexInfo *>& vertexes ) const;


    void calcTileElevations( const tgArray* array );

    // ********** Triangulation I/O **********
    // 
    // Main APIs
    void toShapefile( const std::string& datasource, const char* layer, bool marked ) const;

    // Save points only
    void toShapefile( const std::string& datasource, const char* layer, const std::vector<meshTriPoint>& points ) const;
    void toShapefile( OGRLayer* poLayer, const meshTriPoint& pt, const char* desc ) const;

    // save points with vertex info
    void toShapefile( const std::string& datasource, const char* layer, std::vector<const meshVertexInfo *>& points ) const;
    void toShapefile( OGRLayer* poLayer, const meshVertexInfo* vip) const;

    // save face info
    void toShapefile( OGRLayer* poLayer, const meshFaceInfo& fi) const;

    // helper - read vertex info from feature
    void fromShapefile( const OGRFeatureDefn* poFDefn, OGRCoordinateTransformation* poCT, OGRFeature* poFeature, std::vector<meshVertexInfo>& points ) const;

    // helper - read face info from feature
    void fromShapefile( const OGRFeatureDefn* poFDefn, OGRCoordinateTransformation* poCT, OGRFeature* poFeature, std::vector<meshFaceInfo>& faces ) const;

    // debug i/o
    void toShapefile( const std::string& datasource, const char* layer, const std::vector<movedNode>& nodes );
    void toShapefile( const std::string& datasource, const char* layer, const nodeMembershipTree& tree );
    void toShapefile( OGRLayer* poLayer, const meshTriSegment& seg, const char* desc ) const;

    // loading stage 1 shared edge data
    void fromShapefile( const std::string& filename, std::vector<meshVertexInfo>& points ) const;

    // loading stage 1 triangulation 
    void fromShapefile( const std::string& filename, std::vector<meshFaceInfo>& faces ) const;

    bool loadTds( const std::string& bucketPath );

    void prepareTds( void );
    void saveTds( const std::string& bucketPath ) const;

private:
    void loadStage1SharedEdge( const std::string& p, const SGBucket& b, edgeType edge, std::vector<meshVertexInfo>& points );
    void sortByLat( std::vector<meshVertexInfo>& points ) const;
    void sortByLon( std::vector<meshVertexInfo>& points ) const;

    void getEdgeNodes( std::vector<const meshVertexInfo *>& north, std::vector<const meshVertexInfo *>& south, std::vector<const meshVertexInfo *>& east, std::vector<const meshVertexInfo *>& west ) const;

    const meshVertexInfo* findVertex( int idx ) const;

    // DEBUG
    void saveCDTAscii( const std::string& datasource, const char* filename ) const;
    void saveTdsAscii( const std::string& datasource, const char* layer ) const;

    void saveFlippable( const char* layer ) const;
    void saveConstrained( const char* layer ) const;

    void saveEdgeBoundingBox( const meshTriPoint& lr, const meshTriPoint& ll, const meshTriPoint& ul, const meshTriPoint& ur, const char* name ) const;

    // CGAL debugging - save triangulation that can be read byt cgal_tri_test app - for reporting issues upstream
    void writeCdtFile(  const char* filename, std::vector<meshTriPoint>& points,  std::vector<meshTriSegment>& constraints ) const;
    void writeCdtFile2( const char* filename, const meshTriCDT& cdt) const;

private:
    // data
    tgMesh*                                         mesh;
    meshTriCDT                                      meshTriangulation;    

    std::vector<meshVertexInfo>                     vertexInfo;
    std::vector<meshFaceInfo>                       faceInfo;

    CGAL::Unique_hash_map<meshTriVertexHandle, int> vertexHandleToIndexMap;
    CGAL::Unique_hash_map<meshTriFaceHandle, int>   faceHandleToIndexMap;

    std::map<int, meshTriVertexHandle>              vertexIndexToHandleMap;
    std::map<int, meshTriFaceHandle>                faceIndexToHandleMap;
};

#endif /* __TG_MESH_TRIANGULATION_HXX__ */