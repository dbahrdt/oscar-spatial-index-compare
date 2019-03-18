#include "HCQRIndexWithCache.h"

namespace hic {

HCQRIndexWithCache::HCQRIndexWithCache(HCQRIndex & base) :
m_base(base)
{}

HCQRIndexWithCache::~HCQRIndexWithCache() {}

void
HCQRIndexWithCache::setCacheSize(uint32_t size) const {
    m_cache.setSize(size);
}

sserialize::StringCompleter::SupportedQuerries
HCQRIndexWithCache::getSupportedQueries() const {
    return m_base->getSupportedQueries();
}

HCQRIndexWithCache::HCQRPtr
HCQRIndexWithCache::complete(const std::string & qstr, const sserialize::StringCompleter::QuerryType qt) const {
    CacheKey ck(CacheKey::ITEMS_AND_REGIONS, qt, qstr);
    if (m_cache.count(ck)) {
        return m_cache.find(ck);
    }
    auto result = m_base->complete(qstr, qt);
    m_cache.insert(ck, result);
    return result;
}

HCQRIndexWithCache::HCQRPtr
HCQRIndexWithCache::items(const std::string & qstr, const sserialize::StringCompleter::QuerryType qt) const {
    CacheKey ck(CacheKey::ITEMS, qt, qstr);
    if (m_cache.count(ck)) {
        return m_cache.find(ck);
    }
    auto result = m_base->items(qstr, qt);
    m_cache.insert(ck, result);
    return result;
}

HCQRIndexWithCache::HCQRPtr
HCQRIndexWithCache::regions(const std::string & qstr, const sserialize::StringCompleter::QuerryType qt) const {
    CacheKey ck(CacheKey::REGIONS, qt, qstr);
    if (m_cache.count(ck)) {
        return m_cache.find(ck);
    }
    auto result = m_base->regions(qstr, qt);
    m_cache.insert(ck, result);
    return result;
}

HCQRIndexWithCache::SpatialGridInfo const &
HCQRIndexWithCache::sgi() const {
    return m_base->sgi();
}

HCQRIndexWithCache::SpatialGrid const &
HCQRIndexWithCache::sg() const {
    return m_base->sg();
}

}//end namespace hic