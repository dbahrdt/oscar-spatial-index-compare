#pragma once
#include "SpatialGrid.h"

namespace hic {

///Note that h3-index levels are off by one
class S2GeomSpatialGrid final: public interface::SpatialGrid {
public:
	static constexpr PixelId RootPixelId = std::numeric_limits<PixelId>::max();
public:
	static sserialize::RCPtrWrapper<S2GeomSpatialGrid> make(uint32_t defaultLevel);
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
	Size childrenCount(PixelId pixelId) const override;
	std::unique_ptr<TreeNode> tree(CellIterator begin, CellIterator end) const override;
public:
	double area(PixelId pixel) const override;
	sserialize::spatial::GeoRect bbox(PixelId pixel) const override;
protected:
	S2GeomSpatialGrid(uint32_t defaultLevel);
	~S2GeomSpatialGrid() override;
private:
	uint32_t m_defaultLevel;
};

}//end namespace hic
