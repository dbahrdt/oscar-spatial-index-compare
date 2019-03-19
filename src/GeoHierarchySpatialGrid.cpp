#include "GeoHierarchySpatialGrid.h"


namespace hic::impl {
	
//BEGIN GeoHierarchySpatialGrid::TreeNode
	
GeoHierarchySpatialGrid::TreeNode::TreeNode(PixelId pid, SizeType parent) :
m_pid(pid),
m_parentPos(parent),
m_childrenBegin(0),
m_childrenEnd(0)
{}

GeoHierarchySpatialGrid::PixelId
GeoHierarchySpatialGrid::TreeNode::pixelId() const {
	return m_pid;
}

bool
GeoHierarchySpatialGrid::TreeNode::isRegion() const {
	return GeoHierarchySpatialGrid::isRegion(pixelId());
}

bool
GeoHierarchySpatialGrid::TreeNode::isCell() const {
	return GeoHierarchySpatialGrid::isCell(pixelId());
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
m_gh(gh)
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

GeoHierarchySpatialGrid::Level
GeoHierarchySpatialGrid::level(PixelId pixelId) const {
	if (!m_pid2tn.count(pixelId)) {
		throw sserialize::OutOfBoundsException("Invalid pixel id");
	}
	std::size_t myLevel = 0;
	std::size_t nodePos = m_pid2tn.at(pixelId);
	while (nodePos != 0) {
		++myLevel;
		nodePos = m_tree.at(nodePos).parentPos();
		SSERIALIZE_ASSERT_NOT_EQUAL(nodePos, TreeNode::npos);
	}
	return myLevel;
}

bool
GeoHierarchySpatialGrid::isAncestor(PixelId ancestor, PixelId decendant) const {
	if (!m_pid2tn.count(ancestor) || !m_pid2tn.count(decendant)) {
		throw sserialize::OutOfBoundsException("Invalid pixel id");
	}
	auto ancestorPos = m_pid2tn.at(ancestor);
	auto decendantPos = m_pid2tn.at(decendant);
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
	if (!m_pid2tn.count(parent)) {
		throw sserialize::OutOfBoundsException("Invalid pixel id");
	}
	auto const & parentNode = m_tree.at( m_pid2tn.at(parent) );
	if (parentNode.numberOfChildren() <= childNumber) {
		throw sserialize::OutOfBoundsException("Pixel does not have that many children");
	}
	return m_tree.at( parentNode.childrenBegin()+childNumber ).pixelId();
}

GeoHierarchySpatialGrid::PixelId
GeoHierarchySpatialGrid::parent(PixelId child) const {
	if (!m_pid2tn.count(child)) {
		throw sserialize::OutOfBoundsException("Invalid pixel id");
	}
	auto const & childNode = m_tree.at( m_pid2tn.at(child) );
	if (childNode.parentPos() == TreeNode::npos) {
		throw sserialize::OutOfBoundsException("Pixel has no parent");
	}
	return m_tree.at( childNode.parentPos() ).pixelId();
}

GeoHierarchySpatialGrid::Size
GeoHierarchySpatialGrid::childrenCount(PixelId pixelId) const {
	if (!m_pid2tn.count(pixelId)) {
		throw sserialize::OutOfBoundsException("Invalid pixel id");
	}
	return m_tree.at( m_pid2tn.at(pixelId) ).numberOfChildren();
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
	if (!m_pid2tn.count(pixel)) {
		throw sserialize::OutOfBoundsException("Invalid pixel id");
	}
	return m_gh.regionBoundary(pixel >> 1);
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
		void operator()(std::size_t nodePos) {
			TreeNode & node = that.m_tree.at(nodePos);
			if (node.isCell()) {
				return;
			}
			Region region = that.m_gh.region( that.regionId(node.pixelId()) );
			sserialize::ItemIndex coveredCells;
			sserialize::ItemIndex coverableCells;
			sserialize::ItemIndex uncoverableCells;
			std::vector<uint32_t> selectedRegions;
			std::set<uint32_t> selectableRegions;
			
			uncoverableCells = that.m_idxStore.at( region.exclusiveCellIndexPtr() );
			coverableCells = that.m_idxStore.at( region.cellIndexPtr() ) - uncoverableCells;
			
			for(uint32_t i(region.childrenBegin()), s(region.childrenEnd()); i < s; ++i) {
				selectableRegions.insert(that.m_gh.region(i).ghId());
			}
			
			while (coverableCells.size() && selectableRegions.size()) {
				uint32_t bestRegion = std::numeric_limits<uint32_t>::max();
				double bestCost = std::numeric_limits<double>::max();
				for(auto it(selectableRegions.begin()), end(selectableRegions.end()); it != end;) {
					auto currentRegion = that.m_gh.region(*it);
					auto regionCells = that.m_idxStore.at( currentRegion.cellIndexPtr() );
					auto cellsCoveredByRegion = regionCells - coveredCells;
					if (!cellsCoveredByRegion.size()) {
						it = selectableRegions.erase(it);
					}
					else {
						double c = costFn(region, regionCells, cellsCoveredByRegion);
						if (c < bestCost) {
							bestCost = c;
							bestRegion = *it;
						}
						++it;
					}
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
			
			node.setChildrenBegin(that.m_tree.size());
			for(uint32_t rid : selectedRegions) {
				that.m_tree.emplace_back(that.regionIdToPixelId(rid), nodePos);
			}
			for(uint32_t cid : uncoverableCells) {
				that.m_tree.emplace_back(that.cellIdToPixelId(cid), nodePos);
			}
			node.setChildrenEnd(that.m_tree.size());
		}
	};
	Worker w(*result, costFn);
	
	result->m_tree.emplace_back(regionIdToPixelId(gh.rootRegion().ghId()), TreeNode::npos);
	for(std::size_t i(0); i < result->m_tree.size(); ++i) {
		w(i);
	}
	return result;
}

bool
GeoHierarchySpatialGrid::isCell(PixelId pid) {
	return pid & int(PixelType::CELL);
}

bool
GeoHierarchySpatialGrid::isRegion(PixelId pid) {
	return pid & int(PixelType::REGION);
}

GeoHierarchySpatialGrid::PixelId
GeoHierarchySpatialGrid::regionIdToPixelId(uint32_t rid) {
	return (PixelId(rid) << 1) | int(PixelType::REGION);
}

GeoHierarchySpatialGrid::PixelId
GeoHierarchySpatialGrid::cellIdToPixelId(uint32_t cid) {
	return (PixelId(cid) << 1) | int(PixelType::CELL);
}

uint32_t
GeoHierarchySpatialGrid::regionId(PixelId pid) {
	return pid >> 1;
}

uint32_t
GeoHierarchySpatialGrid::cellId(PixelId pid) {
	return pid >> 1;
}

} //end namespace hic::impl
