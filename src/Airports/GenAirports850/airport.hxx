#ifndef _AIRPORT_H_
#define _AIRPORT_H_

#include <stdio.h>
#include <stdlib.h>

#include "runway.hxx"
#include "object.hxx"
#include "helipad.hxx"
#include "closedpoly.hxx"
#include "linearfeature.hxx"
#include "linked_objects.hxx"

using std::string;

class Airport
{
public:
    Airport( int c, char* def);

    void AddRunway( Runway* runway )
    {
        runways.push_back( runway );
    }

    void AddWaterRunway( WaterRunway* waterrunway )
    {
        waterrunways.push_back( waterrunway );
    }

    void AddObj( LightingObj* lightobj )
    {
        lightobjects.push_back( lightobj );
    }

    void AddHelipad( Helipad* helipad )
    {
        helipads.push_back( helipad );
    }

    void AddPavement( ClosedPoly* pavement )
    {
        pavements.push_back( pavement );
    }

    void AddFeature( LinearFeature* feature )
    {
        SG_LOG(SG_GENERAL, SG_ALERT, "Adding Feature");
        features.push_back( feature );
    }

    void AddFeatures( FeatureList* feature_list )
    {
        for (int i=0; i<feature_list->size(); i++)
        {
            features.push_back( feature_list->at(i) );
        }
    }

    void AddWindsock( Windsock* windsock )
    {
        windsocks.push_back( windsock );
    }

    void AddBeacon( Beacon* beacon )
    {
        beacons.push_back( beacon );
    }

    void AddSign( Sign* sign )
    {
        signs.push_back( sign );
    }

    void BuildOsg( osg::Group* airport );
    void BuildBtg( const string& root, const string_list& elev_src );

private:
    int     code;               // airport, heliport or sea port
    int     altitude;           // in meters
    string  icao;               // airport code
    string  description;        // description

    PavementList    pavements;
    FeatureList     features;
    RunwayList      runways;
    WaterRunwayList waterrunways;
    LightingObjList lightobjects;
    WindsockList    windsocks;
    BeaconList      beacons;
    SignList        signs;
    HelipadList     helipads;
};

typedef std::vector <Airport *> AirportList;

#endif
