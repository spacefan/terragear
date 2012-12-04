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


#include <string>
#include <iostream>

#include <boost/thread.hpp>

#include <simgear/debug/logstream.hxx>
#include <simgear/misc/sg_path.hxx>
#include <simgear/misc/sgstream.hxx>

#include <Polygon/index.hxx>
#include <Geometry/util.hxx>
#include <Geometry/poly_support.hxx>
#include <Include/version.h>

#include "scheduler.hxx"
#include "beznode.hxx"
#include "closedpoly.hxx"
#include "linearfeature.hxx"
#include "parser.hxx"
#include "scheduler.hxx"

using namespace std;

// Display usage
static void usage( int argc, char **argv ) {
    GENAPT_LOG(SG_GENERAL, SG_ALERT, "Usage: " << argv[0] << "\n--input=<apt_file>"
    << "\n--work=<work_dir>\n[ --start-id=abcd ] [ --restart-id=abcd ] [ --nudge=n ] "
    << "[--min-lon=<deg>] [--max-lon=<deg>] [--min-lat=<deg>] [--max-lat=<deg>] "
    << "[ --airport=abcd ] [--max-slope=<decimal>] [--tile=<tile>] [--threads] [--threads=x]"
    << "[--chunk=<chunk>] [--clear-dem-path] [--dem-path=<path>] [--verbose] [--help]");
}


void setup_default_elevation_sources(string_list& elev_src) {
    elev_src.push_back( "SRTM2-Africa-3" );
    elev_src.push_back( "SRTM2-Australia-3" );
    elev_src.push_back( "SRTM2-Eurasia-3" );
    elev_src.push_back( "SRTM2-Islands-3" );
    elev_src.push_back( "SRTM2-North_America-3" );
    elev_src.push_back( "SRTM2-South_America-3" );
    elev_src.push_back( "DEM-USGS-3" );
    elev_src.push_back( "SRTM-1" );
    elev_src.push_back( "SRTM-3" );
    elev_src.push_back( "SRTM-30" );
}

// Display help and usage
static void help( int argc, char **argv, const string_list& elev_src ) {
    cout << "genapts generates airports for use in generating scenery for the FlightGear flight simulator.  \n";
    cout << "Airport, runway, and taxiway vector data and attributes are input, and generated 3D airports \n";
    cout << "are output for further processing by the TerraGear scenery creation tools.  \n";
    cout << "\n\n";
    cout << "The standard input file is apt.dat.gz which is found in $FG_ROOT/Airports.  \n";
    cout << "This file is periodically generated by Robin Peel, who maintains  \n";
    cout << "the airport database for both the X-Plane and FlightGear simulators.  \n";
    cout << "The format of this file is documented at  \n";
    cout << "http://data.x-plane.com/designers.html#Formats   \n";
    cout << "Any other input file corresponding to this format may be used as input to genapts.  \n";
    cout << "Input files may be gzipped or left as plain text as required.  \n";
    cout << "\n\n";
    cout << "Processing all the world's airports takes a *long* time.  To cut down processing time \n";
    cout << "when only some airports are required, you may refine the input selection either by airport \n";
    cout << "or by area.  By airport, either one airport can be specified using --airport=abcd, where abcd is \n";
    cout << "a valid airport code eg. --airport-id=KORD, or a starting airport can be specified using --start-id=abcd \n";
    cout << "where once again abcd is a valid airport code.  In this case, all airports in the file subsequent to the \n";
    cout << "start-id are done.  This is convienient when re-starting after a previous error.  \n";
    cout << "If you want to restart with the airport after a problam icao, use --restart-id=abcd, as this works the same as\n";
    cout << " with the exception that the airport abcd is skipped \n";
    cout << "\nAn input area may be specified by lat and lon extent using min and max lat and lon.  \n";
    cout << "Alternatively, you may specify a chunk (10 x 10 degrees) or tile (1 x 1 degree) using a string \n";
    cout << "such as eg. w080n40, e000s27.  \n";
    cout << "\nAn input file containing only a subset of the world's \n";
    cout << "airports may of course be used.\n";
    cout << "\n\n";
    cout << "It is necessary to generate the elevation data for the area of interest PRIOR TO GENERATING THE AIRPORTS.  \n";
    cout << "Failure to do this will result in airports being generated with an elevation of zero.  \n";
    cout << "The following subdirectories of the work-dir will be searched for elevation files:\n\n";
    
    string_list::const_iterator elev_src_it;
    for (elev_src_it = elev_src.begin(); elev_src_it != elev_src.end(); elev_src_it++) {
    	    cout << *elev_src_it << "\n";
    }
    cout << "\n";
    usage( argc, argv );
}

// TODO: where do these belong
int nudge = 10;
double slope_max = 0.02;
double gSnap = 0.00000001;      // approx 1 mm

//TODO : new polygon chop API
extern bool tgPolygon_index_init( const std::string& path );

int main(int argc, char **argv)
{
    SGGeod min = SGGeod::fromDeg( -180, -90 );
    SGGeod max = SGGeod::fromDeg( 180, 90 );
    long  position = 0;

    // Setup elevation directories
    string_list elev_src;
    elev_src.clear();
    setup_default_elevation_sources(elev_src);

    std::string debug_dir = ".";
    vector<std::string> debug_runway_defs;
    vector<std::string> debug_pavement_defs;
    vector<std::string> debug_taxiway_defs;
    vector<std::string> debug_feature_defs;

    // Set Normal logging
    sglog().setLogLevels( SG_GENERAL, SG_INFO );

    // parse arguments
    std::string work_dir = "";
    std::string input_file = "";
    std::string summary_file = "./genapt850.csv";
    std::string start_id = "";
    std::string restart_id = "";
    std::string airport_id = "";
    std::string last_apt_file = "./last_apt.txt";
    int         num_threads    =  1;

    int arg_pos;
    for (arg_pos = 1; arg_pos < argc; arg_pos++)
    {
        string arg = argv[arg_pos];
        if ( arg.find("--work=") == 0 )
        {
            work_dir = arg.substr(7);
        }
        else if ( arg.find("--input=") == 0 )
        {
            input_file = arg.substr(8);
        }
        else if ( arg.find("--start-id=") == 0 )
        {
            start_id = arg.substr(11);
        }
        else if ( arg.find("--restart-id=") == 0 )
        {
            restart_id = arg.substr(13);
        }
        else if ( arg.find("--nudge=") == 0 )
        {
            nudge = atoi( arg.substr(8).c_str() );
        }
        else if ( arg.find("--snap=") == 0 )
        {
            gSnap = atof( arg.substr(7).c_str() );
        }
        else if ( arg.find("--last_apt_file=") == 0 )
        {
            last_apt_file = arg.substr(16);
        }
        else if ( arg.find("--min-lon=") == 0 )
        {
            min.setLongitudeDeg(atof( arg.substr(10).c_str() ));
        }
        else if ( arg.find("--max-lon=") == 0 )
        {
            max.setLongitudeDeg(atof( arg.substr(10).c_str() ));
        }
        else if ( arg.find("--min-lat=") == 0 )
        {
            min.setLatitudeDeg(atof( arg.substr(10).c_str() ));
        }
        else if ( arg.find("--max-lat=") == 0 )
        {
            max.setLatitudeDeg(atof( arg.substr(10).c_str() ));
        }
        else if ( arg.find("--chunk=") == 0 )
        {
            tg::Rectangle rectangle = tg::parseChunk(arg.substr(8).c_str(), 10.0);
            min = rectangle.getMin();
            max = rectangle.getMax();
        }
        else if ( arg.find("--tile=") == 0 )
        {
            tg::Rectangle rectangle = tg::parseTile(arg.substr(7).c_str());
            min = rectangle.getMin();
            max = rectangle.getMax();
        }
        else if ( arg.find("--airport=") == 0 ) 
        {
    	    airport_id = arg.substr(10).c_str();
    	} 
        else if ( arg == "--clear-dem-path" ) 
        {
    	    elev_src.clear();
    	} 
        else if ( arg.find("--dem-path=") == 0 ) 
        {
    	    elev_src.push_back( arg.substr(11) );
    	} 
        else if ( (arg.find("--verbose") == 0) || (arg.find("-v") == 0) ) 
        {
    	    sglog().setLogLevels( SG_GENERAL, SG_BULK );
    	}
        else if ( (arg.find("--max-slope=") == 0) ) 
        {
    	    slope_max = atof( arg.substr(12).c_str() );
        }
        else if ( (arg.find("--threads=") == 0) )
        {
            num_threads = atoi( arg.substr(10).c_str() );
        }
        else if ( (arg.find("--threads") == 0) )
        {
            num_threads = boost::thread::hardware_concurrency();
        }
        else if (arg.find("--debug-dir=") == 0)
        {
            debug_dir = arg.substr(12);
        }
        else if (arg.find("--debug-runways=") == 0)
        {
            debug_runway_defs.push_back( arg.substr(16) );
        }
        else if (arg.find("--debug-pavements=") == 0)
        {
            debug_pavement_defs.push_back( arg.substr(18) );
        }
        else if (arg.find("--debug-taxiways=") == 0)
        {
            GENAPT_LOG(SG_GENERAL, SG_INFO, "add debug taxiway " << arg.substr(17) );
            debug_taxiway_defs.push_back( arg.substr(17) );
        }
        else if (arg.find("--debug-features=") == 0)
        {
            debug_feature_defs.push_back( arg.substr(17) );
        }
        else if ( (arg.find("--help") == 0) || (arg.find("-h") == 0) )
        {
    	    help( argc, argv, elev_src );
    	    exit(-1);
    	} 
        else 
        {
    	    usage( argc, argv );
    	    exit(-1);
    	}
    }

    std::string airportareadir=work_dir+"/AirportArea";

    // this is the main program -
    GENAPT_LOG(SG_GENERAL, SG_INFO, "Genapts850 version " << getTGVersion() << " running with " << num_threads << " threads" );
    GENAPT_LOG(SG_GENERAL, SG_INFO, "Launch command was " << argv[0] );
    GENAPT_LOG(SG_GENERAL, SG_INFO, "Input file = " << input_file);
    GENAPT_LOG(SG_GENERAL, SG_INFO, "Work directory = " << work_dir);
    GENAPT_LOG(SG_GENERAL, SG_INFO, "Longitude = " << min.getLongitudeDeg() << ':' << max.getLongitudeDeg());
    GENAPT_LOG(SG_GENERAL, SG_INFO, "Latitude = " << min.getLatitudeDeg() << ':' << max.getLatitudeDeg());
    GENAPT_LOG(SG_GENERAL, SG_INFO, "Terrain sources = ");
    for ( unsigned int i = 0; i < elev_src.size(); ++i ) {
        GENAPT_LOG(SG_GENERAL, SG_INFO, "  " << work_dir << "/" << elev_src[i] );
    }
    GENAPT_LOG(SG_GENERAL, SG_INFO, "Nudge = " << nudge);

    if (!max.isValid() || !min.isValid())
    {
        GENAPT_LOG(SG_GENERAL, SG_ALERT, "Bad longitude or latitude");
        exit(1);
    }

    // make work directory
    GENAPT_LOG(SG_GENERAL, SG_DEBUG, "Creating AirportArea directory");

    SGPath sgp( airportareadir );
    sgp.append( "dummy" );
    sgp.create_dir( 0755 );

    std::string lastaptfile = work_dir+"/last_apt";

    // initialize persistant polygon counter
    std::string counter_file = airportareadir+"/poly_counter";
    tgPolygon_index_init( counter_file );

    tg::Rectangle boundingBox(min, max);
    boundingBox.sanify();

    if ( work_dir == "" ) 
    {
    	GENAPT_LOG( SG_GENERAL, SG_ALERT, "Error: no work directory specified." );
    	usage( argc, argv );
	    exit(-1);
    }

    if ( input_file == "" ) 
    {
    	GENAPT_LOG( SG_GENERAL, SG_ALERT,  "Error: no input file." );
    	exit(-1);
    }

    // Initialize shapefile support (for debugging)
    tgShapefileInit();

    sg_gzifstream in( input_file );
    if ( !in.is_open() ) 
    {
        GENAPT_LOG( SG_GENERAL, SG_ALERT, "Cannot open file: " << input_file );
        exit(-1);
    }

    // Create the scheduler
    Scheduler* scheduler = new Scheduler(input_file, work_dir, elev_src);

    // Add any debug 
    scheduler->set_debug( debug_dir, debug_runway_defs, debug_pavement_defs, debug_taxiway_defs, debug_feature_defs );

    // just one airport 
    if ( airport_id != "" )
    {
        // just find and add the one airport
        scheduler->AddAirport( airport_id );

        GENAPT_LOG(SG_GENERAL, SG_INFO, "Finished Adding airport - now parse");
        
        // and schedule parsers
        scheduler->Schedule( num_threads, summary_file );
    }

    else if ( start_id != "" )
    {
        GENAPT_LOG(SG_GENERAL, SG_INFO, "move forward to " << start_id );

        // scroll forward in datafile
        position = scheduler->FindAirport( start_id );

        // add remaining airports within boundary
        if ( scheduler->AddAirports( position, &boundingBox ) )
        {
            // parse all the airports that were found
            scheduler->Schedule( num_threads, summary_file );
        }
    }
    else
    {
        // find all airports within given boundary
        if ( scheduler->AddAirports( 0, &boundingBox ) )
        {
            // and parse them
            scheduler->Schedule( num_threads, summary_file );
        }
    }

    GENAPT_LOG(SG_GENERAL, SG_INFO, "Done");
    exit(0);

    return 0;
}
