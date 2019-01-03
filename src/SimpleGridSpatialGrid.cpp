#include "SimpleGridSpatialGrid.h"
#include <sserialize/utility/exceptions.h>

#include <lsst/sphgeom/LonLat.h>
#include <lsst/sphgeom/Circle.h>
#include <lsst/sphgeom/Box.h>

namespace hic {

sserialize::RCPtrWrapper<SimpleGridSpatialGrid>
SimpleGridSpatialGrid::make(uint32_t levels) {
	return sserialize::RCPtrWrapper<SimpleGridSpatialGrid>(new SimpleGridSpatialGrid(levels));
}

std::string
SimpleGridSpatialGrid::name() const {
	return "simplegrid";
}

SimpleGridSpatialGrid::Level
SimpleGridSpatialGrid::maxLevel() const {
	return 15;
}

SimpleGridSpatialGrid::Level
SimpleGridSpatialGrid::defaultLevel() const {
	return m_grids.size()-1;
}

SimpleGridSpatialGrid::Level
SimpleGridSpatialGrid::level(PixelId pixelId) const {
	return pixelId & LevelMask;
}

SimpleGridSpatialGrid::PixelId
SimpleGridSpatialGrid::index(double lat, double lon, Level level) const {
	return (m_grids.at(level).select(lat, lon).tile << 8) | uint8_t(level);
}

SimpleGridSpatialGrid::PixelId
SimpleGridSpatialGrid::index(double lat, double lon) const {
	return index(lat, lon, defaultLevel());
}

SimpleGridSpatialGrid::PixelId
SimpleGridSpatialGrid::index(PixelId parent, uint32_t childNumber) const {
	throw sserialize::UnimplementedFunctionException("SimpleGridSpatialGrid is not hierarchical (yet)");
	return 0;
}

SimpleGridSpatialGrid::Size
SimpleGridSpatialGrid::childrenCount(PixelId pixel) const {
	return 4;
}

std::unique_ptr<SimpleGridSpatialGrid::TreeNode>
SimpleGridSpatialGrid::tree(CellIterator begin, CellIterator end) const {
	return std::unique_ptr<SimpleGridSpatialGrid::TreeNode>();
}

double
SimpleGridSpatialGrid::area(PixelId pixel) const {
	return bbox(pixel).area()/(1000*1000);
}

sserialize::spatial::GeoRect
SimpleGridSpatialGrid::bbox(PixelId pixel) const {
	uint8_t level = this->level(pixel);
	uint32_t tile = pixel >> LevelBits;
	return grid(level).cellBoundary(grid(level).select(tile));
}

SimpleGridSpatialGrid::SimpleGridSpatialGrid(uint32_t levels)
{
	sserialize::spatial::GeoRect rect(-90, 90, -180, 180);
	for(uint32_t i(0); i <= levels; ++i) {
		m_grids.emplace_back(rect, uint32_t(1) << i, uint32_t(1) << i);
	}
}

SimpleGridSpatialGrid::~SimpleGridSpatialGrid()
{}

sserialize::spatial::GeoGrid const &
SimpleGridSpatialGrid::grid(uint8_t level) const {
	return m_grids.at(level);
}

}//end namespace hic
