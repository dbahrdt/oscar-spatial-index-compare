#pragma once

#include <functional>

#include "SpatialGrid.h"

#include <sserialize/Static/GeoHierarchy.h>


namespace hic::impl {

class GeoHierarchySpatialGrid: public hic::interface::SpatialGrid {
public:
	struct CostFunction {
		CostFunction() {}
		CostFunction(CostFunction const &) = default;
		virtual ~CostFunction() {}
		virtual double operator()(
			sserialize::Static::spatial::GeoHierarchy::Region const & region,
			sserialize::ItemIndex const & regionCells,
			sserialize::ItemIndex const & cellsCoveredByRegion,
			sserialize::ItemIndex const & coveredCells,
			sserialize::ItemIndex const & coverableCells
		) const = 0;
	};
	
	///cost = regionCells.size() / cellsCoveredByRegion.size()
	struct SimpleCostFunction: public CostFunction {
		SimpleCostFunction() {}
		SimpleCostFunction(SimpleCostFunction const &) = default;
		~SimpleCostFunction() override {}
		double operator()(
			sserialize::Static::spatial::GeoHierarchy::Region const & region,
			sserialize::ItemIndex const & regionCells,
			sserialize::ItemIndex const & cellsCoveredByRegion,
			sserialize::ItemIndex const & coveredCells,
			sserialize::ItemIndex const & coverableCells
		) const override;
	};
	
	///cost = (regionCells.size() + penalFactor * (regionCells.size() - cellsCoveredByRegion.size()))/cellsCoveredByRegion.size()
	struct PenalizeDoubleCoverCostFunction: public CostFunction {
		PenalizeDoubleCoverCostFunction(double penalFactor);
		PenalizeDoubleCoverCostFunction(PenalizeDoubleCoverCostFunction const &) = default;
		~PenalizeDoubleCoverCostFunction() override {}
		double operator()(
			sserialize::Static::spatial::GeoHierarchy::Region const & region,
			sserialize::ItemIndex const & regionCells,
			sserialize::ItemIndex const & cellsCoveredByRegion,
			sserialize::ItemIndex const & coveredCells,
			sserialize::ItemIndex const & coverableCells
		) const override;
		double penalFactor;
	};
	
	template<typename T_BASE>
	struct NoDoubleCoverCostFunction: public CostFunction {
		NoDoubleCoverCostFunction(T_BASE const & base) : m_base(base) {}
		NoDoubleCoverCostFunction(NoDoubleCoverCostFunction const &) = default;
		~NoDoubleCoverCostFunction() override {}
		double operator()(
			sserialize::Static::spatial::GeoHierarchy::Region const & region,
			sserialize::ItemIndex const & regionCells,
			sserialize::ItemIndex const & cellsCoveredByRegion,
			sserialize::ItemIndex const & coveredCells,
			sserialize::ItemIndex const & coverableCells
		) const override {
			if (regionCells.size() != cellsCoveredByRegion.size()) {
				return std::numeric_limits<double>::max();
			}
			else {
				return m_base(region, regionCells, cellsCoveredByRegion, coveredCells, coverableCells);
			}
		}
		T_BASE m_base;
	};
	
	///cost = BaseCost/log_2(cellsCoveredByRegion.size()+1)
	template<typename T_BASE>
	struct PreferLargeCostFunction: public CostFunction {
		PreferLargeCostFunction(PreferLargeCostFunction const &) = default;
		PreferLargeCostFunction(T_BASE const & base) : m_base(base) {}
		~PreferLargeCostFunction() override {}
		double operator()(
			sserialize::Static::spatial::GeoHierarchy::Region const & region,
			sserialize::ItemIndex const & regionCells,
			sserialize::ItemIndex const & cellsCoveredByRegion,
			sserialize::ItemIndex const & coveredCells,
			sserialize::ItemIndex const & coverableCells
		) const override {
			double baseCost = m_base(region, regionCells, cellsCoveredByRegion, coveredCells, coverableCells);
			return baseCost/std::log2(cellsCoveredByRegion.size()+1); //+1 makes sure that result is >= 1
		}
		T_BASE m_base;
	};
	//If type==REGION_DUMMY_FOR_CELL -> ghId = cellId
	class CompoundPixel final {
	public:
		enum Type : int { REGION=0, CELL=1, REGION_DUMMY_FOR_CELL=2};
		enum Flags : int {HAS_TREE_ID=1};
	public:
		CompoundPixel();
		CompoundPixel(Type type, uint32_t ghId, uint32_t treeId);
		CompoundPixel(CompoundPixel const & other) = default;
		CompoundPixel(PixelId other);
		operator PixelId() const;
		Type type() const;
		int flags() const;
		bool hasFlag(int f) const;
		uint32_t ghId() const;
		uint32_t treeId() const;
		bool valid() const;
	private:
		struct Data {
			uint64_t type:2;
			uint64_t flags:1;
			uint64_t ghId:30;
			uint64_t treeId:31;
		} m_d;
	};
public:
	~GeoHierarchySpatialGrid() override;
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
public:
	static sserialize::RCPtrWrapper<GeoHierarchySpatialGrid> make(
		sserialize::Static::spatial::GeoHierarchy const & gh,
		sserialize::Static::ItemIndexStore const & idxStore,
		CostFunction const & costs);
public:
	sserialize::Static::spatial::GeoHierarchy const & gh() const;
	sserialize::Static::ItemIndexStore const & idxStore() const;
public:
	bool valid(CompoundPixel const & cp) const;
public:
	bool isCell(PixelId pid) const;
	bool isRegionDummy(PixelId pid) const;
	bool isRegion(PixelId pid) const;
	PixelId cellIdToPixelId(uint32_t cid) const;
	uint32_t regionId(PixelId pid) const;
	uint32_t cellId(PixelId pid) const;
private:
	class TreeNode {
	public:
		using SizeType = uint32_t;
		static constexpr SizeType npos = std::numeric_limits<SizeType>::max();
	public:
		TreeNode() = default;
		TreeNode(TreeNode const &) = default;
		TreeNode(CompoundPixel const & cp, SizeType parent);
		TreeNode & operator=(TreeNode const &) = default;
	public:
		CompoundPixel const & cp() const;
		CompoundPixel & cp();
		bool isRegion() const;
		bool isRegionDummy() const;
		bool isCell() const;
		SizeType numberOfChildren() const;
		SizeType parentPos() const;
		SizeType childrenBegin() const;
		SizeType childrenEnd() const;
	public:
		uint32_t regionId() const;
		uint32_t cellId() const;
	public:
		void setChildrenBegin(SizeType v);
		void setChildrenEnd(SizeType v);
	private:
		CompoundPixel m_cp;
		SizeType m_parentPos{npos};
		SizeType m_childrenBegin{npos};
		SizeType m_childrenEnd{npos}; //one passed the end
	};
private:
	GeoHierarchySpatialGrid(sserialize::Static::spatial::GeoHierarchy const & gh, sserialize::Static::ItemIndexStore const & idxStore);
private:
	sserialize::Static::spatial::GeoHierarchy m_gh;
	sserialize::Static::ItemIndexStore m_idxStore;
	std::vector<TreeNode> m_tree;
	std::vector<std::size_t> m_cellNodePos;
};

	
}//end namespace
