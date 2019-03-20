#pragma once

#include "SpatialGrid.h"
#include <sserialize/spatial/GeoGrid.h>

namespace hic {
/*
 * 
 * union InternalPixelId {
 *     PixelId pixelId;
 *     struct Internal {
 *        uint64_t level:8;
 *        uint64_t tile:32;
 *        uint64_t dummy:24;
 *     };
 * };
 */
	
class SimpleGridSpatialGrid final: public interface::SpatialGrid {
public:
	static sserialize::RCPtrWrapper<SimpleGridSpatialGrid> make(uint32_t maxLevel);
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
	SimpleGridSpatialGrid(uint32_t maxLevel);
	~SimpleGridSpatialGrid() override;
private:
	using TileId = uint32_t;
	static constexpr int LevelBits = 8;
	static constexpr uint8_t LevelMask = 0xFF;
private:
	sserialize::spatial::GeoGrid const & grid(uint8_t level) const;
private:
	std::vector<sserialize::spatial::GeoGrid> m_grids;
};

}//end namespace hic
