#include "HtmSpatialGrid.h"
#include <sserialize/utility/exceptions.h>

#include <lsst/sphgeom/LonLat.h>
#include <lsst/sphgeom/Circle.h>
#include <lsst/sphgeom/Box.h>

namespace hic {

sserialize::RCPtrWrapper<HtmSpatialGrid>
HtmSpatialGrid::make(uint32_t maxLevel) {
	return sserialize::RCPtrWrapper<HtmSpatialGrid>(new HtmSpatialGrid(maxLevel));
}

std::string
HtmSpatialGrid::name() const {
	return "htm";
}

HtmSpatialGrid::Level
HtmSpatialGrid::maxLevel() const {
	return m_hps.size();
}

HtmSpatialGrid::Level
HtmSpatialGrid::defaultLevel() const {
	return maxLevel();
}

HtmSpatialGrid::PixelId
HtmSpatialGrid::rootPixelId() const {
	return RootPixelId;
}

HtmSpatialGrid::Level
HtmSpatialGrid::level(PixelId pixelId) const {
	if (pixelId == RootPixelId) {
		return 0;
	}
	auto result = lsst::sphgeom::HtmPixelization::level(pixelId);
	if (result < 0) {
		throw hic::exceptions::InvalidPixelId("HtmSpatialGrid: invalid pixelId");
	}
	return result+1;
}

bool
HtmSpatialGrid::isAncestor(PixelId ancestor, PixelId decendant) const {
	if (ancestor == RootPixelId) {
		return true;
	}
	auto alvl = level(ancestor);
	auto dlvl = level(decendant);
	return (alvl < dlvl) && (decendant >> (2*(dlvl-alvl))) == ancestor;
}

HtmSpatialGrid::PixelId
HtmSpatialGrid::index(double lat, double lon, Level level) const {
	if (level == 0) {
		return RootPixelId;
	}
	else {
		return m_hps.at(level-1).index(
			lsst::sphgeom::UnitVector3d(
				lsst::sphgeom::LonLat::fromDegrees(lon, lat)
			)
		);
	}
}

HtmSpatialGrid::PixelId
HtmSpatialGrid::index(double lat, double lon) const {
	return index(lat, lon, defaultLevel());
}

HtmSpatialGrid::PixelId
HtmSpatialGrid::index(PixelId parent, uint32_t childNumber) const {
	if (parent == RootPixelId) {
		if (UNLIKELY_BRANCH(childNumber > 8)) {
			throw hic::exceptions::InvalidPixelId("HtmSpatialGrid only has 8 root children");
		}
		return 8+childNumber;
	}
	else {
		if (UNLIKELY_BRANCH(childNumber > 3)) {
			throw hic::exceptions::InvalidPixelId("HtmSpatialGrid only has 4 children");
		}
		return (parent << 2) | childNumber;
	}
}

HtmSpatialGrid::PixelId
HtmSpatialGrid::parent(PixelId child) const {
	auto lvl = level(child);
	if (lvl > 1) {
		return child >> 2;
	}
	else if (lvl == 1) {
		return RootPixelId;
	}
	else {
		throw hic::exceptions::InvalidPixelId("HtmSpatialGrid::parent with child=" + std::to_string(child));
	}
}

HtmSpatialGrid::Size
HtmSpatialGrid::childPosition(PixelId parent, PixelId child) const {
	if (parent == RootPixelId) {
		return child-8;
	}
	else {
		return child & 0x3;
	}
}

HtmSpatialGrid::Size
HtmSpatialGrid::childrenCount(PixelId pixel) const {
	return pixel == RootPixelId ? 8 :  4;
}

std::unique_ptr<HtmSpatialGrid::TreeNode>
HtmSpatialGrid::tree(CellIterator begin, CellIterator end) const {
	throw sserialize::UnimplementedFunctionException("HtmSpatialGrid::tree");
	return std::unique_ptr<HtmSpatialGrid::TreeNode>();
}

double
HtmSpatialGrid::area(PixelId pixel) const {
	if (pixel == RootPixelId) {
		throw hic::exceptions::InvalidPixelId("HtmSpatialGrid: root pixel has no area");
		return 0;
	}
	return (12700/2)*(12700/2) * HtmPixelization::triangle(pixel).getBoundingCircle().getArea();
}


sserialize::spatial::GeoRect
HtmSpatialGrid::bbox(PixelId pixel) const {
	if (pixel == RootPixelId) {
		throw hic::exceptions::InvalidPixelId("HtmSpatialGrid: root pixel has no boundary");
		return sserialize::spatial::GeoRect();
	}
	lsst::sphgeom::Box box = HtmPixelization::triangle(pixel).getBoundingBox();
	auto lat = box.getLat();
	auto lon = box.getLon();
	return sserialize::spatial::GeoRect(lat.getA().asDegrees(), lat.getB().asDegrees(), lon.getA().asDegrees(), lon.getB().asDegrees());
}

HtmSpatialGrid::HtmSpatialGrid(uint32_t maxLevel) {
	m_hps.reserve(maxLevel+1);
	for(uint32_t i(0); i <= maxLevel; ++i) {
		m_hps.emplace_back(i);
	}
}

HtmSpatialGrid::~HtmSpatialGrid()
{}

}//end namespace hic
