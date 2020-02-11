#include <hic/S2GeomSpatialGrid.h>
#include <sserialize/utility/exceptions.h>
#include <sserialize/spatial/dgg/Static/SpatialGridRegistry.h>

#include <s2/s2cell_id.h>
#include <s2/s2cell.h>
#include <s2/s2latlng.h>
#include <s2/s2latlng_rect.h>

namespace hic {
	
void S2GeomSpatialGrid::registerWithSpatialGridRegistry() {
	using Registry = sserialize::spatial::dgg::Static::SpatialGridRegistry;
	Registry::get().set("s2geom", [](Registry::SpatialGridInfo const & info) -> Registry::SpatialGridPtr {
		return S2GeomSpatialGrid::make(info.levels());
	});
}

sserialize::RCPtrWrapper<S2GeomSpatialGrid>
S2GeomSpatialGrid::make(uint32_t defLevel) {
	return sserialize::RCPtrWrapper<S2GeomSpatialGrid>(new S2GeomSpatialGrid(defLevel));
}

std::string
S2GeomSpatialGrid::name() const {
	return "s2geom";
}

S2GeomSpatialGrid::Level
S2GeomSpatialGrid::maxLevel() const {
	return m_defaultLevel;
}

S2GeomSpatialGrid::Level
S2GeomSpatialGrid::defaultLevel() const {
	return m_defaultLevel;
}

S2GeomSpatialGrid::PixelId
S2GeomSpatialGrid::rootPixelId() const {
	return RootPixelId;
}

S2GeomSpatialGrid::Level
S2GeomSpatialGrid::level(PixelId pixelId) const {
	if (pixelId == RootPixelId) {
		return 0;
	}
	return S2CellId(pixelId).level()+1;
}

bool
S2GeomSpatialGrid::isAncestor(PixelId ancestor, PixelId decendant) const {
	if (ancestor == RootPixelId) {
		return true;
	}
	while(decendant != RootPixelId) {
		decendant = parent(decendant);
		if (decendant == ancestor) {
			return true;
		}
	}
	return false;
}

S2GeomSpatialGrid::PixelId
S2GeomSpatialGrid::index(double lat, double lon, Level level) const {
	if (level == 0) {
		return RootPixelId;
	}
	return S2CellId(S2LatLng::FromDegrees(lat, lon)).parent(level+1).id();
}

S2GeomSpatialGrid::PixelId
S2GeomSpatialGrid::index(double lat, double lon) const {
	return index(lat, lon, defaultLevel());
}

S2GeomSpatialGrid::PixelId
S2GeomSpatialGrid::index(PixelId parent, uint32_t childNumber) const {
	if (parent == RootPixelId) {
		return S2CellId::FromFace(childNumber).id();
	}
	else {
		return S2CellId(parent).child(childNumber).id();
	}
}

S2GeomSpatialGrid::PixelId
S2GeomSpatialGrid::parent(PixelId child) const {
	auto lvl = level(child);
	if (lvl > 1) {
		return S2CellId(child).parent().id();
	}
	else if (lvl == 1) {
		return RootPixelId;
	}
	else {
		throw sserialize::spatial::dgg::exceptions::InvalidPixelId("S2GeomSpatialGrid::parent with child=" + std::to_string(child));
	}
}

S2GeomSpatialGrid::Size
S2GeomSpatialGrid::childrenCount(PixelId pixel) const {
	if (pixel == RootPixelId) {
		return 6;
	}
	else {
		return 4;
	}
}

std::unique_ptr<S2GeomSpatialGrid::TreeNode>
S2GeomSpatialGrid::tree(CellIterator begin, CellIterator end) const {
	throw sserialize::UnimplementedFunctionException("S2GeomSpatialGrid::tree");
	return std::unique_ptr<S2GeomSpatialGrid::TreeNode>();
}

double
S2GeomSpatialGrid::area(PixelId pixel) const {
	if (pixel == RootPixelId) {
		throw sserialize::spatial::dgg::exceptions::InvalidPixelId("S2GeomSpatialGrid: root pixel has no area");
		return 0;
	}
	return (12700/2)*(12700/2) * S2Cell(S2CellId(pixel)).ApproxArea();
}

sserialize::spatial::GeoRect
S2GeomSpatialGrid::bbox(PixelId pixel) const {
	if (pixel == RootPixelId) {
		throw sserialize::spatial::dgg::exceptions::InvalidPixelId("S2GeomSpatialGrid: root pixel has no boundary");
		return sserialize::spatial::GeoRect();
	}
	S2LatLngRect s2r = S2Cell(S2CellId(pixel)).GetRectBound();
	return sserialize::spatial::GeoRect(
		s2r.lat_lo().degrees(), s2r.lat_hi().degrees(),
		s2r.lng_lo().degrees(), s2r.lng_hi().degrees()
	);
}

S2GeomSpatialGrid::S2GeomSpatialGrid(uint32_t defLevel) :
m_defaultLevel(defLevel)
{}

S2GeomSpatialGrid::~S2GeomSpatialGrid()
{}

}//end namespace hic
