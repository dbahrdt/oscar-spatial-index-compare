#include "HtmSpatialGrid.h"
#include <sserialize/utility/exceptions.h>

namespace hic {


sserialize::RCPtrWrapper<HtmSpatialGrid>
HtmSpatialGrid::make(uint32_t levels) {
	return sserialize::RCPtrWrapper<HtmSpatialGrid>(new HtmSpatialGrid(levels));
}

uint32_t
HtmSpatialGrid::maxLevel() const {
	return m_hps.size()-1;
}

uint32_t
HtmSpatialGrid::defaultLevel() const {
	return maxLevel();
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

HtmSpatialGrid::HtmSpatialGrid(uint32_t levels) {
	m_hps.reserve(levels+1);
	for(uint32_t i(0); i <= levels; ++i) {
		m_hps.emplace_back(i);
	}
}

HtmSpatialGrid::~HtmSpatialGrid()
{}

}//end namespace hic
