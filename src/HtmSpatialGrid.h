#pragma once

#include "SpatialGrid.h"
#include <lsst/sphgeom/HtmPixelization.h>

namespace hic {
	
class HtmSpatialGrid final: public interface::SpatialGrid {
public:
	using HtmPixelization = lsst::sphgeom::HtmPixelization;
public:
	static sserialize::RCPtrWrapper<HtmSpatialGrid> make(uint32_t levels);
public:
	virtual Level maxLevel() const override;
	virtual Level defaultLevel() const override;
	virtual Level level(PixelId pixelId) const override;
public:
	virtual PixelId index(double lat, double lon, Level level) const override;
	virtual PixelId index(double lat, double lon) const override;
	virtual PixelId index(PixelId parent, uint32_t childNumber) const override;
	virtual Size childrenCount(PixelId pixelId) const override;
	virtual std::unique_ptr<TreeNode> tree(CellIterator begin, CellIterator end) const override;
public:
	virtual double area(PixelId pixel) const override;
	virtual sserialize::spatial::GeoRect bbox(PixelId pixel) const override;
protected:
	HtmSpatialGrid(uint32_t levels);
	virtual ~HtmSpatialGrid();
private:
	std::vector<lsst::sphgeom::HtmPixelization> m_hps;
};

}//end namespace hic
