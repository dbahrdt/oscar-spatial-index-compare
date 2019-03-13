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
	std::string name() const override;
	Level maxLevel() const override;
	Level defaultLevel() const override;
	Level level(PixelId pixelId) const override;
	bool isAncestor(PixelId ancestor, PixelId decendant) const override;
public:
	PixelId index(double lat, double lon, Level level) const override;
	PixelId index(double lat, double lon) const override;
	PixelId index(PixelId parent, uint32_t childNumber) const override;
	PixelId parent(PixelId child) const override;
public:
	Size childrenCount(PixelId pixelId) const override;
	std::unique_ptr<TreeNode> tree(CellIterator begin, CellIterator end) const override;
public:
	double area(PixelId pixel) const override;
	sserialize::spatial::GeoRect bbox(PixelId pixel) const override;
protected:
	HtmSpatialGrid(uint32_t levels);
	~HtmSpatialGrid() override;
private:
	std::vector<lsst::sphgeom::HtmPixelization> m_hps;
};

}//end namespace hic
