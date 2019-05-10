#pragma once

#include <sserialize/spatial/CellQueryResult.h>
#include <sserialize/containers/RandomCache.h>

#include "SpatialGrid.h"
#include "HCQRIndex.h"


namespace hic {
namespace detail::HCQRIndexFromCellIndex::interface {

class CellIndex: public sserialize::RefCountObject {
public:
    using Self = CellIndex;
    using CellQueryResult = sserialize::CellQueryResult;
public:
    CellIndex() {}
    virtual ~CellIndex() {}
public:
    virtual sserialize::StringCompleter::SupportedQuerries getSupportedQueries() const = 0;
	virtual CellQueryResult complete(const std::string & qstr, const sserialize::StringCompleter::QuerryType qt) const = 0;
	virtual CellQueryResult items(const std::string & qstr, const sserialize::StringCompleter::QuerryType qt) const = 0;
	virtual CellQueryResult regions(const std::string & qstr, const sserialize::StringCompleter::QuerryType qt) const = 0;
public:
	virtual CellQueryResult cell(uint32_t cellId) const = 0;
	virtual CellQueryResult region(uint32_t regionId) const = 0;
};

class CellInfo: public sserialize::RefCountObject {
public:
    using SpatialGrid = hic::interface::SpatialGrid;
    using PixelId = SpatialGrid::PixelId;
    using CompressedPixelId = SpatialGrid::CompressedPixelId;
    using ItemIndex = sserialize::ItemIndex;
public:
    CellInfo() {}
    virtual ~CellInfo() {}
public:
    virtual SpatialGrid::Level level() const = 0;
public:
    virtual bool hasPixel(PixelId pid) const = 0;
	///should throw iff !hasPixel(pid)
    virtual ItemIndex items(PixelId pid) const = 0;
	virtual PixelId pixelId(CompressedPixelId const & cpid) const = 0;
public:
	virtual std::vector<CompressedPixelId> cells() const = 0;
};

}//end namespace detail::HCQRIndexFromCellIndex::interface

namespace detail::HCQRIndexFromCellIndex::impl {

class SpatialGridInfoFromCellIndex: public hic::interface::SpatialGridInfo {
public:
    using SizeType = uint32_t;
    using ItemIndex = sserialize::ItemIndex;

    using CellInfo = hic::detail::HCQRIndexFromCellIndex::interface::CellInfo;
    using SpatialGrid = hic::interface::SpatialGrid;

    using CellInfoPtr = sserialize::RCPtrWrapper<CellInfo>;
    using SpatialGridPtr = sserialize::RCPtrWrapper<SpatialGrid>;
public:
    SpatialGridInfoFromCellIndex(SpatialGridPtr const & sg, CellInfoPtr const & ci);
    SizeType itemCount(PixelId pid) const override;
    ItemIndex items(PixelId pid) const override;
	PixelId pixelId(CompressedPixelId const & cpid) const override;
private:
    using PixelItemsCache = sserialize::LFUCache<PixelId, sserialize::ItemIndex>;
private:
    SpatialGridPtr m_sg;
    CellInfoPtr m_ci;
	mutable std::mutex m_cacheLock;
    mutable PixelItemsCache m_cache;
};

}//end namespace detail::HCQRIndexFromCellIndex::impl

class HCQRIndexFromCellIndex: public hic::interface::HCQRIndex {
public:
    using ItemIndexStore = sserialize::Static::ItemIndexStore;

    using CellIndex = hic::detail::HCQRIndexFromCellIndex::interface::CellIndex;
    using CellIndexPtr = sserialize::RCPtrWrapper<CellIndex>;

    using SpatialGridPtr = sserialize::RCPtrWrapper<SpatialGrid>;
    using SpatialGridInfoPtr = sserialize::RCPtrWrapper<SpatialGridInfo>;

    using MyHCQR = hic::impl::HCQRSpatialGrid;

    using PixelId = SpatialGrid::PixelId;
public:
    HCQRIndexFromCellIndex(
        SpatialGridPtr const & sg,
        SpatialGridInfoPtr const & sgi,
        CellIndexPtr const & ci
    );
    virtual ~HCQRIndexFromCellIndex();
public:
    sserialize::StringCompleter::SupportedQuerries getSupportedQueries() const override;
	HCQRPtr complete(const std::string & qstr, const sserialize::StringCompleter::QuerryType qt) const override;
	HCQRPtr items(const std::string & qstr, const sserialize::StringCompleter::QuerryType qt) const override;
	HCQRPtr regions(const std::string & qstr, const sserialize::StringCompleter::QuerryType qt) const override;
public:
	HCQRPtr cell(uint32_t cellId) const override;
	HCQRPtr region(uint32_t regionId) const override;
public:
	SpatialGridInfo const & sgi() const override;
	SpatialGrid const & sg() const override;
public:
    CellIndex const & ci() const;
    ItemIndexStore const & idxStore() const;
private:
    SpatialGridPtr m_sg;
    SpatialGridInfoPtr m_sgi;
    CellIndexPtr m_ci;
};

}//end namespace hic

