#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <simgear/compiler.h>
#include <simgear/math/point3d.hxx>
#include <simgear/misc/exception.hxx>
#include <simgear/misc/props_io.hxx>
#include <simgear/misc/props.hxx>

#include <Geometry/util.hxx>
#include <Polygon/index.hxx>
#include <Polygon/names.hxx>
#include <Polygon/polygon.hxx>
#include <Polygon/split.hxx>

#include <stdlib.h>

#include <vector>
SG_USING_STD(vector);

static string prog_name;
static string work_dir = ".";
static tg::Rectangle bounds(Point3D(-180, -90, 0), Point3D(180, 90, 0)); 
static FGPolygon bounds_poly;


/**
 * Parse two vertices from a string.
 */
static const char *
parse_point (const char * s, Point3D &p)
{
  char * endptr;
  float x = strtod(s, &endptr);
  if (endptr == s)
    return 0;
  else
    s = endptr;
  float y = strtod(s, &endptr);
  if (endptr == s) {
    SG_LOG(SG_TERRAIN, SG_WARN, "Uneven number of vertices!!");
    return 0;
  }
  p.setx(x);
  p.sety(y);
  return endptr;
}

static void
add_point (SGPropertyNode_ptr node)
{
  AreaType material =
    get_area_type(node->getStringValue("material", "Default"));
  Point3D p, dummy;
  const char * s = node->getStringValue("v");
  s = parse_point(s, p);
  if (s == 0) {
    SG_LOG(SG_TERRAIN, SG_WARN, "No point supplied; skipped");
    return;
  }
  s = parse_point(s, dummy);
  if (s != 0)
    SG_LOG(SG_TERRAIN, SG_WARN, "More than one vertex supplied for point");
  FGPolygon poly;
  tg::makePolygon(p, node->getIntValue("width", 500), poly);
  poly = polygon_int(poly, bounds_poly);
  split_polygon(".", material, poly);
}

static void
add_line (SGPropertyNode_ptr node)
{
  AreaType material =
    get_area_type(node->getStringValue("material", "Default"));
  const char * s = node->getStringValue("v");

  Point3D p;
  tg::Line line;
  s = parse_point(s, p);
  while (s != 0) {
    line.addPoint(p);
    s = parse_point(s, p);
  }

  FGPolygon poly;
  makePolygon(line, node->getIntValue("width", 10), poly);
  poly = polygon_int(poly, bounds_poly);
  split_polygon(".", material, poly);
}

static void
add_polygon (SGPropertyNode_ptr node)
{
  FGPolygon poly;
  AreaType material =
    get_area_type(node->getStringValue("material", "Default"));
  vector<SGPropertyNode_ptr> contour_nodes = node->getChildren("contour");
  for (int i = 0; i < contour_nodes.size(); i++) {
    SGPropertyNode_ptr contour_node = contour_nodes[i];
    Point3D p;
    const char * s = contour_node->getStringValue("v");
    s = parse_point(s, p);
    while (s != 0) {
      poly.add_node(i, p);
      s = parse_point(s, p);
    }
    poly.set_hole_flag(i, contour_node->getBoolValue("hole", false));
  }
  poly = polygon_int(poly, bounds_poly);
  split_polygon(".", material, poly);
}

void
usage ()
{
  SG_LOG(SG_TERRAIN, SG_ALERT, "Usage: " << prog_name << " [opts] <file>");
  exit(2);
}

int
main (int ac, char ** av)
{
  sglog().setLogLevels( SG_ALL, SG_DEBUG );

  poly_index_init( "../poly_counter" );

  prog_name = av[0];

  //
  // Process command-line options.
  //
  int argPos = 1;
  while (argPos < ac) {
    string arg = av[argPos];

    if (arg.find("--chunk=") == 0) {
      bounds = tg::parseChunk(arg.substr(8));
      argPos++;
    }

    else if (arg.find("--min-lon=") == 0) {
      bounds.getMin().setx(strtod(arg.substr(10).c_str(), 0));
      argPos++;
    }

    else if (arg.find("--min-lat=") == 0) {
      bounds.getMin().sety(strtod(arg.substr(10).c_str(), 0));
      argPos++;
    }

    else if (arg.find("--max-lon=") == 0) {
      bounds.getMax().setx(strtod(arg.substr(10).c_str(), 0));
      argPos++;
    } 

    else if (arg.find("--max-lat=") == 0) {
      bounds.getMax().sety(strtod(arg.substr(10).c_str(), 0));
      argPos++;
    }

    else if (arg.find("--work-dir=") == 0) {
      work_dir = arg.substr(11);
      argPos++;
    }

    else if (arg == "--") {
      argPos++;
      break;
    }

    else if (arg.find("-") == 0) {
      cerr << "Unrecognized option: " << arg << endl;
      usage();
    }

    else {
      break;
    }
  }

  bounds.sanify();
  bounds_poly = bounds.toPoly();

  SGPropertyNode props;
  if (argPos == ac) {
    usage();
    return 2;
  }

  for (int i = argPos; i < ac; i++) {
    try {
      readProperties(av[i], &props);
    } catch (const sg_throwable &ex) {
      SG_LOG(SG_TERRAIN, SG_ALERT, "Fatal error loading " << av[1] << ": "
	     << ex.getFormattedMessage());
      return 1;
    }

    int nChildren = props.nChildren();
    for (int j = 0; j < nChildren; j++) {
      SGPropertyNode_ptr child = props.getChild(j);
      if (!strcmp("point", child->getName()))
	add_point(child);
      else if (!strcmp("line", child->getName()))
	add_line(child);
      else if (!strcmp("polygon", child->getName()))
	add_polygon(child);
      else
	SG_LOG(SG_TERRAIN, SG_WARN, "Unrecognized shape type "
	       << child->getName());
    }
  }
}
