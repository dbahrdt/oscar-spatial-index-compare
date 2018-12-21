#include "HtmSpatialGrid.h"
#include <sserialize/utility/exceptions.h>

#include <lsst/sphgeom/LonLat.h>
#include <lsst/sphgeom/Circle.h>
#include <lsst/sphgeom/Box.h>

namespace hic {


sserialize::RCPtrWrapper<HtmSpatialGrid>
HtmSpatialGrid::make(uint32_t levels) {
	return sserialize::RCPtrWrapper<HtmSpatialGrid>(new HtmSpatialGrid(levels));
}

HtmSpatialGrid::Level
HtmSpatialGrid::maxLevel() const {
	return m_hps.size()-1;
}

HtmSpatialGrid::Level
HtmSpatialGrid::defaultLevel() const {
	return maxLevel();
}


HtmSpatialGrid::Level
HtmSpatialGrid::level(PixelId pixelId) const {
	return lsst::sphgeom::HtmPixelization::level(pixelId);
}

HtmSpatialGrid::PixelId
HtmSpatialGrid::index(double lat, double lon, Level level) const {
	return m_hps.at(level).index(
		lsst::sphgeom::UnitVector3d(
			lsst::sphgeom::LonLat::fromDegrees(lon, lat)
		)
	);
}

HtmSpatialGrid::PixelId
HtmSpatialGrid::index(double lat, double lon) const {
	return index(lat, lon, defaultLevel());
}

HtmSpatialGrid::PixelId
HtmSpatialGrid::index(PixelId parent, uint32_t childNumber) const {
	if (UNLIKELY_BRANCH(childNumber > 3)) {
		throw sserialize::OutOfBoundsException("HtmSpatialGrid only has 4 children");
	}
	return (parent << 2) | childNumber;
}

HtmSpatialGrid::Size
HtmSpatialGrid::childrenCount(PixelId pixel) const {
	return 4;
}

std::unique_ptr<HtmSpatialGrid::TreeNode>
HtmSpatialGrid::tree(CellIterator begin, CellIterator end) const {
	return std::unique_ptr<HtmSpatialGrid::TreeNode>();
}

double
HtmSpatialGrid::area(PixelId pixel) const {
	return (12700/2)*(12700/2) * HtmPixelization::triangle(pixel).getBoundingCircle().getArea();
}


sserialize::spatial::GeoRect
HtmSpatialGrid::bbox(PixelId pixel) const {
	lsst::sphgeom::Box box = HtmPixelization::triangle(pixel).getBoundingBox();
	auto lat = box.getLat();
	auto lon = box.getLon();
	return sserialize::spatial::GeoRect(lat.getA().asDegrees(), lat.getB().asDegrees(), lon.getA().asDegrees(), lon.getB().asDegrees());
}

HtmSpatialGrid::HtmSpatialGrid(uint32_t levels) {
	m_hps.reserve(levels+1);
	for(uint32_t i(0); i <= levels; ++i) {
		m_hps.emplace_back(i);
	}
}

HtmSpatialGrid::~HtmSpatialGrid()
{}

}//end namespace hic
