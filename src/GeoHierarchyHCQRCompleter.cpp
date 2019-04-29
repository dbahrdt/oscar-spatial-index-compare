#include "GeoHierarchyHCQRCompleter.h"

namespace hic::detail::GeoHierarchyHCQRCompleter {

SpatialGridInfo::SpatialGridInfo(sserialize::RCPtrWrapper<hic::impl::GeoHierarchySpatialGrid> const & base) :
m_base(base)
{}

SpatialGridInfo::~SpatialGridInfo() {}

SpatialGridInfo::SizeType
SpatialGridInfo::itemCount(PixelId pid) const {
	return items(pid).size();
}

SpatialGridInfo::ItemIndex
SpatialGridInfo::items(PixelId pid) const {
	if (m_base->isRegion(pid)) {
		if (m_base->gh().hasRegionItems()) {
			return m_base->idxStore().at( m_base->gh().regionItemsPtr( m_base->regionId(pid) ) );
		}
		else {
			sserialize::ItemIndex regionCells = m_base->idxStore().at( m_base->gh().regionCellIdxPtr(m_base->regionId(pid)) );
			std::vector<sserialize::ItemIndex> tmp;
			for(uint32_t cellId : regionCells) {
				tmp.emplace_back( m_base->idxStore().at( m_base->gh().cellItemsPtr(cellId) ) );
			}
			return sserialize::ItemIndex::unite(tmp);
		}
	}
	else { //cell or dummy region
		return m_base->idxStore().at( m_base->gh().cellItemsPtr(m_base->cellId(pid)) );
	}
}

SpatialGridInfo::PixelId
SpatialGridInfo::pixelId(CompressedPixelId const & cpid) const {
	return m_base->cellIdToPixelId(cpid.value());
}

CellIndex::CellIndex(liboscar::Static::OsmCompleter const & d) {
	if (!d.textSearch().hasSearch(liboscar::TextSearch::GEOCELL)) {
		throw sserialize::MissingDataException("OsmCompleter has not CellTextCompleter");
	}
	m_ctc = d.textSearch().get<liboscar::TextSearch::GEOCELL>();
}

CellIndex::~CellIndex() {}

sserialize::StringCompleter::SupportedQuerries
CellIndex::getSupportedQueries() const {
	return m_ctc.getSupportedQuerries();
}

CellIndex::CellQueryResult
CellIndex::complete(const std::string & qstr, const sserialize::StringCompleter::QuerryType qt) const {
	return m_ctc.complete(qstr, qt);
}

CellIndex::CellQueryResult
CellIndex::items(const std::string & qstr, const sserialize::StringCompleter::QuerryType qt) const {
	return m_ctc.items(qstr, qt);
}

CellIndex::CellQueryResult
CellIndex::regions(const std::string & qstr, const sserialize::StringCompleter::QuerryType qt) const {
	return m_ctc.regions(qstr, qt);
}

CellIndex::CellQueryResult
CellIndex::cell(uint32_t cellId) const {
	return m_ctc.cqrFromCellId(cellId);
}

CellIndex::CellQueryResult
CellIndex::region(uint32_t regionId) const {
	return m_ctc.cqrFromRegionStoreId(regionId);
}
	
} //end namespace hic::detail::GeoHierarchyHCQRCompleter

namespace hic {
	
template<typename T_BASE>
struct PreferAdminLevelCostFunction: public hic::impl::GeoHierarchySpatialGrid::CostFunction {
	PreferAdminLevelCostFunction(PreferAdminLevelCostFunction const &) = default;
	PreferAdminLevelCostFunction(T_BASE const & base, liboscar::Static::OsmCompleter const & cmp) : m_base(base), m_cmp(cmp) {}
	~PreferAdminLevelCostFunction() override {}
	double operator()(
		sserialize::Static::spatial::GeoHierarchy::Region const & region,
		sserialize::ItemIndex const & regionCells,
		sserialize::ItemIndex const & cellsCoveredByRegion,
		sserialize::ItemIndex const & coveredCells,
		sserialize::ItemIndex const & coverableCells
	) const override {
		auto baseCost = m_base(region, regionCells, cellsCoveredByRegion, coveredCells, coverableCells);
		if (m_cmp.store().kvItem(region.storeId()).countKey("admin:level")) {
			return baseCost;
		}
		else {
			return baseCost*100;
		}
	}
	T_BASE m_base;
	liboscar::Static::OsmCompleter const & m_cmp;
};

sserialize::RCPtrWrapper<hic::interface::HCQRIndex>
makeGeoHierarchyHCQRIndex(liboscar::Static::OsmCompleter const & d) {
	using HCQRIndexImp = hic::HCQRIndexFromCellIndex;
	using MySpatialGrid = hic::impl::GeoHierarchySpatialGrid;
	MySpatialGrid::SimpleCostFunction baseCostFn;
	MySpatialGrid::PreferLargeCostFunction<decltype(baseCostFn)> preferLargeCFn(baseCostFn);
	PreferAdminLevelCostFunction<decltype(preferLargeCFn)> preferAdminLevleCFn(preferLargeCFn, d);
	auto sg = MySpatialGrid::make(d.store().geoHierarchy(), d.indexStore(), preferAdminLevleCFn);
	HCQRIndexImp::SpatialGridInfoPtr sgi( new hic::detail::GeoHierarchyHCQRCompleter::SpatialGridInfo(sg) );
	HCQRIndexImp::CellIndexPtr ci( new hic::detail::GeoHierarchyHCQRCompleter::CellIndex(d) );

	return sserialize::RCPtrWrapper<HCQRIndexImp>( new HCQRIndexImp(sg, sgi, ci) );
}

}//end namespace hic
