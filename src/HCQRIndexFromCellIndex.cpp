#include "HCQRIndexFromCellIndex.h"

#include <sserialize/utility/debuggerfunctions.h>

namespace hic {
namespace detail::HCQRIndexFromCellIndex {
namespace impl {

SpatialGridInfoFromCellIndex::SpatialGridInfoFromCellIndex(SpatialGridPtr const & sg, CellInfoPtr const & ci) :
m_sg(sg),
m_ci(ci)
{}

SpatialGridInfoFromCellIndex::SizeType
SpatialGridInfoFromCellIndex::itemCount(PixelId pid) const {
    return items(pid).size();
}

SpatialGridInfoFromCellIndex::ItemIndex
SpatialGridInfoFromCellIndex::items(PixelId pid) const {
	std::unique_lock<std::mutex> cacheLck(m_cacheLock);
    if (m_cache.count(pid)) {
        return m_cache.find(pid);
    }
    cacheLck.unlock();
    struct Recurser {
        SpatialGridInfoFromCellIndex const & that;
        Recurser(SpatialGridInfoFromCellIndex const & that) : that(that) {}
        ItemIndex operator()(PixelId pid) {
            if (that.m_ci->hasPixel(pid)) {
                return that.m_ci->items(pid);
            }
            else if (that.m_sg->level(pid) <= that.m_ci->level()) {
                std::vector<ItemIndex> tmp;
                auto numChildren = that.m_sg->childrenCount(pid);
                for(decltype(numChildren) i(0); i < numChildren; ++i) {
                    tmp.emplace_back( (*this)( that.m_sg->index(pid, i) ) );
                }
                return ItemIndex::unite(tmp);
            }
            else {
                return ItemIndex();
            }
        }
    };
    Recurser rec(*this);
    ItemIndex result = rec(pid);
	cacheLck.lock();
    m_cache.insert(pid, result);
	cacheLck.unlock();
    return result;
}

SpatialGridInfoFromCellIndex::PixelId
SpatialGridInfoFromCellIndex::pixelId(CompressedPixelId const & cpid) const {
	return m_ci->pixelId(cpid);
}

}//end namespace impl

}//end namespace detail::HCQRIndexFromCellIndex

HCQRIndexFromCellIndex::HCQRIndexFromCellIndex(
    SpatialGridPtr const & sg,
    SpatialGridInfoPtr const & sgi,
    CellIndexPtr const & ci
) :
m_sg(sg),
m_sgi(sgi),
m_ci(ci)
{}

HCQRIndexFromCellIndex::~HCQRIndexFromCellIndex() {}

sserialize::StringCompleter::SupportedQuerries
HCQRIndexFromCellIndex::getSupportedQueries() const {
    return m_ci->getSupportedQueries();
}

HCQRIndexFromCellIndex::HCQRPtr
HCQRIndexFromCellIndex::complete(const std::string & qstr, const sserialize::StringCompleter::QuerryType qt) const {
    sserialize::CellQueryResult cqr = ci().complete(qstr, qt);
    HCQRPtr result( new MyHCQR(cqr, cqr.idxStore(), m_sg, m_sgi) );
	SSERIALIZE_EXPENSIVE_ASSERT_EQUAL(cqr.flaten(), result->items());
	return result;
}

HCQRIndexFromCellIndex::HCQRPtr
HCQRIndexFromCellIndex::items(const std::string & qstr, const sserialize::StringCompleter::QuerryType qt) const {
    sserialize::CellQueryResult cqr = ci().items(qstr, qt);
    HCQRPtr result( new MyHCQR(cqr, cqr.idxStore(), m_sg, m_sgi) );
	SSERIALIZE_EXPENSIVE_ASSERT_EQUAL(cqr.flaten(), result->items());
	return result;
}

HCQRIndexFromCellIndex::HCQRPtr
HCQRIndexFromCellIndex::regions(const std::string & qstr, const sserialize::StringCompleter::QuerryType qt) const {
    sserialize::CellQueryResult cqr = ci().regions(qstr, qt);
    HCQRPtr result( new MyHCQR(cqr, cqr.idxStore(), m_sg, m_sgi) );
	SSERIALIZE_EXPENSIVE_ASSERT_EQUAL(cqr.flaten(), result->items());
	return result;
}

HCQRIndexFromCellIndex::HCQRPtr
HCQRIndexFromCellIndex::cell(uint32_t cellId) const {
    sserialize::CellQueryResult cqr = ci().cell(cellId);
    HCQRPtr result( new MyHCQR(cqr, cqr.idxStore(), m_sg, m_sgi) );
	SSERIALIZE_EXPENSIVE_ASSERT_EQUAL(cqr.flaten(), result->items());
	return result;
	
}

HCQRIndexFromCellIndex::HCQRPtr
HCQRIndexFromCellIndex::region(uint32_t regionId) const {
    sserialize::CellQueryResult cqr = ci().region(regionId);
    HCQRPtr result( new MyHCQR(cqr, cqr.idxStore(), m_sg, m_sgi) );
	SSERIALIZE_EXPENSIVE_ASSERT_EQUAL(cqr.flaten(), result->items());
	return result;
}

HCQRIndexFromCellIndex::SpatialGridInfo const &
HCQRIndexFromCellIndex::sgi() const {
    return *m_sgi;
}

HCQRIndexFromCellIndex::SpatialGrid const &
HCQRIndexFromCellIndex::sg() const {
    return *m_sg;
}

HCQRIndexFromCellIndex::CellIndex const &
HCQRIndexFromCellIndex::ci() const {
    return *m_ci;
}

}//end namespace
