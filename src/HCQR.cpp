#include "HCQR.h"
#include <memory>

namespace hic::interface {

//BEGIN HCQR

HCQR::HCQR() {}
HCQR::~HCQR() {}

//END HCQR

//BEGIN HCQRBasedOnSpatialGrid

HCQRSpatialGrid::HCQRSpatialGrid(
	sserialize::RCPtrWrapper<hic::interface::SpatialGrid> sg,
	sserialize::RCPtrWrapper<hic::interface::SpatialGridInfo> sgi) :
m_sg(sg),
m_sgi(sgi)
{}

HCQRSpatialGrid::HCQRSpatialGrid(HCQRSpatialGrid const & other) :
m_sg(other.m_sg),
m_sgi(other.m_sgi)
{}

HCQRSpatialGrid::HCQRSpatialGrid(HCQRSpatialGrid && other) :
m_sg(std::move(other.m_sg)),
m_sgi(std::move(other.m_sgi))
{}

HCQRSpatialGrid::~HCQRSpatialGrid() {}

std::vector<HCQRSpatialGrid::PixelId>
HCQRSpatialGrid::pixelChildren(PixelId pid) const {
	std::vector<PixelId> result(sg().childrenCount(pid));
	for(uint32_t i(0), s(result.size()); i < s; ++i) {
		result[i] = sg().index(pid, i);
	}
	std::sort(result.begin(), result.end());
	return result;
}

sserialize::ItemIndex
HCQRSpatialGrid::items(PixelId pid) const {
	return sgi().items(pid);
}

HCQRSpatialGrid::PixelLevel
HCQRSpatialGrid::level(PixelId pid) const {
	return sg().level(pid);
}

}//end namespace hic::interface


