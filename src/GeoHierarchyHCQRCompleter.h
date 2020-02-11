#pragma once

#include <liboscar/StaticOsmCompleter.h>

#include <sserialize/spatial/dgg/HCQR.h>
#include <sserialize/spatial/dgg/HCQRIndexFromCellIndex.h>
#include <sserialize/spatial/dgg/HCQRIndexWithCache.h>
#include <sserialize/spatial/dgg/GeoHierarchySpatialGrid.h>

namespace hic::detail::GeoHierarchyHCQRCompleter {

class SpatialGridInfo: public  sserialize::spatial::dgg::interface::SpatialGridInfo {
public:
	SpatialGridInfo(sserialize::RCPtrWrapper<sserialize::spatial::dgg::impl::GeoHierarchySpatialGrid> const & base);
	~SpatialGridInfo() override;
	SizeType itemCount(PixelId pid) const override;
	ItemIndex items(PixelId pid) const override;
	PixelId pixelId(CompressedPixelId const & cpid) const override;
private:
	sserialize::RCPtrWrapper<sserialize::spatial::dgg::impl::GeoHierarchySpatialGrid> m_base;
};

class CellIndex: public sserialize::spatial::dgg::HCQRIndexFromCellIndex::CellIndex {
public:
	using Parent = sserialize::spatial::dgg::HCQRIndexFromCellIndex::CellIndex;
    using Self = CellIndex;
public:
    CellIndex(liboscar::Static::OsmCompleter const & d);
    ~CellIndex() override;
public:
    sserialize::StringCompleter::SupportedQuerries getSupportedQueries() const override;
	CellQueryResult complete(const std::string & qstr, const sserialize::StringCompleter::QuerryType qt) const override;
	CellQueryResult items(const std::string & qstr, const sserialize::StringCompleter::QuerryType qt) const override;
	CellQueryResult regions(const std::string & qstr, const sserialize::StringCompleter::QuerryType qt) const override;
public:
	CellQueryResult cell(uint32_t cellId) const override;
	CellQueryResult region(uint32_t regionId) const override;
private:
	sserialize::Static::CellTextCompleter m_ctc;
};
	
} //end namespace hic::detail::GeoHierarchyHCQRCompleter

namespace hic {
	

sserialize::RCPtrWrapper<sserialize::spatial::dgg::interface::HCQRIndex>
makeGeoHierarchyHCQRIndex(liboscar::Static::OsmCompleter const & d);

}//end namespace hic
