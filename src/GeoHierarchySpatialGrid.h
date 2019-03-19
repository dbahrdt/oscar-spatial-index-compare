#pragma once

#include <functional>

#include "SpatialGrid.h"

#include <sserialize/Static/GeoHierarchy.h>


namespace hic::impl {

class GeoHierarchySpatialGrid: public hic::interface::SpatialGrid {
public:
	struct CostFunction {
		virtual double operator()(
			sserialize::Static::spatial::GeoHierarchy::Region const & region,
			sserialize::ItemIndex const & regionCells,
			sserialize::ItemIndex const & cellsCoveredByRegion
		) const = 0;
	};
	
	///cost = regionCells.size() / cellsCoveredByRegion.size()
	struct SimpleCostFunction: public CostFunction {
		double operator()(
			sserialize::Static::spatial::GeoHierarchy::Region const & region,
			sserialize::ItemIndex const & regionCells,
			sserialize::ItemIndex const & cellsCoveredByRegion
		) const override;
	};
	
	///cost = (regionCells.size() + penalFactor * (regionCells.size() - cellsCoveredByRegion.size()))/cellsCoveredByRegion.size()
	struct PenalizeDoubleCoverCostFunction: public CostFunction {
		PenalizeDoubleCoverCostFunction(double penalFactor);
		double operator()(
			sserialize::Static::spatial::GeoHierarchy::Region const & region,
			sserialize::ItemIndex const & regionCells,
			sserialize::ItemIndex const & cellsCoveredByRegion
		) const override;
		double penalFactor;
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
		CostFunction const & costs);
public:
	sserialize::Static::spatial::GeoHierarchy const & gh() const;
	sserialize::Static::ItemIndexStore const & idxStore() const;
public:
	static bool isCell(PixelId pid);
	static bool isRegion(PixelId pid);
	static PixelId regionIdToPixelId(uint32_t rid);
	static PixelId cellIdToPixelId(uint32_t cid);
	static uint32_t regionId(PixelId pid);
	static uint32_t cellId(PixelId pid);
private:
	class TreeNode {
	public:
		using SizeType = uint32_t;
		static constexpr SizeType npos = std::numeric_limits<SizeType>::max();
	public:
		TreeNode(PixelId pid, SizeType parent);
	public:
		PixelId pixelId() const;
		bool isRegion() const;
		bool isCell() const;
		SizeType numberOfChildren() const;
		SizeType parentPos() const;
		SizeType childrenBegin() const;
		SizeType childrenEnd() const;
	public:
		void setChildrenBegin(SizeType v);
		void setChildrenEnd(SizeType v);
	private:
		PixelId m_pid;
		SizeType m_parentPos;
		SizeType m_childrenBegin;
		SizeType m_childrenEnd; //one passed the end
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
