#pragma once

#include <sserialize/storage/UByteArrayAdapter.h>
#include <sserialize/Static/ItemIndexStore.h>
#include <sserialize/containers/CompactUintArray.h>
#include <sserialize/search/StringCompleter.h>
#include <sserialize/Static/CellTextCompleter.h>
#include <sserialize/Static/Map.h>
#include <liboscar/AdvancedOpTree.h>
#include <lsst/sphgeom/HtmPixelization.h>


namespace hic::Static {

/**
 *  struct HtmInfo: Version(1) {
 *      uint<8> htmLevels;
 *      sserialize::BoundedCompactUintArray trixelId2HtmIndexId;
 *      sserialize::Static::Map<uint64_t, uint32_t> htmIndexId2TrixelId; 
 *      sserialize::BoundedCompactUintArray trixelItemIndexIds;
 *  };
 *  
 *  struct OscarSearchHtmIndex: Version(1) {
 *      uint<8> supportedQueries;
 *      HtmInfo htmInfo;
 *      sserialize::Static::FlatTrie<sserialize::Static::CellTextCompleter::Payload> trie;
 *  };
 **/

class HtmInfo final {
public:
	using SizeType = uint32_t;
	using IndexId = uint32_t;
	using TrixelId = uint32_t;
	using HtmIndexId = uint64_t;
public:
    struct MetaData {
        static constexpr uint8_t version{1};
    };
public:
	HtmInfo(const sserialize::UByteArrayAdapter & d);
	~HtmInfo();
public:
	sserialize::UByteArrayAdapter::SizeType getSizeInBytes() const;
public:
	int levels() const;
	SizeType trixelCount() const;
public:
	IndexId trixelItemIndexId(TrixelId trixelId) const;
public:
	TrixelId trixelId(HtmIndexId htmIndex) const;
	HtmIndexId htmIndex(TrixelId trixelId) const;
private:
	int m_numLevels;
	sserialize::BoundedCompactUintArray m_trixelId2HtmIndexId;
	sserialize::Static::Map<HtmIndexId, TrixelId> m_htmIndexId2TrixelId;
	sserialize::BoundedCompactUintArray m_trixelItemIndexIds;
};

class OscarSearchHtmIndex: public sserialize::RefCountObject {
public:
    using Self = OscarSearchHtmIndex;
    using Payload = sserialize::Static::CellTextCompleter::Payload;
	using Trie = sserialize::UnicodeStringMap<Payload>;
	using FlatTrieType = sserialize::Static::UnicodeTrie::UnicodeStringMapFlatTrie<Payload>;
public:
    struct MetaData {
        static constexpr uint8_t version{1};
    };
public:
    static sserialize::RCPtrWrapper<Self> make(const sserialize::UByteArrayAdapter & d, const sserialize::Static::ItemIndexStore & idxStore);
    virtual ~OscarSearchHtmIndex() override;
    sserialize::UByteArrayAdapter::SizeType getSizeInBytes() const;
	const sserialize::Static::ItemIndexStore & idxStore() const;
    int flags() const;
	std::ostream & printStats(std::ostream & out) const;
public:
    sserialize::StringCompleter::SupportedQuerries getSupportedQueries() const;

    Payload::Type typeFromCompletion(const std::string& qs, const sserialize::StringCompleter::QuerryType qt) const;

	template<typename T_CQR_TYPE>
	T_CQR_TYPE complete(const std::string & qstr, const sserialize::StringCompleter::QuerryType qt) const;
public:
	inline HtmInfo const & htmInfo() const { return m_htmInfo; }
	inline lsst::sphgeom::HtmPixelization const & htm() const { return m_hp; }
private:
    OscarSearchHtmIndex(const sserialize::UByteArrayAdapter & d, const sserialize::Static::ItemIndexStore & idxStore);
private:
    char m_sq;
    char m_htmLevels;
    Trie m_trie;
	HtmInfo m_htmInfo;
    sserialize::Static::ItemIndexStore m_idxStore;
	lsst::sphgeom::HtmPixelization m_hp;
    int m_flags{ sserialize::CellQueryResult::FF_CELL_GLOBAL_ITEM_IDS };
};

namespace detail {

class OscarSearchHtmIndexCellInfo: public sserialize::interface::CQRCellInfoIface {
public:
	using RCType = sserialize::RCPtrWrapper<sserialize::interface::CQRCellInfoIface>;
    using IndexType = hic::Static::OscarSearchHtmIndex;
public:
	OscarSearchHtmIndexCellInfo(const sserialize::RCPtrWrapper<IndexType> & d);
	virtual ~OscarSearchHtmIndexCellInfo() override;
public:
	static RCType makeRc(const sserialize::RCPtrWrapper<IndexType> & d);
public:
	virtual SizeType cellSize() const override;
	virtual sserialize::spatial::GeoRect cellBoundary(CellId cellId) const override;
	virtual SizeType cellItemsCount(CellId cellId) const override;
	virtual IndexId cellItemsPtr(CellId cellId) const override;
private:
	sserialize::RCPtrWrapper<hic::Static::OscarSearchHtmIndex> m_d;
};

}//end namespace detail

class HtmOpTree: public liboscar::AdvancedOpTree {
public:
    HtmOpTree(sserialize::RCPtrWrapper<hic::Static::OscarSearchHtmIndex> const & d);
    virtual ~HtmOpTree() {}
public:
    sserialize::CellQueryResult calc();
private:
    class Calc final {
    public:
        using CQRType = sserialize::CellQueryResult;
    public:
        Calc(sserialize::RCPtrWrapper<hic::Static::OscarSearchHtmIndex> const & d) : m_d(d) {}
        ~Calc() {}
        CQRType calc(const Node * node);
    private:
        sserialize::RCPtrWrapper<hic::Static::OscarSearchHtmIndex> m_d;
    };
private:
    sserialize::RCPtrWrapper<hic::Static::OscarSearchHtmIndex> m_d;
};

}//end namespace hic::Static

//BEGIN Template function implementations
namespace hic::Static {

template<typename T_CQR_TYPE>
T_CQR_TYPE OscarSearchHtmIndex::complete(const std::string& qstr, const sserialize::StringCompleter::QuerryType qt) const {
	using CellInfo = hic::Static::detail::OscarSearchHtmIndexCellInfo;
    auto ci = CellInfo::makeRc( sserialize::RCPtrWrapper<Self>(const_cast<OscarSearchHtmIndex*>(this)) );
    try {
		Payload::Type t(typeFromCompletion(qstr, qt));
		return T_CQR_TYPE(idxStore().at( t.fmPtr() ), idxStore().at( t.pPtr() ), t.pItemsPtrBegin(), ci, idxStore(), flags());
	}
	catch (const sserialize::OutOfBoundsException & e) {
		return T_CQR_TYPE(ci, idxStore(), flags());
	}
}

}//end namespace hic::Static
//END template function implemenations
