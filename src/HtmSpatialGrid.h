#pragma once

#include "SpatialGrid.h"
#include <lsst/sphgeom/HtmPixelization.h>

namespace hic {
	
class HtmSpatialGrid: public interface::SpatialGrid {
public:
	sserialize::RCPtrWrapper<HtmSpatialGrid> make(uint32_t levels);
public:
	virtual uint32_t maxLevel() const override;
	virtual uint32_t defaultLevel() const override;
public:
	virtual PixelId index(double lat, double lon, Level level) const override;
	virtual PixelId index(double lat, double lon) const override;
	virtual PixelId index(PixelId parent, uint32_t childNumber) const override;
	virtual Size childrenCount(PixelId pixelId) const override;
	virtual std::unique_ptr<TreeNode> tree(CellIterator begin, CellIterator end) const override;
protected:
	HtmSpatialGrid(uint32_t levels);
	virtual ~HtmSpatialGrid();
private:
	std::vector<lsst::sphgeom::HtmPixelization> m_hps;
};

}//end namespace hic
