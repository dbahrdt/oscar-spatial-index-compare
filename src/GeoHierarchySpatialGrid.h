#pragma once

#include <functional>

#include "SpatialGrid.h"

#include <sserialize/Static/GeoHierarchy.h>


namespace hic::impl {

class GeoHierarchySpatialGrid: public hic::interface::SpatialGrid {
public:
	struct CostFunction {
		virtual double operator()(uint32_t regionId, std::vector<uint32_t> const & coveredCells);
	};
public:
	~GeoHierarchySpatialGrid() override;
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
public:
	static sserialize::RCPtrWrapper<GeoHierarchySpatialGrid> make(
		sserialize::Static::spatial::GeoHierarchy const & gh,
		sserialize::Static::ItemIndexStore const & idxStore,
		CostFunction & costs);
public:
	static bool isCell(PixelId pid);
	static bool isRegion(PixelId pid);
	static PixelId regionIdToPixelId(uint32_t rid);
	static PixelId cellIdToPixelId(uint32_t cid);
private:
	class TreeNode {
	public:
		static constexpr std::size_t npos = std::numeric_limits<std::size_t>::max();
	public:
		inline PixelId pixelId() const { return m_pid; }
	public:
		bool isRegion() const;
		bool isCell() const;
		std::size_t numberOfChildren() const;
		std::size_t parentPos() const;
		std::size_t childrenBegin() const;
		std::size_t childrenEnd() const;
	private:
		PixelId m_pid;
		std::size_t m_parentPos;
		std::size_t m_childrenBegin;
		std::size_t m_childrenEnd; //one passed the end
	};
	enum class PixelType : int { REGION=0x0, CELL=0x1};
private:
	GeoHierarchySpatialGrid(sserialize::Static::spatial::GeoHierarchy const & gh, sserialize::Static::ItemIndexStore const & idxStore);
private:
	sserialize::Static::spatial::GeoHierarchy m_gh;
	sserialize::Static::ItemIndexStore m_idxStore;
	std::vector<TreeNode> m_tree;
	std::unordered_map<PixelId, std::size_t> m_pid2tn;
};

	
}//end namespace
