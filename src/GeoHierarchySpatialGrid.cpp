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
CompoundPixel(REGION, sserialize::Static::spatial::GeoHierarchy::npos)
{}

GeoHierarchySpatialGrid::CompoundPixel::CompoundPixel(Type type, uint32_t ghId) :
CompoundPixel(type, ghId, TreeNode::npos)
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

GeoHierarchySpatialGrid::CompoundPixel
GeoHierarchySpatialGrid::TreeNode::cp() const {
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
m_idxStore(idxStore)
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
	return regionIdToPixelId(m_gh.rootRegion().ghId());
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
		nodePos = m_tree.at(nodePos).parentPos();
		SSERIALIZE_ASSERT_NOT_EQUAL(nodePos, TreeNode::npos);
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
		throw sserialize::OutOfBoundsException("Invalid pixel id");
	}
	return m_gh.regionBoundary(cp.ghId());
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
			Region region = that.m_gh.region( node.regionId() );
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
					if (!cellsCoveredByRegion.size()) {
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
	
	result->m_tree.emplace_back(CompoundPixel(CompoundPixel::REGION, gh.rootRegion().ghId(), 0), TreeNode::npos);
	for(std::size_t i(0); i < result->m_tree.size(); ++i) {
		w(i);
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
GeoHierarchySpatialGrid::isCell(PixelId pid) {
	return CompoundPixel(pid).type() == CompoundPixel::CELL;
}

bool
GeoHierarchySpatialGrid::isRegion(PixelId pid) {
	return CompoundPixel(pid).type() == CompoundPixel::REGION;
}

GeoHierarchySpatialGrid::PixelId
GeoHierarchySpatialGrid::regionIdToPixelId(uint32_t rid) {
	return CompoundPixel(CompoundPixel::REGION, rid);
}

GeoHierarchySpatialGrid::PixelId
GeoHierarchySpatialGrid::cellIdToPixelId(uint32_t cid) {
	return CompoundPixel(CompoundPixel::CELL, cid);
}

uint32_t
GeoHierarchySpatialGrid::regionId(PixelId pid) {
	SSERIALIZE_CHEAP_ASSERT(isRegion(pid));
	return CompoundPixel(pid).ghId();
}

uint32_t
GeoHierarchySpatialGrid::cellId(PixelId pid) {
	SSERIALIZE_CHEAP_ASSERT(isCell(pid));
	return CompoundPixel(pid).ghId();
}

} //end namespace hic::impl
