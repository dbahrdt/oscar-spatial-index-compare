#include "GeoHierarchyHCQRCompleter.h"

#include "HcqrOpTree.h"

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
			m_base->idxStore().at( m_base->gh().regionItemsPtr( m_base->regionId(pid) ) );
		}
		else {
			sserialize::ItemIndex regionCells;
			std::vector<sserialize::ItemIndex> tmp;
			for(uint32_t cellId : regionCells) {
				tmp.emplace_back( m_base->idxStore().at( m_base->gh().cellItemsPtr(cellId) ) );
			}
			return sserialize::ItemIndex::unite(tmp);
		}
	}
	else {
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
	
} //end namespace hic::detail::GeoHierarchyHCQRCompleter

namespace hic {
	
GeoHierarchyHCQRCompleter::GeoHierarchyHCQRCompleter(liboscar::Static::OsmCompleter const & d) {
	using HCQRIndexImp = hic::HCQRIndexFromCellIndex;
	using MySpatialGrid = hic::impl::GeoHierarchySpatialGrid;

	MySpatialGrid::PenalizeDoubleCoverCostFunction costfn(10);
	auto sg = MySpatialGrid::make(d.store().geoHierarchy(), d.indexStore(), costfn);
	HCQRIndexImp::SpatialGridInfoPtr sgi( new hic::detail::GeoHierarchyHCQRCompleter::SpatialGridInfo(sg) );
	HCQRIndexImp::CellIndexPtr ci( new hic::detail::GeoHierarchyHCQRCompleter::CellIndex(d) );

	sserialize::RCPtrWrapper<HCQRIndexImp> uncachedIndex( new HCQRIndexImp(sg, sgi, ci) );
	m_d.reset( new HCQRIndexWithCache(uncachedIndex) );
}

GeoHierarchyHCQRCompleter::~GeoHierarchyHCQRCompleter() {}

sserialize::RCPtrWrapper<hic::interface::HCQR>
GeoHierarchyHCQRCompleter::complete(std::string const & str) {
	hic::HcqrOpTree opTree(m_d);
	opTree.parse(str);
	return opTree.calc();
}

void
GeoHierarchyHCQRCompleter::setCacheSize(uint32_t size) {
	static_cast<HCQRIndexWithCache&>(*m_d.get()).setCacheSize(size);
}
	
}//end namespace hic
