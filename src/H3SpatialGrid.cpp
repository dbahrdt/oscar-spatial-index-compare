#include <hic/H3SpatialGrid.h>
#include <sserialize/utility/exceptions.h>
#include <sserialize/spatial/dgg/Static/SpatialGridRegistry.h>

#include <h3api.h>

namespace hic {

void H3SpatialGrid::registerWithSpatialGridRegistry() {
	using Registry = sserialize::spatial::dgg::Static::SpatialGridRegistry;
	Registry::get().set("h3", [](Registry::SpatialGridInfo const & info) -> Registry::SpatialGridPtr {
		return H3SpatialGrid::make(info.levels());
	});
}

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

H3SpatialGrid::PixelId
H3SpatialGrid::rootPixelId() const {
	return RootPixelId;
}

H3SpatialGrid::Level
H3SpatialGrid::level(PixelId pixelId) const {
	if (pixelId == RootPixelId) {
		return 0;
	}
	return h3_h3GetResolution(pixelId)+1;
}

bool
H3SpatialGrid::isAncestor(PixelId ancestor, PixelId decendant) const {
	if (ancestor == RootPixelId) {
		return true;
	}
	auto h3lvl = level(decendant)-1;
	for(;h3lvl > 0; --h3lvl) {
		decendant = h3_h3ToParent(decendant, h3lvl-1);
		if (decendant == ancestor) {
			return true;
		}
	}
	return false;
}

H3SpatialGrid::PixelId
H3SpatialGrid::index(double lat, double lon, Level level) const {
	if (level == 0) {
		return RootPixelId;
	}
	GeoCoord coord;
	coord.lat = h3_degsToRads(lat);
	coord.lon = h3_degsToRads(lon);
	return h3_geoToH3(&coord, level-1);
}

H3SpatialGrid::PixelId
H3SpatialGrid::index(double lat, double lon) const {
	return index(lat, lon, defaultLevel());
}

H3SpatialGrid::PixelId
H3SpatialGrid::index(PixelId parent, uint32_t childNumber) const {
	if (parent == RootPixelId) {
		std::vector<PixelId> c(childrenCount(parent));
		h3_getRes0Indexes(c.data());
		return c.at(childNumber);
	}
	else {
		std::vector<PixelId> c(childrenCount(parent));
		h3_h3ToChildren(parent, 1, c.data());
		return c.at(childNumber);
	}
}

H3SpatialGrid::PixelId
H3SpatialGrid::parent(PixelId child) const {
	auto lvl = level(child);
	if (lvl > 1) {
		return h3_h3ToParent(child, lvl-1);
	}
	else if (lvl == 1) {
		return RootPixelId;
	}
	else {
		throw sserialize::spatial::dgg::exceptions::InvalidPixelId("H3SpatialGrid::parent with child=" + std::to_string(child));
	}
}

H3SpatialGrid::Size
H3SpatialGrid::childrenCount(PixelId pixel) const {
	if (pixel == RootPixelId) {
		return h3_res0IndexCount();
	}
	else {
		return h3_maxH3ToChildrenSize(pixel, 1);
	}
}

std::unique_ptr<H3SpatialGrid::TreeNode>
H3SpatialGrid::tree(CellIterator begin, CellIterator end) const {
	throw sserialize::UnimplementedFunctionException("H3SpatialGrid::tree");
	return std::unique_ptr<H3SpatialGrid::TreeNode>();
}

double
H3SpatialGrid::area(PixelId pixel) const {
	if (pixel == RootPixelId) {
		throw sserialize::spatial::dgg::exceptions::InvalidPixelId("H3SpatialGrid: root pixel has no area");
		return 0;
	}
	return h3_hexAreaKm2(level(pixel));
}

sserialize::spatial::GeoRect
H3SpatialGrid::bbox(PixelId pixel) const {
	if (pixel == RootPixelId) {
		throw sserialize::spatial::dgg::exceptions::InvalidPixelId("H3SpatialGrid: root pixel has no boundary");
		return sserialize::spatial::GeoRect();
	}
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
