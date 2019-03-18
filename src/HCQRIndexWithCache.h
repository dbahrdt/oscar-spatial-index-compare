#pragma once

#include <sserialize/containers/LFUCache.h>

#include "HCQRIndex.h"

namespace hic {
namespace detail::HCQRIndexWithCache {

struct CacheKey {
    enum {ITEMS_AND_REGIONS, ITEMS, REGIONS} ItemType;
    CacheKey(uint8_t itemType, uint8_t qt, std::string const & qstr) :
    itemType(itemType), qt(qt), qstr(qstr)
    {}
    CacheKey(CacheKey const &) = default;
    CacheKey(CacheKey &&) = default;
    uint8_t itemType;
    uint8_t qt;
    std::string qstr;
}; 

} //end namespace detail::HCQRIndexWithCache

class HCQRIndexWithCache: hic::interface::HCQRIndex {
public:
    using HCQRIndexPtr = sserialize::RCPtrWrapper<hic::interface::HCQRIndex>;
public:
    HCQRIndexWithCache(HCQRIndex & base);
    ~HCQRIndexWithCache() override;
public:
    void setCacheSize(uint32_t size) const;
public:
    sserialize::StringCompleter::SupportedQuerries getSupportedQueries() const override;
public:
	HCQRPtr complete(const std::string & qstr, const sserialize::StringCompleter::QuerryType qt) const override;
	HCQRPtr items(const std::string & qstr, const sserialize::StringCompleter::QuerryType qt) const override;
	HCQRPtr regions(const std::string & qstr, const sserialize::StringCompleter::QuerryType qt) const override;
public:
	SpatialGridInfo const & sgi() const override;
	SpatialGrid const & sg() const override;
private:
    using CacheKey = hic::detail::HCQRIndexWithCache::CacheKey;
    using Cache = sserialize::LFUCache<CacheKey, HCQRPtr>;
private:
    HCQRIndexPtr m_base;
    Cache m_cache;
};

}//end namespace hic