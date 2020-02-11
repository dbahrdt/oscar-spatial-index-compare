#pragma once

#include <sserialize/storage/UByteArrayAdapter.h>
#include <sserialize/iterator/MultiBitBackInserter.h>
#include <sserialize/spatial/dgg/HCQRIndex.h>
#include <sserialize/spatial/dgg/HCQRSpatialGrid.h>
#include "static-htm-index.h"

namespace hic::Static {
namespace detail::HCQRTextIndex {
	
/**
* struct Payload {
*    u8 type;
*    hic::Static::detail::HCQRSpatialGrid::Tree trees[popcount(type)];
* };
* 
* 
* 
*/
class Payload final {
public:
	using Type = sserialize::UByteArrayAdapter;
	using QuerryType = sserialize::StringCompleter::QuerryType;
public:
	Payload();
	Payload(sserialize::UByteArrayAdapter const & d);
	~Payload();
public:
	QuerryType types() const;
	Type type(QuerryType qt) const;
	sserialize::UByteArrayAdapter typeData(QuerryType qt) const;
public:
	uint8_t m_types;
	sserialize::UByteArrayAdapter m_d;
};

/*
 *struct CompactNode {
 *  u1 type = {FULL, PARTIAL};
 *  u5 numPixelIdBits;
 *  u<numPixelIdBits> pixelId;
 *  u5 numItemIndexIdBits;
 *  u<numItemIndexIdBits> itemIndexId; //present if type==PARTIAL
 *};
 *
 */
class CompactNode {
public:
	static void create(sserialize::spatial::dgg::impl::HCQRSpatialGrid::TreeNode const & src, sserialize::MultiBitBackInserter & dest);
};

} //end namespace detail::HCQRTextIndex

/**
*  struct HCQRTextIndex: Version(1) {
*      uint<8> supportedQueries;
*      SpatialGridInfo sgInfo;
*      sserialize::Static::FlatTrieBase trie;
*      sserialize::Static::Array<detail::HCQRTextIndex::Payload> mixed;
*      sserialize::Static::Array<detail::HCQRTextIndex::Payload> regions;
*      sserialize::Static::Array<detail::HCQRTextIndex::Payload> items;
*  };
*/
class HCQRTextIndex: public sserialize::spatial::dgg::interface::HCQRIndex {
public:
    using Self = HCQRTextIndex;
	using SpatialGridInfo = hic::Static::SpatialGridInfo;
    using Payload = hic::Static::detail::HCQRTextIndex::Payload;
	using Trie = sserialize::Static::UnicodeTrie::FlatTrieBase;
	using Payloads = sserialize::Static::Array<Payload>;
	using HCQRPtr = sserialize::spatial::dgg::interface::HCQR::HCQRPtr;
public:
    struct MetaData {
        static constexpr uint8_t version{2};
    };
public:
	static sserialize::RCPtrWrapper<Self> make(const sserialize::UByteArrayAdapter & d, const sserialize::Static::ItemIndexStore & idxStore);
	~HCQRTextIndex() override;
public:
	sserialize::UByteArrayAdapter::SizeType getSizeInBytes() const;
	const sserialize::Static::ItemIndexStore & idxStore() const;
	int flags() const;
	std::ostream & printStats(std::ostream & out) const;
public:
	sserialize::StringCompleter::SupportedQuerries getSupportedQueries() const override;

	HCQRPtr complete(const std::string & qstr, const sserialize::StringCompleter::QuerryType qt) const override;
	HCQRPtr items(const std::string & qstr, const sserialize::StringCompleter::QuerryType qt) const override;
	HCQRPtr regions(const std::string & qstr, const sserialize::StringCompleter::QuerryType qt) const override;
public:
	HCQRPtr cell(uint32_t cellId) const override;
	HCQRPtr region(uint32_t regionId) const override;
public:
	SpatialGridInfo const & sgInfo() const;
	std::shared_ptr<SpatialGridInfo> const & sgInfoPtr() const;
	sserialize::spatial::dgg::interface::SpatialGrid const & sg() const override;
	sserialize::RCPtrWrapper<sserialize::spatial::dgg::interface::SpatialGrid> const & sgPtr() const;
	sserialize::spatial::dgg::interface::SpatialGridInfo const & sgi() const override;
	sserialize::RCPtrWrapper<sserialize::spatial::dgg::interface::SpatialGridInfo> const & sgiPtr() const;
private:
	HCQRTextIndex(const sserialize::UByteArrayAdapter & d, const sserialize::Static::ItemIndexStore & idxStore);
private:
	Payload::Type typeFromCompletion(const std::string& qs, sserialize::StringCompleter::QuerryType qt, Payloads const & pd) const;
private:
	char m_sq;
	std::shared_ptr<SpatialGridInfo> m_sgInfo;
	Trie m_trie;
	Payloads m_mixed;
	Payloads m_regions;
	Payloads m_items;
	sserialize::Static::ItemIndexStore m_idxStore;
	sserialize::RCPtrWrapper<sserialize::spatial::dgg::interface::SpatialGrid> m_sg;
	sserialize::RCPtrWrapper<sserialize::spatial::dgg::interface::SpatialGridInfo> m_sgi;
	int m_flags{ sserialize::CellQueryResult::FF_CELL_GLOBAL_ITEM_IDS };
};


}//end namespace
