#include "H3SpatialGrid.h"
#include <sserialize/utility/exceptions.h>

#include <h3api.h>

namespace hic {


sserialize::RCPtrWrapper<H3SpatialGrid>
H3SpatialGrid::make(uint32_t defLevel) {
	return sserialize::RCPtrWrapper<H3SpatialGrid>(new H3SpatialGrid(defLevel));
}

std::string
H3SpatialGrid::name() const {
	return "h3";
}

H3SpatialGrid::Level
H3SpatialGrid::maxLevel() const {
	return m_defaultLevel;
}

H3SpatialGrid::Level
H3SpatialGrid::defaultLevel() const {
	return m_defaultLevel;
}


H3SpatialGrid::Level
H3SpatialGrid::level(PixelId pixelId) const {
	return h3_h3GetResolution(pixelId);
}


bool
H3SpatialGrid::isAncestor(PixelId ancestor, PixelId decendant) const {
	auto lvl = level(decendant);
	for(;lvl > 0; --lvl) {
		decendant = h3_h3ToParent(decendant, lvl-1);
		if (decendant == ancestor) {
			return true;
		}
	}
	return false;
}

H3SpatialGrid::PixelId
H3SpatialGrid::index(double lat, double lon, Level level) const {
	GeoCoord coord;
	coord.lat = h3_degsToRads(lat);
	coord.lon = h3_degsToRads(lon);
	return h3_geoToH3(&coord, level);
}

H3SpatialGrid::PixelId
H3SpatialGrid::index(double lat, double lon) const {
	return index(lat, lon, defaultLevel());
}

H3SpatialGrid::PixelId
H3SpatialGrid::index(PixelId parent, uint32_t childNumber) const {
	std::vector<PixelId> c(childrenCount(parent));
	h3_h3ToChildren(parent, 1, c.data());
	return c.at(childNumber);
}

H3SpatialGrid::PixelId
H3SpatialGrid::parent(PixelId child) const {
	auto lvl = level(child);
	if (lvl > 0) {
		return h3_h3ToParent(child, lvl-1);
	}
	else {
		throw hic::exceptions::InvalidPixelId("H3SpatialGrid::parent with child=" + std::to_string(child));
	}
}

H3SpatialGrid::Size
H3SpatialGrid::childrenCount(PixelId pixel) const {
	return h3_maxH3ToChildrenSize(pixel, 1);
}

std::unique_ptr<H3SpatialGrid::TreeNode>
H3SpatialGrid::tree(CellIterator begin, CellIterator end) const {
	return std::unique_ptr<H3SpatialGrid::TreeNode>();
}


double
H3SpatialGrid::area(PixelId pixel) const {
	return h3_hexAreaKm2(level(pixel));
}


sserialize::spatial::GeoRect
H3SpatialGrid::bbox(PixelId pixel) const {
	GeoBoundary gb;
	h3_h3ToGeoBoundary(pixel, &gb);
	sserialize::spatial::GeoRect rect;
	for(int i(0); i < gb.numVerts; ++i) {
		rect.enlarge(h3_degsToRads(gb.verts[i].lat), h3_degsToRads(gb.verts[i].lon));
	}
	return rect;
}

H3SpatialGrid::H3SpatialGrid(uint32_t defLevel) :
m_defaultLevel(defLevel)
{}

H3SpatialGrid::~H3SpatialGrid()
{}

}//end namespace hic
