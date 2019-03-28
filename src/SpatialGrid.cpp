#include "SpatialGrid.h"

namespace hic::interface {
	

uint32_t
SpatialGrid::childPosition(PixelId parent, PixelId child) const {
	uint32_t result = std::numeric_limits<uint32_t>::max();
	for(uint32_t i(0), s(childrenCount(parent)); i < s; ++i) {
		if (index(parent, i) == child) {
			return i;
		}
	}
	throw hic::exceptions::InvalidPixelId(this->to_string(child) + " is not a child of " + this->to_string(parent));
	return result;
}

std::string
SpatialGrid::to_string(PixelId pixel) const {
	return std::to_string(pixel);
}

}//end namespace hic::interface
