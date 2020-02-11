#pragma once

#include <sserialize/spatial/dgg/SpatialGrid.h>
#include <lsst/sphgeom/HtmPixelization.h>

namespace hic {
	
class HtmSpatialGrid final: public sserialize::spatial::dgg::interface::SpatialGrid {
public:
	using HtmPixelization = lsst::sphgeom::HtmPixelization;
public:
	static constexpr PixelId RootPixelId = 0;
public:
	static sserialize::RCPtrWrapper<HtmSpatialGrid> make(uint32_t maxLevel);
public:
	std::string name() const override;
	Level maxLevel() const override;
	Level defaultLevel() const override;
	PixelId rootPixelId() const override;
	Level level(PixelId pixelId) const override;
	bool isAncestor(PixelId ancestor, PixelId decendant) const override;
public:
	PixelId index(double lat, double lon, Level level) const override;
	PixelId index(double lat, double lon) const override;
	PixelId index(PixelId parent, uint32_t childNumber) const override;
	PixelId parent(PixelId child) const override;
public:
	Size childPosition(PixelId parent, PixelId child) const override;
	Size childrenCount(PixelId pixelId) const override;
	std::unique_ptr<TreeNode> tree(CellIterator begin, CellIterator end) const override;
public:
	double area(PixelId pixel) const override;
	sserialize::spatial::GeoRect bbox(PixelId pixel) const override;
protected:
	HtmSpatialGrid(uint32_t maxLevel);
	~HtmSpatialGrid() override;
private:
	///not thant HtmPixelization levels are off by one: level 0 contains 8 nodes, not a single one
	std::vector<lsst::sphgeom::HtmPixelization> m_hps;
};

}//end namespace hic
