#pragma once
#include "SpatialGrid.h"

namespace hic {
	
class H3SpatialGrid final: public interface::SpatialGrid {
public:
	static sserialize::RCPtrWrapper<H3SpatialGrid> make(uint32_t defaultLevel);
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
	H3SpatialGrid(uint32_t defaultLevel);
	virtual ~H3SpatialGrid ();
private:
	uint32_t m_defaultLevel;
};

}//end namespace hic
