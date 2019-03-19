#pragma once

#include <liboscar/StaticOsmCompleter.h>

#include "HCQR.h"
#include "HCQRIndexFromCellIndex.h"
#include "HCQRIndexWithCache.h"
#include "GeoHierarchySpatialGrid.h"

namespace hic::detail::GeoHierarchyHCQRCompleter {

class SpatialGridInfo: public hic::interface::SpatialGridInfo {
public:
	SpatialGridInfo(sserialize::RCPtrWrapper<hic::impl::GeoHierarchySpatialGrid> const & base);
	~SpatialGridInfo() override;
	SizeType itemCount(PixelId pid) const override;
	ItemIndex items(PixelId pid) const override;
	PixelId pixelId(CompressedPixelId const & cpid) const override;
private:
	sserialize::RCPtrWrapper<hic::impl::GeoHierarchySpatialGrid> m_base;
};

class CellIndex: public hic::HCQRIndexFromCellIndex::CellIndex {
public:
	using Parent = hic::HCQRIndexFromCellIndex::CellIndex;
    using Self = CellIndex;
public:
    CellIndex(liboscar::Static::OsmCompleter const & d);
    ~CellIndex() override;
public:
    sserialize::StringCompleter::SupportedQuerries getSupportedQueries() const override;
	CellQueryResult complete(const std::string & qstr, const sserialize::StringCompleter::QuerryType qt) const override;
	CellQueryResult items(const std::string & qstr, const sserialize::StringCompleter::QuerryType qt) const override;
	CellQueryResult regions(const std::string & qstr, const sserialize::StringCompleter::QuerryType qt) const override;
private:
	sserialize::Static::CellTextCompleter m_ctc;
};
	
} //end namespace hic::detail::GeoHierarchyHCQRCompleter

namespace hic {
	
class GeoHierarchyHCQRCompleter {
public:
	GeoHierarchyHCQRCompleter(liboscar::Static::OsmCompleter const & d);
	~GeoHierarchyHCQRCompleter();
public:
	sserialize::RCPtrWrapper<hic::interface::HCQR> complete(std::string const & str);
public:
	void setCacheSize(uint32_t size);
private:
	using HCQRIndex = hic::interface::HCQRIndex;
	using HCQRIndexPtr = sserialize::RCPtrWrapper<HCQRIndex>;
private:
	HCQRIndexPtr m_d;
};
	
}//end namespace hic
