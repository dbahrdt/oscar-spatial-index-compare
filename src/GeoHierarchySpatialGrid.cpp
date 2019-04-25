#include "GeoHierarchySpatialGrid.h"


namespace hic::impl {
	
//BEGIN GeoHierarchySpatialGrid Cost functions
	
double
GeoHierarchySpatialGrid::SimpleCostFunction::operator()(
	sserialize::Static::spatial::GeoHierarchy::Region const & /*region*/,
	sserialize::ItemIndex const & regionCells,
	sserialize::ItemIndex const & cellsCoveredByRegion,
	sserialize::ItemIndex const & /*coveredCells*/,
	sserialize::ItemIndex const & /*coverableCells*/
) const
{
	return double(regionCells.size()) / cellsCoveredByRegion.size(); 
}
	
GeoHierarchySpatialGrid::PenalizeDoubleCoverCostFunction::PenalizeDoubleCoverCostFunction(double penalFactor) :
penalFactor(penalFactor)
{}

double
GeoHierarchySpatialGrid::PenalizeDoubleCoverCostFunction::operator()(
	sserialize::Static::spatial::GeoHierarchy::Region const & /*region*/,
	sserialize::ItemIndex const & regionCells,
	sserialize::ItemIndex const & cellsCoveredByRegion,
	sserialize::ItemIndex const & /*coveredCells*/,
	sserialize::ItemIndex const & /*coverableCells*/
) const
{
	return double(regionCells.size() + penalFactor * (regionCells.size() - cellsCoveredByRegion.size()))/cellsCoveredByRegion.size();
}
	
//END GeoHierarchySpatialGrid Cost functions

//BEGIN GeoHierarchySpatialGrid::CompoundPixel


GeoHierarchySpatialGrid::CompoundPixel::CompoundPixel() :
CompoundPixel(REGION, sserialize::Static::spatial::GeoHierarchy::npos, TreeNode::npos)
{}

GeoHierarchySpatialGrid::CompoundPixel::CompoundPixel(Type type, uint32_t ghId, uint32_t treeId) {
	m_d.type = type;
	m_d.ghId = ghId;
	m_d.treeId = treeId;
	if (treeId != TreeNode::npos) {
		m_d.flags = CompoundPixel::HAS_TREE_ID;
	}
}

GeoHierarchySpatialGrid::CompoundPixel::CompoundPixel(PixelId other) {
	static_assert(sizeof(PixelId) == sizeof(Data));
	::memmove(&m_d, &other, sizeof(PixelId));
}

GeoHierarchySpatialGrid::CompoundPixel::operator PixelId() const {
	static_assert(sizeof(PixelId) == sizeof(Data));
	PixelId pid = 0;
	::memmove(&pid, &m_d, sizeof(PixelId));
	return pid;
}

GeoHierarchySpatialGrid::CompoundPixel::Type
GeoHierarchySpatialGrid::CompoundPixel::type() const {
	return Type(m_d.type);
}

int
GeoHierarchySpatialGrid::CompoundPixel::flags() const {
	return m_d.flags;
}

bool
GeoHierarchySpatialGrid::CompoundPixel::hasFlag(int f) const {
	return flags() & f;
}

uint32_t
GeoHierarchySpatialGrid::CompoundPixel::ghId() const {
	return m_d.ghId;
}

uint32_t
GeoHierarchySpatialGrid::CompoundPixel::treeId() const {
	return m_d.treeId;
}

bool
GeoHierarchySpatialGrid::CompoundPixel::valid() const {
	return m_d.ghId != sserialize::Static::spatial::GeoHierarchy::npos;
}

//END GeoHierarchySpatialGrid::CompoundPixel
	
//BEGIN GeoHierarchySpatialGrid::TreeNode
	
GeoHierarchySpatialGrid::TreeNode::TreeNode(CompoundPixel const & cp, SizeType parent) :
m_cp(cp),
m_parentPos(parent)
{}

GeoHierarchySpatialGrid::CompoundPixel const &
GeoHierarchySpatialGrid::TreeNode::cp() const {
	return m_cp;
}

GeoHierarchySpatialGrid::CompoundPixel &
GeoHierarchySpatialGrid::TreeNode::cp() {
	return m_cp;
}

bool
GeoHierarchySpatialGrid::TreeNode::isRegion() const {
	return m_cp.type() == CompoundPixel::REGION;
}

bool
GeoHierarchySpatialGrid::TreeNode::isCell() const {
	return m_cp.type() == CompoundPixel::CELL;
}

GeoHierarchySpatialGrid::TreeNode::SizeType
GeoHierarchySpatialGrid::TreeNode::numberOfChildren() const {
	return m_childrenEnd - m_childrenBegin;
}

GeoHierarchySpatialGrid::TreeNode::SizeType
GeoHierarchySpatialGrid::TreeNode::parentPos() const {
	return m_parentPos;
}

GeoHierarchySpatialGrid::TreeNode::SizeType
GeoHierarchySpatialGrid::TreeNode::childrenBegin() const {
	return m_childrenBegin;
}

GeoHierarchySpatialGrid::TreeNode::SizeType
GeoHierarchySpatialGrid::TreeNode::childrenEnd() const {
	return m_childrenEnd;
}

uint32_t
GeoHierarchySpatialGrid::TreeNode::regionId() const {
	return m_cp.ghId();
}

uint32_t
GeoHierarchySpatialGrid::TreeNode::cellId() const {
	return m_cp.ghId();
}

void
GeoHierarchySpatialGrid::TreeNode::setChildrenBegin(GeoHierarchySpatialGrid::TreeNode::SizeType v) {
	m_childrenBegin = v;
}

void
GeoHierarchySpatialGrid::TreeNode::setChildrenEnd(GeoHierarchySpatialGrid::TreeNode::SizeType v) {
	m_childrenEnd = v;
}

GeoHierarchySpatialGrid::GeoHierarchySpatialGrid(
	sserialize::Static::spatial::GeoHierarchy const & gh,
	sserialize::Static::ItemIndexStore const & idxStore
) :
m_gh(gh),
m_idxStore(idxStore),
m_cellNodePos(m_gh.cellSize(), TreeNode::npos)
{}

GeoHierarchySpatialGrid::~GeoHierarchySpatialGrid() {}

std::string
GeoHierarchySpatialGrid::name() const {
	return "GeoHierarchySpatialGrid";
}

GeoHierarchySpatialGrid::Level
GeoHierarchySpatialGrid::maxLevel() const {
	return 256;
}

GeoHierarchySpatialGrid::Level
GeoHierarchySpatialGrid::defaultLevel() const {
	return 0;
}

GeoHierarchySpatialGrid::PixelId
GeoHierarchySpatialGrid::rootPixelId() const {
	return m_tree.front().cp();
}

GeoHierarchySpatialGrid::Level
GeoHierarchySpatialGrid::level(PixelId pixelId) const {
	CompoundPixel cp(pixelId);
	if (!valid(cp)) {
		throw sserialize::OutOfBoundsException("Invalid pixel id");
	}
	std::size_t myLevel = 0;
	std::size_t nodePos = cp.treeId();
	while (nodePos != 0) {
		++myLevel;
		auto parentNodePos = m_tree.at(nodePos).parentPos();
		SSERIALIZE_ASSERT_SMALLER(parentNodePos, nodePos);
		SSERIALIZE_ASSERT_NOT_EQUAL(parentNodePos, TreeNode::npos);
		nodePos = parentNodePos;
	}
	return myLevel;
}

bool
GeoHierarchySpatialGrid::isAncestor(PixelId ancestor, PixelId decendant) const {
	if (!valid(CompoundPixel(ancestor)) || !valid(CompoundPixel(decendant))) {
		throw sserialize::OutOfBoundsException("Invalid pixel id");
	}
	auto ancestorPos = CompoundPixel(ancestor).treeId();
	auto decendantPos = CompoundPixel(decendant).treeId();
	while (ancestorPos < decendantPos && decendantPos != TreeNode::npos) {
		if (m_tree.at(decendantPos).parentPos() == ancestorPos) {
			return true;
		}
		decendantPos = m_tree.at(decendantPos).parentPos();
	}
	return false;
}

GeoHierarchySpatialGrid::PixelId
GeoHierarchySpatialGrid::index(double lat, double lon, Level level) const {
	throw sserialize::UnimplementedFunctionException("GeoHierarchySpatialGrid::tree");
	return 0;
}

GeoHierarchySpatialGrid::PixelId
GeoHierarchySpatialGrid::index(double lat, double lon) const {
	throw sserialize::UnimplementedFunctionException("GeoHierarchySpatialGrid::tree");
	return 0;
}

GeoHierarchySpatialGrid::PixelId
GeoHierarchySpatialGrid::index(PixelId parent, uint32_t childNumber) const {
	CompoundPixel cp(parent);
	if (!valid(cp)) {
		throw sserialize::OutOfBoundsException("Invalid pixel id");
	}
	auto const & parentNode = m_tree.at( cp.treeId() );
	if (parentNode.numberOfChildren() <= childNumber) {
		throw sserialize::OutOfBoundsException("Pixel does not have that many children");
	}
	return m_tree.at( parentNode.childrenBegin()+childNumber ).cp();
}

GeoHierarchySpatialGrid::PixelId
GeoHierarchySpatialGrid::parent(PixelId child) const {
	CompoundPixel cp(child);
	if (!valid(cp)) {
		throw sserialize::OutOfBoundsException("Invalid pixel id");
	}
	auto const & childNode = m_tree.at( cp.treeId() );
	if (childNode.parentPos() == TreeNode::npos) {
		throw sserialize::OutOfBoundsException("Pixel has no parent");
	}
	return m_tree.at( childNode.parentPos() ).cp();
}

GeoHierarchySpatialGrid::Size
GeoHierarchySpatialGrid::childrenCount(PixelId pixelId) const {
	CompoundPixel cp(pixelId);
	if (!valid(cp)) {
		throw sserialize::OutOfBoundsException("Invalid pixel id");
	}
	return m_tree.at( cp.treeId() ).numberOfChildren();
}

std::unique_ptr<hic::interface::SpatialGrid::TreeNode>
GeoHierarchySpatialGrid::tree(CellIterator begin, CellIterator end) const {
	throw sserialize::UnimplementedFunctionException("GeoHierarchySpatialGrid::tree");
	return std::unique_ptr<hic::interface::SpatialGrid::TreeNode>();
}

double
GeoHierarchySpatialGrid::area(PixelId pixel) const {
	return bbox(pixel).area() / (1000*1000);
}

sserialize::spatial::GeoRect
GeoHierarchySpatialGrid::bbox(PixelId pixel) const {
	CompoundPixel cp(pixel);
	if (!valid(cp)) {
		throw hic::exceptions::InvalidPixelId("Invalid pixel id");
	}
	if (cp.type() == CompoundPixel::REGION) {
		return m_gh.regionBoundary(cp.ghId());
	}
	else if (cp.type() == CompoundPixel::CELL || cp.type() == CompoundPixel::REGION_DUMMY_FOR_CELL) {
		return m_gh.cellBoundary(cp.ghId());
	}
	else {
		throw hic::exceptions::InvalidPixelId("Invalid pixel id");
	}
}

sserialize::RCPtrWrapper<GeoHierarchySpatialGrid>
GeoHierarchySpatialGrid::make(
	sserialize::Static::spatial::GeoHierarchy const & gh,
	sserialize::Static::ItemIndexStore const & idxStore,
	CostFunction const & costFn)
{
	sserialize::RCPtrWrapper<GeoHierarchySpatialGrid> result( new GeoHierarchySpatialGrid(gh, idxStore) );
	
	struct Worker {
		using Region = sserialize::Static::spatial::GeoHierarchy::Region;
		
		GeoHierarchySpatialGrid & that;
		CostFunction const & costFn;
		Worker(GeoHierarchySpatialGrid & that, CostFunction const & costFn) :
		that(that),
		costFn(costFn)
		{}
		TreeNode & node(std::size_t nodePos) {
			return that.m_tree.at(nodePos);
		}
		void operator()(std::size_t np) {
			if (node(np).isCell()) {
				return;
			}
			Region region = that.m_gh.region( node(np).regionId() );
			sserialize::ItemIndex coveredCells;
			sserialize::ItemIndex coverableCells;
			sserialize::ItemIndex uncoverableCells;
			std::vector<uint32_t> selectedRegions;
			std::set<uint32_t> selectableRegions;
			
			uncoverableCells = that.m_idxStore.at( region.exclusiveCellIndexPtr() );
			coverableCells = that.m_idxStore.at( region.cellIndexPtr() ) - uncoverableCells;
			
			for(uint32_t i(0), s(region.childrenSize()); i < s; ++i) {
				selectableRegions.insert(region.child(i));
			}
			
			while (coverableCells.size() && selectableRegions.size()) {
				uint32_t bestRegion = std::numeric_limits<uint32_t>::max();
				double bestCost = std::numeric_limits<double>::max();
				for(auto it(selectableRegions.begin()), end(selectableRegions.end()); it != end;) {
					auto currentRegion = that.m_gh.region(*it);
					auto regionCells = that.m_idxStore.at( currentRegion.cellIndexPtr() );
					auto cellsCoveredByRegion = regionCells - coveredCells;
					auto doubleCoveredCells = coveredCells / regionCells;
					if (!cellsCoveredByRegion.size() || doubleCoveredCells.size()) {
						it = selectableRegions.erase(it);
					}
					else {
						double c = costFn(region, regionCells, cellsCoveredByRegion, coveredCells, coverableCells);
						if (c < bestCost) {
							bestCost = c;
							bestRegion = *it;
						}
						++it;
					}
				}
				if (bestRegion == std::numeric_limits<uint32_t>::max()) {
					break;
				}
				auto regionCells = that.m_idxStore.at( that.m_gh.regionCellIdxPtr(bestRegion) );
				coveredCells += regionCells;
				coverableCells -= regionCells;
				selectedRegions.push_back(bestRegion);
				selectableRegions.erase(bestRegion);
			}
			
			if (coverableCells.size()) {
				uncoverableCells += coverableCells;
			}
			
			node(np).setChildrenBegin(that.m_tree.size());
			for(uint32_t rid : selectedRegions) {
				CompoundPixel cp(CompoundPixel::REGION, rid, that.m_tree.size());
				that.m_tree.emplace_back(cp, np);
			}
			for(uint32_t cid : uncoverableCells) {
				CompoundPixel cp(CompoundPixel::CELL, cid, that.m_tree.size());
				that.m_tree.emplace_back(cp, np);
				that.m_cellNodePos.at(cp.ghId()) = cp.treeId();
			}
			node(np).setChildrenEnd(that.m_tree.size());
		}
	};
	Worker w(*result, costFn);
	
	std::array<uint32_t, 2> nodeCounts = {0,0};
	
	result->m_tree.emplace_back(CompoundPixel(CompoundPixel::REGION, gh.rootRegion().ghId(), 0), TreeNode::npos);
	for(std::size_t i(0); i < result->m_tree.size(); ++i) {
		w(i);
		nodeCounts.at(result->m_tree.at(i).cp().type()) += 1;
	}
#ifndef NDEBUG
	std::cout << "GeoHierarchySpatialGrid: selected " << nodeCounts.at(CompoundPixel::REGION) << " out of " << gh.regionSize() << " regions" << std::endl;
#endif
	SSERIALIZE_CHEAP_ASSERT_EQUAL(result->m_cellNodePos.size(), gh.cellSize());
	{ //Introduce dummy nodes for cells not in the base level
		struct Depth {
			GeoHierarchySpatialGrid const & that;
			Depth(GeoHierarchySpatialGrid const & that) : that(that) {}
			uint32_t operator()(std::size_t nodePos) const {
				uint32_t depth = 0;
				auto const & node = that.m_tree.at(nodePos);
				for(uint32_t i(node.childrenBegin()), s(node.childrenEnd()); i < s; ++i) {
					depth = std::max(depth, (*this)(i)+1);
				}
				return depth;
			}
		};
		struct AddDummyNodes {
			GeoHierarchySpatialGrid & that;
			uint32_t targetDepth;
			AddDummyNodes(GeoHierarchySpatialGrid & that, uint32_t targetDepth) : that(that), targetDepth(targetDepth) {}
			auto & node(std::size_t nodePos) const {
				return that.m_tree.at(nodePos);
			}
			void operator()(std::size_t nodePos, uint32_t depth) {
				if (node(nodePos).cp().type() == CompoundPixel::CELL && depth < targetDepth) {
					node(nodePos).cp() = CompoundPixel(CompoundPixel::REGION_DUMMY_FOR_CELL, node(nodePos).cp().ghId(), node(nodePos).cp().treeId());
					node(nodePos).setChildrenBegin(that.m_tree.size());
					node(nodePos).setChildrenEnd(node(nodePos).childrenBegin()+1);
					std::size_t prev = nodePos;
					for(uint32_t i(0), s(targetDepth - depth - 1);  i < s; ++i) {
						CompoundPixel cp(CompoundPixel::REGION_DUMMY_FOR_CELL, node(nodePos).cp().ghId(), that.m_tree.size());
						that.m_tree.emplace_back(cp, prev);
						prev = cp.treeId();
					}
					CompoundPixel cp(CompoundPixel::CELL, node(nodePos).cp().ghId(), that.m_tree.size());
					that.m_cellNodePos.at(node(nodePos).cp().ghId()) = cp.treeId();
					that.m_tree.emplace_back(cp, prev);
				}
				else if (node(nodePos).cp().type() == CompoundPixel::REGION) {
					for(uint32_t i(node(nodePos).childrenBegin()), s(node(nodePos).childrenEnd()); i < s; ++i) {
						(*this)(i, depth+1);
					}
				}
			}
		};
		uint32_t depth = Depth(*result)(0);
		AddDummyNodes(*result, depth)(0, 0);
		struct CheckTargetDepth {
			GeoHierarchySpatialGrid const & that;
			uint32_t targetDepth;
			CheckTargetDepth(GeoHierarchySpatialGrid const & that, uint32_t targetDepth) : that(that), targetDepth(targetDepth) {}
			void operator()(std::size_t nodePos, uint32_t depth) const {
				auto & node = that.m_tree.at(nodePos);
				if (node.cp().type() == CompoundPixel::CELL) {
					SSERIALIZE_EXPENSIVE_ASSERT_EQUAL(depth, targetDepth);
					SSERIALIZE_EXPENSIVE_ASSERT_EQUAL(that.m_cellNodePos.at(node.cp().ghId()), nodePos);
				}
				else {
					for(uint32_t i(node.childrenBegin()), s(node.childrenEnd()); i < s; ++i) {
						(*this)(i, depth+1);
					}
				}
			}
		};
		CheckTargetDepth(*result, depth);
	}
	return result;
}

sserialize::Static::spatial::GeoHierarchy const &
GeoHierarchySpatialGrid::gh() const {
	return m_gh;
}

sserialize::Static::ItemIndexStore const &
GeoHierarchySpatialGrid::idxStore() const {
	return m_idxStore;
}

bool GeoHierarchySpatialGrid::valid(CompoundPixel const & cp) const {
	return cp.valid() && cp.hasFlag(CompoundPixel::HAS_TREE_ID);
}

bool
GeoHierarchySpatialGrid::isCell(PixelId pid) const {
	if (!valid(CompoundPixel(pid))) {
		throw hic::exceptions::InvalidPixelId("isCell");
	}
	return CompoundPixel(pid).type() == CompoundPixel::CELL;
}

bool
GeoHierarchySpatialGrid::isRegionDummy(PixelId pid) const {
	if (!valid(CompoundPixel(pid))) {
		throw hic::exceptions::InvalidPixelId("isCell");
	}
	return CompoundPixel(pid).type() == CompoundPixel::REGION_DUMMY_FOR_CELL;
}

bool
GeoHierarchySpatialGrid::isRegion(PixelId pid) const {
	if (!valid(CompoundPixel(pid))) {
		throw hic::exceptions::InvalidPixelId("isRegion");
	}
	return CompoundPixel(pid).type() == CompoundPixel::REGION;
}

GeoHierarchySpatialGrid::PixelId
GeoHierarchySpatialGrid::cellIdToPixelId(uint32_t cid) const {
	auto nodePos = m_cellNodePos.at(cid);
	auto const & cp = m_tree.at(nodePos).cp();
	SSERIALIZE_CHEAP_ASSERT_EQUAL(cp.type(), CompoundPixel::CELL);
	return cp;
}

uint32_t
GeoHierarchySpatialGrid::regionId(PixelId pid) const {
	CompoundPixel cp(pid);
	if (!valid(cp)) {
		throw hic::exceptions::InvalidPixelId("regionId");
	}
	if (cp.type() != CompoundPixel::REGION) {
		throw sserialize::TypeMissMatchException("Pixel is not a region");
	}
	return CompoundPixel(pid).ghId();
}

uint32_t
GeoHierarchySpatialGrid::cellId(PixelId pid) const {
	CompoundPixel cp(pid);
	if (!valid(cp)) {
		throw hic::exceptions::InvalidPixelId("cellId");
	}
	if (cp.type() != CompoundPixel::CELL && cp.type() != CompoundPixel::REGION_DUMMY_FOR_CELL) {
		throw sserialize::TypeMissMatchException("Pixel is not a cell");
	}
	return CompoundPixel(pid).ghId();
}

} //end namespace hic::impl
