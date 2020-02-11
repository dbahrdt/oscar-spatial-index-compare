#include <hic/H3SpatialGrid.h>
#include <hic/HtmSpatialGrid.h>
#include <hic/S2GeomSpatialGrid.h>
#include <sserialize/spatial/dgg/SimpleGridSpatialGrid.h>
#include <sserialize/utility/debug.h>

namespace {

NO_OPTIMIZE bool hic_spatial_grid_init_fn() {
	hic::H3SpatialGrid::registerWithSpatialGridRegistry();
	hic::HtmSpatialGrid::registerWithSpatialGridRegistry();
	hic::S2GeomSpatialGrid::registerWithSpatialGridRegistry();
	sserialize::spatial::dgg::SimpleGridSpatialGrid::registerWithSpatialGridRegistry();
	return true;
}

static volatile bool hic_spatial_grid_init_dummy = hic_spatial_grid_init_fn();

}
