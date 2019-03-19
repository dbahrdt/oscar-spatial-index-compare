#include "GeoHierarchySpatialGrid.h"


namespace hic::impl {

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
	CostFunction & costFn)
{
	sserialize::RCPtrWrapper<GeoHierarchySpatialGrid> result( new GeoHierarchySpatialGrid(gh, idxStore) );
	
	struct Worker {
		GeoHierarchySpatialGrid & that;
		CostFunction & costFn;
		Worker(GeoHierarchySpatialGrid & that, CostFunction & costFn) :
		that(that),
		costFn(costFn)
		{}
		void operator()(uint32_t region) {
			std::vector<uint32_t> coveredCells;
			std::vector<uint32_t> uncoveredCells;
			std::vector<uint32_t> selectedRegions;
			
		}
	};
	Worker w(*result, costFn);
	w(gh.rootRegion().ghId());
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
	
	
} //end namespace hic::impl
