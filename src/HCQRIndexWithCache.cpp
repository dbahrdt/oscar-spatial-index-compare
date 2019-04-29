#include "HCQRIndexWithCache.h"

namespace hic {

HCQRIndexWithCache::HCQRIndexWithCache(HCQRIndexPtr const & base, uint32_t cacheSize) :
m_base(base)
{
	setCacheSize(cacheSize);
}

HCQRIndexWithCache::~HCQRIndexWithCache() {}

HCQRIndexWithCache::HCQRIndexPtr
HCQRIndexWithCache::make(HCQRIndexPtr const & base, uint32_t cacheSize) {
	return HCQRIndexPtr( new HCQRIndexWithCache(base, cacheSize) );
}

void
HCQRIndexWithCache::setCacheSize(uint32_t size) {
    std::lock_guard<std::mutex> lck(m_cacheLock);
    m_cache.setSize(size);
}

sserialize::StringCompleter::SupportedQuerries
HCQRIndexWithCache::getSupportedQueries() const {
    return m_base->getSupportedQueries();
}

HCQRIndexWithCache::HCQRPtr
HCQRIndexWithCache::complete(const std::string & qstr, const sserialize::StringCompleter::QuerryType qt) const {
    std::lock_guard<std::mutex> lck(m_cacheLock);
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
    std::lock_guard<std::mutex> lck(m_cacheLock);
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
    std::lock_guard<std::mutex> lck(m_cacheLock);
    CacheKey ck(CacheKey::REGIONS, qt, qstr);
    if (m_cache.count(ck)) {
        return m_cache.find(ck);
    }
    auto result = m_base->regions(qstr, qt);
    m_cache.insert(ck, result);
    return result;
}


HCQRIndexWithCache::HCQRPtr
HCQRIndexWithCache::cell(uint32_t cellId) const {
    CacheKey ck(CacheKey::CELL, 0, std::to_string(cellId));
    std::lock_guard<std::mutex> lck(m_cacheLock);
    if (m_cache.count(ck)) {
        return m_cache.find(ck);
    }
    auto result = m_base->cell(cellId);
    m_cache.insert(ck, result);
    return result;
}

HCQRIndexWithCache::HCQRPtr
HCQRIndexWithCache::region(uint32_t regionId) const {
    CacheKey ck(CacheKey::REGION, 0, std::to_string(regionId));
    std::lock_guard<std::mutex> lck(m_cacheLock);
    if (m_cache.count(ck)) {
        return m_cache.find(ck);
    }
    auto result = m_base->region(regionId);
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
