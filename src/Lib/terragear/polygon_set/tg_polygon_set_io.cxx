#include <simgear/debug/logstream.hxx>

#include "tg_polygon_set.hxx"

#include <simgear/misc/sg_path.hxx> // for file i/o

// we are loading polygonal data from untrusted sources
// high probability this will crash CGAL if we just load 
// the points.  previous terragear would attempt to clean
// the input using duplicate point detection, degenerate
// edge detection, etc.
// 
// Instead, we'll generate an arrangement from each ring
// the first ring is considered the boundary, and all of 
// the rest are considered holes.
// this should never fail.                   _
//           _______________________________|_|
//          /                            ___|
//         /    _______                 / 
//        /    |       |               /
//        |    |     __|_             /
//        |    |____|__| |     ______/____
//        |         |____|     |____/     |
//        |                   /|          |
//        \                  / |__________|
//         \________________/
//
// NOTE: using this method, the above shapefile will result in a
// single polygon with holes.
// 1) the outer boundary is not simple - it self intersects in the 
//    top right corner.  We generate the outer boundary as the 
//    union of all faces generated by the first ring
// 2) the three remaining rings are unioned together as holes
//    a boolean difference is performed to make them holes.
//
// NOTE: the first two self intersecting holes become a single hole.
//       the third ring decreases the boundary of the polygon
//
// the final result is two polygons_with_holes.
// the frst is a poly with a single hole.
// the second is the degenerate piece in the top right.
tgPolygonSet::tgPolygonSet( OGRPolygon* poGeometry, const tgPolygonSetMeta& metaInfo ) : meta(metaInfo)
{
    std::vector<cgalPoly_Polygon>	boundaries;
    std::vector<cgalPoly_Polygon>	holes;
    cgalPoly_PolygonSet             holesUnion;
    std::vector<cgalPoly_Point>     nodes;

    // create PolygonSet from the outer ring
    OGRLinearRing const *ring = poGeometry->getExteriorRing();
    nodes.clear();
    for (int i = 0; i < ring->getNumPoints(); i++) {
        nodes.push_back( cgalPoly_Point(ring->getX(i), ring->getY(i)) );
    }
    facesFromUntrustedNodes( nodes, boundaries );

    // then a PolygonSet from each interior ring
    for ( int i = 0 ; i < poGeometry->getNumInteriorRings(); i++ ) {
        ring = poGeometry->getInteriorRing( i );
        nodes.clear();
        for (int j = 0; j < ring->getNumPoints(); j++) {
            nodes.push_back( cgalPoly_Point(ring->getX(j), ring->getY(j)) );
        }
        facesFromUntrustedNodes( nodes, holes );
    }

    // join all the boundaries
    ps.join( boundaries.begin(), boundaries.end() );

    // join all the holes
    holesUnion.join( holes.begin(), holes.end() );

    // perform difference
    ps.difference( holesUnion );
}

tgPolygonSet::tgPolygonSet( OGRFeature* poFeature, OGRPolygon* poGeometry )
{
    std::vector<cgalPoly_Polygon>	boundaries;
    std::vector<cgalPoly_Polygon>	holes;
    cgalPoly_PolygonSet             holesUnion;
    std::vector<cgalPoly_Point>     nodes;

    // generate texture info from feature
    meta.getFeatureFields( poFeature );
    
    // create PolygonSet from the outer ring
    OGRLinearRing const *ring = poGeometry->getExteriorRing();
    nodes.clear();
    for (int i = 0; i < ring->getNumPoints(); i++) {
        nodes.push_back( cgalPoly_Point(ring->getX(i), ring->getY(i)) );
    }
    facesFromUntrustedNodes( nodes, boundaries );

    // then a PolygonSet from each interior ring
    for ( int i = 0 ; i < poGeometry->getNumInteriorRings(); i++ ) {
        ring = poGeometry->getInteriorRing( i );
        nodes.clear();
        for (int j = 0; j < ring->getNumPoints(); j++) {
            nodes.push_back( cgalPoly_Point(ring->getX(j), ring->getY(j)) );
        }
        facesFromUntrustedNodes( nodes, holes );
    }

    // join all the boundaries
    ps.join( boundaries.begin(), boundaries.end() );

    // join all the holes
    holesUnion.join( holes.begin(), holes.end() );

    // perform difference
    ps.difference( holesUnion );
}

GDALDataset* tgPolygonSet::openDatasource( const char* datasource_name )
{
    GDALDataset*    poDS = NULL;
    GDALDriver*     poDriver = NULL;
    const char*     format_name = "ESRI Shapefile";
    
    SG_LOG( SG_GENERAL, SG_DEBUG, "Open Datasource: " << datasource_name );
    
    GDALAllRegister();
    
    poDriver = GetGDALDriverManager()->GetDriverByName( format_name );
    if ( poDriver ) {    
        poDS = poDriver->Create( datasource_name, 0, 0, 0, GDT_Unknown, NULL );
    }
    
    return poDS;
}

OGRLayer* tgPolygonSet::openLayer( GDALDataset* poDS, OGRwkbGeometryType lt, const char* layer_name )
{
#if 1
    OGRLayer*           poLayer = NULL;
 
    if ( !strlen( layer_name )) {
        SG_LOG(SG_GENERAL, SG_ALERT, "tgPolygonSet::toShapefile: layer name is NULL" );
        exit(0);
    }
    
    poLayer = poDS->GetLayerByName( layer_name );    
    if ( !poLayer ) {
        SG_LOG(SG_GENERAL, SG_DEBUG, "tgPolygonSet::toShapefile: layer " << layer_name << " doesn't exist - create" );

        OGRSpatialReference srs;
        srs.SetWellKnownGeogCS("WGS84");
        
        poLayer = poDS->CreateLayer( layer_name, &srs, lt, NULL );

        OGRFieldDefn descriptionField( "tg_desc", OFTString );
        descriptionField.SetWidth( 128 );
        if( poLayer->CreateField( &descriptionField ) != OGRERR_NONE ) {
            SG_LOG( SG_GENERAL, SG_ALERT, "Creation of field 'tg_desc' failed" );
        }
        
        OGRFieldDefn idField( "tg_id", OFTInteger );
        if( poLayer->CreateField( &idField ) != OGRERR_NONE ) {
            SG_LOG( SG_GENERAL, SG_ALERT, "Creation of field 'tg_id' failed" );
        }
        
        OGRFieldDefn fidField( "OGC_FID", OFTInteger );
        if( poLayer->CreateField( &fidField ) != OGRERR_NONE ) {
            SG_LOG( SG_GENERAL, SG_ALERT, "Creation of field 'OGC_FID' failed" );
        }

        OGRFieldDefn flagsField( "tg_flags", OFTInteger );
        if( poLayer->CreateField( &flagsField ) != OGRERR_NONE ) {
            SG_LOG( SG_GENERAL, SG_ALERT, "Creation of field 'flags' failed" );
        }
        
        OGRFieldDefn materialField( "tg_mat", OFTString );
        materialField.SetWidth( 32 );
        if( poLayer->CreateField( &materialField ) != OGRERR_NONE ) {
            SG_LOG( SG_GENERAL, SG_ALERT, "Creation of field 'tg_material' failed" );
        }
        
        OGRFieldDefn texMethodField( "tg_texmeth", OFTInteger );
        if( poLayer->CreateField( &texMethodField ) != OGRERR_NONE ) {
            SG_LOG( SG_GENERAL, SG_ALERT, "Creation of field 'tg_tex_method' failed" );
        }
        
        OGRFieldDefn texRefLonField( "tg_reflon", OFTReal );
        texRefLonField.SetWidth( 24 );
        texRefLonField.SetPrecision( 3 );        
        if( poLayer->CreateField( &texRefLonField ) != OGRERR_NONE ) {
            SG_LOG( SG_GENERAL, SG_ALERT, "Creation of field 'tg_tp_ref_lon' failed" );
        }
        
        OGRFieldDefn texRefLatField( "tg_reflat", OFTReal );
        if( poLayer->CreateField( &texRefLatField ) != OGRERR_NONE ) {
            SG_LOG( SG_GENERAL, SG_ALERT, "Creation of field 'tg_tp_ref_lat' failed" );
        }
        
        OGRFieldDefn texHeadingField( "tg_heading", OFTReal );
        if( poLayer->CreateField( &texHeadingField ) != OGRERR_NONE ) {
            SG_LOG( SG_GENERAL, SG_ALERT, "Creation of field 'tg_tp_heading' failed" );
        }
        
        OGRFieldDefn texWidthField( "tg_width", OFTReal );
        if( poLayer->CreateField( &texWidthField ) != OGRERR_NONE ) {
            SG_LOG( SG_GENERAL, SG_ALERT, "Creation of field 'tg_tp_width' failed" );
        }
        
        OGRFieldDefn texLengthField( "tg_length", OFTReal );
        if( poLayer->CreateField( &texLengthField ) != OGRERR_NONE ) {
            SG_LOG( SG_GENERAL, SG_ALERT, "Creation of field 'tg_tp_length' failed" );
        }
        
        OGRFieldDefn texMinUField( "tg_minu", OFTReal );
        if( poLayer->CreateField( &texMinUField ) != OGRERR_NONE ) {
            SG_LOG( SG_GENERAL, SG_ALERT, "Creation of field 'tg_tp_minu' failed" );
        }
        
        OGRFieldDefn texMinVField( "tg_minv", OFTReal );
        if( poLayer->CreateField( &texMinVField ) != OGRERR_NONE ) {
            SG_LOG( SG_GENERAL, SG_ALERT, "Creation of field 'tg_tp_minv' failed" );
        }
        
        OGRFieldDefn texMaxUField( "tg_maxu", OFTReal );
        if( poLayer->CreateField( &texMaxUField ) != OGRERR_NONE ) {
            SG_LOG( SG_GENERAL, SG_ALERT, "Creation of field 'tg_tp_maxu' failed" );
        }
        
        OGRFieldDefn texMaxVField( "tg_maxv", OFTReal );
        if( poLayer->CreateField( &texMaxVField ) != OGRERR_NONE ) {
            SG_LOG( SG_GENERAL, SG_ALERT, "Creation of field 'tg_tp_maxv' failed" );
        }
        
        OGRFieldDefn texMinClipUField( "tg_mincu", OFTReal );
        if( poLayer->CreateField( &texMinClipUField ) != OGRERR_NONE ) {
            SG_LOG( SG_GENERAL, SG_ALERT, "Creation of field 'tg_tp_min_clipu' failed" );
        }
        
        OGRFieldDefn texMinClipVField( "tg_mincv", OFTReal );
        texMinClipVField.SetWidth( 24 );
        texMinClipVField.SetPrecision( 3 );        
        if( poLayer->CreateField( &texMinClipVField ) != OGRERR_NONE ) {
            SG_LOG( SG_GENERAL, SG_ALERT, "Creation of field 'tg_tp_min_clipv' failed" );
        }
        
        OGRFieldDefn texMaxClipUField( "tg_maxcu", OFTReal );
        if( poLayer->CreateField( &texMaxClipUField ) != OGRERR_NONE ) {
            SG_LOG( SG_GENERAL, SG_ALERT, "Creation of field 'tg_tp_max_clipu' failed" );
        }
        
        OGRFieldDefn texMaxClipVField( "tg_maxcv", OFTReal );
        if( poLayer->CreateField( &texMaxClipVField ) != OGRERR_NONE ) {
            SG_LOG( SG_GENERAL, SG_ALERT, "Creation of field 'tg_tp_max_clipv' failed" );
        }
        
        OGRFieldDefn texCenterLatField( "tg_clat", OFTReal );
        if( poLayer->CreateField( &texCenterLatField ) != OGRERR_NONE ) {
            SG_LOG( SG_GENERAL, SG_ALERT, "Creation of field 'tg_tp_center_lat' failed" );
        }


        // surface metadata
        OGRFieldDefn texSurfaceMinLonField( "tgsrf_mnln", OFTReal );
        texSurfaceMinLonField.SetWidth( 24 );
        texSurfaceMinLonField.SetPrecision( 3 );        
        if( poLayer->CreateField( &texSurfaceMinLonField ) != OGRERR_NONE ) {
            SG_LOG( SG_GENERAL, SG_ALERT, "Creation of field 'tgsrf_mnln' failed" );
        }

        OGRFieldDefn texSurfaceMinLatField( "tgsrf_mnlt", OFTReal );
        texSurfaceMinLatField.SetWidth( 24 );
        texSurfaceMinLatField.SetPrecision( 3 );        
        if( poLayer->CreateField( &texSurfaceMinLatField ) != OGRERR_NONE ) {
            SG_LOG( SG_GENERAL, SG_ALERT, "Creation of field 'tgsrf_mnlt' failed" );
        }

        OGRFieldDefn texSurfaceMaxLonField( "tgsrf_mxln", OFTReal );
        texSurfaceMaxLonField.SetWidth( 24 );
        texSurfaceMaxLonField.SetPrecision( 3 );        
        if( poLayer->CreateField( &texSurfaceMaxLonField ) != OGRERR_NONE ) {
            SG_LOG( SG_GENERAL, SG_ALERT, "Creation of field 'tgsrf_mxln' failed" );
        }

        OGRFieldDefn texSurfaceMaxLatField( "tgsrf_mxlt", OFTReal );
        texSurfaceMaxLatField.SetWidth( 24 );
        texSurfaceMaxLatField.SetPrecision( 3 );        
        if( poLayer->CreateField( &texSurfaceMaxLatField ) != OGRERR_NONE ) {
            SG_LOG( SG_GENERAL, SG_ALERT, "Creation of field 'tgsrf_mxlt' failed" );
        }
        
        OGRFieldDefn texSurfaceCoef00( "tgsrf_co00", OFTReal );
        texSurfaceCoef00.SetWidth( 24 );
        texSurfaceCoef00.SetPrecision( 3 );        
        if( poLayer->CreateField( &texSurfaceCoef00 ) != OGRERR_NONE ) {
            SG_LOG( SG_GENERAL, SG_ALERT, "Creation of field 'tgsrf_co00' failed" );
        }

        OGRFieldDefn texSurfaceCoef01( "tgsrf_co01", OFTReal );
        texSurfaceCoef01.SetWidth( 24 );
        texSurfaceCoef01.SetPrecision( 3 );        
        if( poLayer->CreateField( &texSurfaceCoef01 ) != OGRERR_NONE ) {
            SG_LOG( SG_GENERAL, SG_ALERT, "Creation of field 'tgsrf_co01' failed" );
        }

        OGRFieldDefn texSurfaceCoef02( "tgsrf_co02", OFTReal );
        texSurfaceCoef02.SetWidth( 24 );
        texSurfaceCoef02.SetPrecision( 3 );        
        if( poLayer->CreateField( &texSurfaceCoef02 ) != OGRERR_NONE ) {
            SG_LOG( SG_GENERAL, SG_ALERT, "Creation of field 'tgsrf_co02' failed" );
        }

        OGRFieldDefn texSurfaceCoef03( "tgsrf_co03", OFTReal );
        texSurfaceCoef03.SetWidth( 24 );
        texSurfaceCoef03.SetPrecision( 3 );        
        if( poLayer->CreateField( &texSurfaceCoef03 ) != OGRERR_NONE ) {
            SG_LOG( SG_GENERAL, SG_ALERT, "Creation of field 'tgsrf_co03' failed" );
        }

        OGRFieldDefn texSurfaceCoef04( "tgsrf_co04", OFTReal );
        texSurfaceCoef04.SetWidth( 24 );
        texSurfaceCoef04.SetPrecision( 3 );        
        if( poLayer->CreateField( &texSurfaceCoef04 ) != OGRERR_NONE ) {
            SG_LOG( SG_GENERAL, SG_ALERT, "Creation of field 'tgsrf_co04' failed" );
        }

        OGRFieldDefn texSurfaceCoef05( "tgsrf_co05", OFTReal );        
        texSurfaceCoef05.SetWidth( 24 );
        texSurfaceCoef05.SetPrecision( 3 );        
        if( poLayer->CreateField( &texSurfaceCoef05 ) != OGRERR_NONE ) {
            SG_LOG( SG_GENERAL, SG_ALERT, "Creation of field 'tgsrf_co05' failed" );
        }

        OGRFieldDefn texSurfaceCoef06( "tgsrf_co06", OFTReal );
        texSurfaceCoef06.SetWidth( 24 );
        texSurfaceCoef06.SetPrecision( 3 );        
        if( poLayer->CreateField( &texSurfaceCoef06 ) != OGRERR_NONE ) {
            SG_LOG( SG_GENERAL, SG_ALERT, "Creation of field 'tgsrf_co06' failed" );
        }
        
        OGRFieldDefn texSurfaceCoef07( "tgsrf_co07", OFTReal );
        texSurfaceCoef07.SetWidth( 24 );
        texSurfaceCoef07.SetPrecision( 3 );        
        if( poLayer->CreateField( &texSurfaceCoef07 ) != OGRERR_NONE ) {
            SG_LOG( SG_GENERAL, SG_ALERT, "Creation of field 'tgsrf_co07' failed" );
        }

        OGRFieldDefn texSurfaceCoef08( "tgsrf_co08", OFTReal );
        texSurfaceCoef08.SetWidth( 24 );
        texSurfaceCoef08.SetPrecision( 3 );        
        if( poLayer->CreateField( &texSurfaceCoef08 ) != OGRERR_NONE ) {
            SG_LOG( SG_GENERAL, SG_ALERT, "Creation of field 'tgsrf_co08' failed" );
        }
        
        OGRFieldDefn texSurfaceCoef09( "tgsrf_co09", OFTReal );
        texSurfaceCoef09.SetWidth( 24 );
        texSurfaceCoef09.SetPrecision( 3 );        
        if( poLayer->CreateField( &texSurfaceCoef09 ) != OGRERR_NONE ) {
            SG_LOG( SG_GENERAL, SG_ALERT, "Creation of field 'tgsrf_co09' failed" );
        }

        OGRFieldDefn texSurfaceCoef10( "tgsrf_co10", OFTReal );
        texSurfaceCoef10.SetWidth( 24 );
        texSurfaceCoef10.SetPrecision( 3 );        
        if( poLayer->CreateField( &texSurfaceCoef10 ) != OGRERR_NONE ) {
            SG_LOG( SG_GENERAL, SG_ALERT, "Creation of field 'tgsrf_co10' failed" );
        }

        OGRFieldDefn texSurfaceCoef11( "tgsrf_co11", OFTReal );
        texSurfaceCoef11.SetWidth( 24 );
        texSurfaceCoef11.SetPrecision( 3 );        
        if( poLayer->CreateField( &texSurfaceCoef11 ) != OGRERR_NONE ) {
            SG_LOG( SG_GENERAL, SG_ALERT, "Creation of field 'tgsrf_co11' failed" );
        }
    } else {
        SG_LOG(SG_GENERAL, SG_DEBUG, "tgPolygonSet::toShapefile: layer " << layer_name << " already exists - open" );        
    }
   
    return poLayer;
#else
    return NULL;
#endif
}

void tgPolygonSet::toShapefile( OGRLayer* layer, const char* description ) const
{
    
}

void tgPolygonSet::toShapefile( OGRLayer* poLayer ) const
{
    toShapefile( poLayer, ps );
}

void tgPolygonSet::toShapefile( OGRLayer* poLayer, const cgalPoly_PolygonWithHoles& pwh ) const
{
    OGRPolygon    polygon;
    OGRPoint      point;
    OGRLinearRing ring;
    
    // in CGAL, the outer boundary is counter clockwise - in GDAL, it's expected to be clockwise
    cgalPoly_Polygon poly;
    cgalPoly_Polygon::Vertex_iterator it;

    poly = pwh.outer_boundary();
    //poly.reverse_orientation();
    for ( it = poly.vertices_begin(); it != poly.vertices_end(); it++ ) {
        point.setX( CGAL::to_double( (*it).x() ) );
        point.setY( CGAL::to_double( (*it).y() ) );
        point.setZ( 0.0 );
                
        ring.addPoint(&point);
    }
    ring.closeRings();
    polygon.addRing(&ring);

    // then write each hole
    cgalPoly_PolygonWithHoles::Hole_const_iterator hit;
    for (hit = pwh.holes_begin(); hit != pwh.holes_end(); ++hit) {
        OGRLinearRing hole;
        poly = (*hit);

        //poly.reverse_orientation();
        for ( it = poly.vertices_begin(); it != poly.vertices_end(); it++ ) {
            point.setX( CGAL::to_double( (*it).x() ) );
            point.setY( CGAL::to_double( (*it).y() ) );
            point.setZ( 0.0 );
                    
            hole.addPoint(&point);            
        }
        hole.closeRings();
        polygon.addRing(&hole);
    }

    OGRFeature* poFeature = OGRFeature::CreateFeature( poLayer->GetLayerDefn() );
    poFeature->SetGeometry(&polygon);
    meta.setFeatureFields( poFeature );
    
    if( poLayer->CreateFeature( poFeature ) != OGRERR_NONE )
    {
        SG_LOG(SG_GENERAL, SG_ALERT, "Failed to create feature in shapefile");
    }
    OGRFeature::DestroyFeature(poFeature);    
}

void tgPolygonSet::toShapefile( OGRLayer* poLayer, const cgalPoly_Polygon& poly ) const
{
    OGRPolygon    polygon;
    OGRPoint      point;
    OGRLinearRing ring;
    
    // in CGAL, the outer boundary is counter clockwise - in GDAL, it's expected to be clockwise
    cgalPoly_Polygon::Vertex_iterator it;
    
    for ( it = poly.vertices_begin(); it != poly.vertices_end(); it++ ) {
        point.setX( CGAL::to_double( (*it).x() ) );
        point.setY( CGAL::to_double( (*it).y() ) );
        point.setZ( 0.0 );
        
        ring.addPoint(&point);
    }
    ring.closeRings();
    polygon.addRing(&ring);
    
    OGRFeature* poFeature = OGRFeature::CreateFeature( poLayer->GetLayerDefn() );
    poFeature->SetGeometry(&polygon);    
    meta.setFeatureFields( poFeature );
    
    if( poLayer->CreateFeature( poFeature ) != OGRERR_NONE )
    {
        SG_LOG(SG_GENERAL, SG_ALERT, "Failed to create feature in shapefile");
    }
    OGRFeature::DestroyFeature(poFeature);    
}

void tgPolygonSet::toShapefile( OGRLayer* poLayer, const cgalPoly_Arrangement& arr ) const
{    
    cgalPoly_EdgeConstIterator eit;
    
    for ( eit = arr.edges_begin(); eit != arr.edges_end(); ++eit ) {        
        cgalPoly_Segment seg = eit->curve();

        OGRLinearRing ring;
        OGRPoint      point;

        point.setX( CGAL::to_double( seg.source().x() ) );
        point.setY( CGAL::to_double( seg.source().y() ) );
        point.setZ( 0 );
        ring.addPoint(&point);

        point.setX( CGAL::to_double( seg.target().x() ) );
        point.setY( CGAL::to_double( seg.target().y() ) );
        point.setZ( 0 );
        ring.addPoint(&point);

        OGRFeature* poFeature = OGRFeature::CreateFeature( poLayer->GetLayerDefn() );
        poFeature->SetGeometry(&ring);    
    
        if( poLayer->CreateFeature( poFeature ) != OGRERR_NONE )
        {
            SG_LOG(SG_GENERAL, SG_ALERT, "Failed to create feature in shapefile");
        }
        OGRFeature::DestroyFeature(poFeature);    
    }    
}

void tgPolygonSet::toShapefile( const char* datasource, const char* layer ) const
{
    // Open datasource and layer
    GDALDataset* poDS = openDatasource( datasource );

    if ( poDS ) {
        OGRLayer* poLayer = openLayer( poDS, wkbPolygon25D, layer );

        if ( poLayer ) {
            toShapefile( poLayer, ps );
        }
    }
    
    // close datasource
    GDALClose( poDS );
}

void tgPolygonSet::toShapefile( OGRLayer* poLayer, const cgalPoly_PolygonSet& polySet ) const
{
    std::list<cgalPoly_PolygonWithHoles>                 pwh_list;
    std::list<cgalPoly_PolygonWithHoles>::const_iterator it;

    polySet.polygons_with_holes( std::back_inserter(pwh_list) );
    SG_LOG(SG_GENERAL, SG_DEBUG, "tgPolygonSet::toShapefile: got " << pwh_list.size() << " polys with holes ");
    
    // save each poly with holes to the layer
    for (it = pwh_list.begin(); it != pwh_list.end(); ++it) {
        cgalPoly_PolygonWithHoles pwh = (*it);

        toShapefile( poLayer, pwh );
    }
}


// static functions for arbitrary polygons and polygons with holes
void tgPolygonSet::toShapefile( const cgalPoly_Polygon& poly, const char* datasource, const char* layer )
{    
    GDALDataset*  poDS = NULL;
    OGRLayer*     poLayer = NULL;
    OGRPolygon    polygon;
    OGRPoint      point;
    OGRLinearRing ring;

    poDS = openDatasource( datasource );

    if ( poDS ) {
        poLayer = openLayer( poDS, wkbPolygon25D, layer );
        
        if ( poLayer ) {
            // in CGAL, the outer boundary is counter clockwise - in GDAL, it's expected to be clockwise
            cgalPoly_Polygon::Vertex_iterator it;
            
            for ( it = poly.vertices_begin(); it != poly.vertices_end(); it++ ) {
                point.setX( CGAL::to_double( (*it).x() ) );
                point.setY( CGAL::to_double( (*it).y() ) );
                point.setZ( 0.0 );
                
                ring.addPoint(&point);
            }
            ring.closeRings();
            polygon.addRing(&ring);
            
            OGRFeature* poFeature = OGRFeature::CreateFeature( poLayer->GetLayerDefn() );
            poFeature->SetGeometry(&polygon);    
            
            if( poLayer->CreateFeature( poFeature ) != OGRERR_NONE )
            {
                SG_LOG(SG_GENERAL, SG_ALERT, "Failed to create feature in shapefile");
            }
            OGRFeature::DestroyFeature(poFeature);    
        }
    }
    
    // close datasource
    GDALClose( poDS );
}

#if 0 // native from GDAL    
tgPolygonSet tgPolygonSet::fromGDAL( OGRPolygon* poGeometry )
{
    cgalPoly_Arrangement  arr;
    cgalPoly_PolygonSet   boundaries;
    cgalPoly_PolygonSet   holes;
    
    // for each boundary contour, we add it to its own arrangement
    // then read the resulting faces as a list of polygons with holes
    // note that a self intersecting face ( like a donut ) will generate
    // more than one face.  We need to determine for each face wether it is a 
    // hole or not.
    // ___________
    // | ______  |
    // | \    /  |
    // |  \  /   |
    // |___\/____|
    //
    // Example of a single self intersecting contour that should be represented by a polygon 
    for (unsigned int i=0; i<subject.Contours(); i++ ) {
        char layer[128];

        //sprintf( layer, "%04u_original_contour_%d", subject.GetId(), i );
        //tgShapefile::FromContour( subject.GetContour(i), false, true, "./clip_dbg", layer, "cont" );
    
        arr.Clear();
        arr.Add( subject.GetContour(i), layer );
    
        // retreive the new Contour(s) from traversing the outermost face first
        // any holes in this face are individual polygons
        // any holes in those faces are holes, etc...
        
        // dump the arrangement to see what we have.
        //sprintf( layer, "%04u_Arrangement_contour_%d", subject.GetId(), i );
        //arr.ToShapefiles( "./clip_dbg", layer );

        // Combine boundaries and holes into their sets
        Polygon_set face = arr.ToPolygonSet( i );
        //sprintf( layer, "%04u_face_contour_%d", subject.GetId(), i );
        //ToShapefile( face, layer );
        
        if ( subject.GetContour(i).GetHole() ) {
            //SG_LOG(SG_GENERAL, SG_ALERT, "ToCgalPolyWithHoles : Join with holes"  );
            
            //SG_LOG(SG_GENERAL, SG_ALERT, "ToCgalPolyWithHoles : before - face_valid " << face.is_valid() << " holes_valid " << holes.is_valid()  );
            holes.join( face );
            //SG_LOG(SG_GENERAL, SG_ALERT, "ToCgalPolyWithHoles : after - face_valid " << face.is_valid() << " holes_valid " << holes.is_valid()  );
        } else {
            //SG_LOG(SG_GENERAL, SG_ALERT, "ToCgalPolyWithHoles : Join with boundaries"  );
            
            //SG_LOG(SG_GENERAL, SG_ALERT, "ToCgalPolyWithHoles : before - face_valid " << face.is_valid() << " boundaries_valid " << boundaries.is_valid()  );
            boundaries.join( face );            
            //SG_LOG(SG_GENERAL, SG_ALERT, "ToCgalPolyWithHoles : after - face_valid " << face.is_valid() << " boundaries_valid " << boundaries.is_valid()  );
        }
        //SG_LOG(SG_GENERAL, SG_ALERT, "ToCgalPolyWithHoles : Join complete"  );
    }

    // now, generate the result
    boundaries.difference( holes );
    
    // dump to shapefile
    if ( boundaries.is_valid() ) {
        return tgPolygonSet( boundaries );
    } else {
        return tgPolygonSet();
    }
}
#endif


// this needs some more thought - static constructor?  do projection?
// texture info? flags?, identifier?

// We want feature and geometry - geometry should be projected already.
// constructor - return new object. - no longer a static function
    
typedef CGAL::Bbox_2    BBox;

#if 0
void tgAccumulator::Diff_cgal( tgPolygon& subject )
{   
    // static int savepoly = 0;
    // char filename[32];
    
    Polygon_set  cgalSubject;
    CGAL::Bbox_2 cgalBbox;
    
    Polygon_set diff = accum_cgal;

    if ( ToCgalPolyWithHoles( subject, cgalSubject, cgalBbox ) ) {
        if ( !accumEmpty ) {
            cgalSubject.difference( diff );
        }

        ToTgPolygon( cgalSubject, subject );
    }
}

void tgAccumulator::Add_cgal( const tgPolygon& subject )
{
    // just add the converted cgalPoly_PolygonWithHoles to the Polygon set
    Polygon_set  cgalSubject;
    CGAL::Bbox_2 cgalBbox;
    
    if ( ToCgalPolyWithHoles( subject, cgalSubject, cgalBbox ) ) {
        accum_cgal.join(cgalSubject);
        accumEmpty = false;
    }
}

// rewrite to use bounding boxes, and lists of polygons with holes 
// need a few functions:
// 1) generate a Polygon_set from the Polygons_with_holes in the list that intersect subject bounding box
// 2) Add to the Polygons_with_holes list with a Polygon set ( and the bounding boxes )
    
Polygon_set tgAccumulator::GetAccumPolygonSet( const CGAL::Bbox_2& bbox ) 
{
    std::list<tgAccumEntry>::const_iterator it;
    std::list<Polygon_with_holes> accum;
    Polygon_set ps;
    
    // traverse all of the Polygon_with_holes and accumulate their union
    for ( it=accum_cgal_list.begin(); it!=accum_cgal_list.end(); it++ ) {
        if ( CGAL::do_overlap( bbox, (*it).bbox ) ) {
            accum.push_back( (*it).pwh );
        }
    }
    
    ps.join( accum.begin(), accum.end() );
    
    return ps;
}

void tgAccumulator::AddAccumPolygonSet( const Polygon_set& ps )
{
    std::list<Polygon_with_holes> pwh_list;
    std::list<Polygon_with_holes>::const_iterator it;
    CGAL::Bbox_2 bbox;
    
    ps.polygons_with_holes( std::back_inserter(pwh_list) );
    for (it = pwh_list.begin(); it != pwh_list.end(); ++it) {
        tgAccumEntry entry;
        entry.pwh  = (*it);
        entry.bbox =  entry.pwh.outer_boundary().bbox();

        accum_cgal_list.push_back( entry );
    }    
}

#define DEBUG_DIFF_AND_ADD 1
void tgAccumulator::Diff_and_Add_cgal( tgPolygon& subject )
{
    Polygon_set     cgSubject;
    CGAL::Bbox_2    cgBoundingBox;

#if DEBUG_DIFF_AND_ADD    
    char            layer[128];
#endif
    
    if ( ToCgalPolyWithHoles( subject, cgSubject, cgBoundingBox ) ) {
        Polygon_set add  = cgSubject;
        Polygon_set diff = GetAccumPolygonSet( cgBoundingBox );

#if DEBUG_DIFF_AND_ADD    
        sprintf( layer, "clip_%03d_pre_subject", subject.GetId() );
        ToShapefile( add, layer );
        
        tgContour bb;
        bb.AddPoint( cgalPoly_Point( cgBoundingBox.xmin(), cgBoundingBox.ymin() ) );
        bb.AddPoint( cgalPoly_Point( cgBoundingBox.xmin(), cgBoundingBox.ymax() ) );
        bb.AddPoint( cgalPoly_Point( cgBoundingBox.xmax(), cgBoundingBox.ymax() ) );
        bb.AddPoint( cgalPoly_Point( cgBoundingBox.xmax(), cgBoundingBox.ymin() ) );
        
        sprintf( layer, "clip_%03d_bbox", subject.GetId() );
        tgShapefile::FromContour( bb, false, false, "./clip_dbg", layer, "bbox" );
        
        sprintf( layer, "clip_%03d_pre_accum", subject.GetId() );
        ToShapefile( diff, layer );
#endif

        if ( diff.number_of_polygons_with_holes() ) {
            cgSubject.difference( diff );
            
#if DEBUG_DIFF_AND_ADD    
            sprintf( layer, "clip_%03d_post_subject", subject.GetId() );
            ToShapefile( cgSubject, layer );            
#endif

        }

        // add the polygons_with_holes to the accumulator list
        AddAccumPolygonSet( add );
        
        // when we convert back to poly, insert face_location points for each face
        ToTgPolygon( cgSubject, subject );        
    } else {
        tgcontour_list contours;
        contours.clear();
        subject.SetContours( contours );
    }    
}
#endif