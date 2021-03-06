#pragma once

#include <sserialize/utility/exceptions.h>
#include <sserialize/storage/UByteArrayAdapter.h>
#include <sserialize/Static/ItemIndexStore.h>
#include <sserialize/containers/CompactUintArray.h>
#include <sserialize/search/StringCompleter.h>
#include <sserialize/Static/CellTextCompleter.h>
#include <sserialize/Static/UnicodeTrie/FlatTrie.h>
#include <sserialize/Static/Map.h>
#include <sserialize/spatial/dgg/SpatialGrid.h>
#include <sserialize/spatial/dgg/HCQRIndexFromCellIndex.h>
#include <sserialize/spatial/dgg/Static/SpatialGridInfo.h>

#include <liboscar/AdvancedOpTree.h>


namespace hic {
	class OscarSearchSgIndex;
}

namespace hic::Static {
	class OscarSearchHCQRTextIndexCreator;
}

namespace hic::Static {

/**
 *  struct OscarSearchSgIndex: Version(2) {
 *      uint<8> supportedQueries;
 *      SpatialGridInfo htmInfo;
 *      sserialize::Static::FlatTrieBase trie;
 *      sserialize::Static::Array<sserialize::Static::CellTextCompleter::Payload> mixed;
 *      sserialize::Static::Array<sserialize::Static::CellTextCompleter::Payload> regions;
 *      sserialize::Static::Array<sserialize::Static::CellTextCompleter::Payload> items;
 *  };
 **/

class OscarSearchSgIndex: public sserialize::RefCountObject {
public:
    using Self = OscarSearchSgIndex;
    using Payload = sserialize::Static::CellTextCompleter::Payload;
	using Trie = sserialize::Static::UnicodeTrie::FlatTrieBase;
	using Payloads = sserialize::Static::Array<Payload>;
	using SpatialGridInfo = sserialize::spatial::dgg::Static::SpatialGridInfo;
public:
    struct MetaData {
        static constexpr uint8_t version{2};
    };
public:
    static sserialize::RCPtrWrapper<Self> make(const sserialize::UByteArrayAdapter & d, const sserialize::Static::ItemIndexStore & idxStore);
    virtual ~OscarSearchSgIndex() override;
    sserialize::UByteArrayAdapter::SizeType getSizeInBytes() const;
	const sserialize::Static::ItemIndexStore & idxStore() const;
    int flags() const;
	std::ostream & printStats(std::ostream & out) const;
public:
    sserialize::StringCompleter::SupportedQuerries getSupportedQueries() const;

	template<typename T_CQR_TYPE>
	T_CQR_TYPE complete(const std::string & qstr, const sserialize::StringCompleter::QuerryType qt) const;
	template<typename T_CQR_TYPE>
	T_CQR_TYPE items(const std::string & qstr, const sserialize::StringCompleter::QuerryType qt) const;
	template<typename T_CQR_TYPE>
	T_CQR_TYPE regions(const std::string & qstr, const sserialize::StringCompleter::QuerryType qt) const;
public:
	template<typename T_CQR_TYPE>
	T_CQR_TYPE cell(uint32_t cellId) const;
	template<typename T_CQR_TYPE>
	T_CQR_TYPE region(uint32_t regionId) const;
public:
	inline SpatialGridInfo const & sgInfo() const { return *m_sgInfo; }
	inline std::shared_ptr<SpatialGridInfo> const & sgInfoPtr() const { return m_sgInfo; }
	inline sserialize::spatial::dgg::interface::SpatialGrid const & sg() const { return *m_sg; }
	inline sserialize::RCPtrWrapper<sserialize::spatial::dgg::interface::SpatialGrid> const & sgPtr() const { return m_sg; }
	inline Trie const & trie() const { return m_trie; }
private:
	friend class OscarSearchHCQRTextIndexCreator;
private:
    OscarSearchSgIndex(const sserialize::UByteArrayAdapter & d, const sserialize::Static::ItemIndexStore & idxStore);
private:
    Payload::Type typeFromCompletion(const std::string& qs, const sserialize::StringCompleter::QuerryType qt, Payloads const & pd) const;
private:
    char m_sq;
	std::shared_ptr<SpatialGridInfo> m_sgInfo;
    Trie m_trie;
	Payloads m_mixed;
	Payloads m_regions;
	Payloads m_items;
    sserialize::Static::ItemIndexStore m_idxStore;
	sserialize::RCPtrWrapper<sserialize::spatial::dgg::interface::SpatialGrid> m_sg;
    int m_flags{ sserialize::CellQueryResult::FF_CELL_GLOBAL_ITEM_IDS };
};

class HCQROscarCellIndex: public sserialize::spatial::dgg::detail::HCQRIndexFromCellIndex::interface::CellIndex {
public:
	HCQROscarCellIndex(sserialize::RCPtrWrapper<OscarSearchSgIndex> const & base);
    ~HCQROscarCellIndex() override;
public:
    sserialize::StringCompleter::SupportedQuerries getSupportedQueries() const override;
	CellQueryResult complete(const std::string & qstr, const sserialize::StringCompleter::QuerryType qt) const override;
	CellQueryResult items(const std::string & qstr, const sserialize::StringCompleter::QuerryType qt) const override;
	CellQueryResult regions(const std::string & qstr, const sserialize::StringCompleter::QuerryType qt) const override;
public:
	CellQueryResult cell(uint32_t cellId) const override;
	CellQueryResult region(uint32_t regionId) const override;
private:
	sserialize::RCPtrWrapper<OscarSearchSgIndex> m_base;
};

class OscarSearchHCQRTextIndexCreator {
public:
	sserialize::UByteArrayAdapter dest;
	sserialize::ItemIndexFactory idxFactory; //empty
	sserialize::Static::ItemIndexStore idxStore; //idx store of the source
	sserialize::UByteArrayAdapter src;
	bool compactify{false};
	bool compactTree{false};
	uint32_t compactLevel{std::numeric_limits<uint32_t>::max()};
	uint32_t threads{0};
public:
	void run();
};

namespace detail {

class OscarSearchSgIndexCellInfo: public sserialize::interface::CQRCellInfoIface {
public:
	using RCType = sserialize::RCPtrWrapper<sserialize::interface::CQRCellInfoIface>;
    using IndexType = hic::Static::OscarSearchSgIndex;
public:
	OscarSearchSgIndexCellInfo(const sserialize::RCPtrWrapper<IndexType> & d);
	virtual ~OscarSearchSgIndexCellInfo() override;
public:
	static RCType makeRc(const sserialize::RCPtrWrapper<IndexType> & d);
public:
	virtual SizeType cellSize() const override;
	virtual sserialize::spatial::GeoRect cellBoundary(CellId cellId) const override;
	virtual SizeType cellItemsCount(CellId cellId) const override;
	virtual IndexId cellItemsPtr(CellId cellId) const override;
private:
	sserialize::RCPtrWrapper<hic::Static::OscarSearchSgIndex> m_d;
};

}//end namespace detail

class SgOpTree: public liboscar::AdvancedOpTree {
public:
    SgOpTree(sserialize::RCPtrWrapper<hic::Static::OscarSearchSgIndex> const & d);
    virtual ~SgOpTree() {}
public:
	template<typename TCQRType>
    TCQRType calc() {
		return Calc<TCQRType>(m_d).calc(root());
	}
private:
	template<typename TCQRType>
    class Calc final {
    public:
        using CQRType = TCQRType;
    public:
        Calc(sserialize::RCPtrWrapper<hic::Static::OscarSearchSgIndex> const & d) : m_d(d) {}
        ~Calc() {}
        CQRType calc(const Node * node);
    private:
        sserialize::RCPtrWrapper<hic::Static::OscarSearchSgIndex> m_d;
    };
private:
    sserialize::RCPtrWrapper<hic::Static::OscarSearchSgIndex> m_d;
};

class OscarSearchSgCompleter {
public:
	OscarSearchSgCompleter() {}
	~OscarSearchSgCompleter() {}
public:
	void energize(std::string const & files);
public:
	inline hic::Static::OscarSearchSgIndex const & index() const { return *m_d; }
	sserialize::RCPtrWrapper<hic::Static::OscarSearchSgIndex> const & indexPtr() const { return m_d; }
public:
	sserialize::CellQueryResult complete(std::string const & str, bool treedCqr, uint32_t threadCount);
private:
	sserialize::RCPtrWrapper<hic::Static::OscarSearchSgIndex> m_d;
};

sserialize::RCPtrWrapper<sserialize::spatial::dgg::interface::HCQRIndex>
makeOscarSearchSgHCQRIndex(sserialize::RCPtrWrapper<hic::Static::OscarSearchSgIndex> const & d);

}//end namespace hic::Static

//BEGIN Template function implementations
namespace hic::Static {

//BEGIN SgOpTree

template<typename TCQRType>
typename SgOpTree::Calc<TCQRType>::CQRType
SgOpTree::Calc<TCQRType>::Calc::calc(const Node * node) {
    if (!node) {
        return CQRType();
    }
	switch (node->baseType) {
	case Node::LEAF:
		switch (node->subType) {
		case Node::STRING:
		case Node::STRING_REGION:
		case Node::STRING_ITEM:
		{
			if (!node->value.size()) {
				return CQRType();
			}
			const std::string & str = node->value;
			std::string qstr(str);
			sserialize::StringCompleter::QuerryType qt = sserialize::StringCompleter::QT_NONE;
			qt = sserialize::StringCompleter::normalize(qstr);
			if (node->subType == Node::STRING_ITEM) {
				return m_d->items<CQRType>(qstr, qt);
			}
			else if (node->subType == Node::STRING_REGION) {
				return m_d->regions<CQRType>(qstr, qt);
			}
			else {
				return m_d->complete<CQRType>(qstr, qt);
			}
		}
		case Node::REGION:
			throw sserialize::UnsupportedFeatureException("OscarSearchWithSg: region");
		case Node::REGION_EXCLUSIVE_CELLS:
			throw sserialize::UnsupportedFeatureException("OscarSearchWithSg: region exclusive cells");
		case Node::CELL:
			throw sserialize::UnsupportedFeatureException("OscarSearchWithSg: cell");
		case Node::CELLS:
			throw sserialize::UnsupportedFeatureException("OscarSearchWithSg: cells");
		case Node::RECT:
			throw sserialize::UnsupportedFeatureException("OscarSearchWithSg: rectangle");
		case Node::POLYGON:
			throw sserialize::UnsupportedFeatureException("OscarSearchWithSg: polygon");
		case Node::PATH:
			throw sserialize::UnsupportedFeatureException("OscarSearchWithSg: path");
		case Node::POINT:
			throw sserialize::UnsupportedFeatureException("OscarSearchWithSg: point");
		case Node::ITEM:
			throw sserialize::UnsupportedFeatureException("OscarSearchWithSg: item");
		default:
			break;
		};
		break;
	case Node::UNARY_OP:
		switch(node->subType) {
		case Node::FM_CONVERSION_OP:
			return calc(node->children.at(0)).allToFull();
		case Node::CELL_DILATION_OP:
			throw sserialize::UnsupportedFeatureException("OscarSearchWithSg: cell dilation");
		case Node::REGION_DILATION_BY_ITEM_COVERAGE_OP:
		case Node::REGION_DILATION_BY_CELL_COVERAGE_OP:
			throw sserialize::UnsupportedFeatureException("OscarSearchWithSg: region dilation");
		case Node::COMPASS_OP:
			throw sserialize::UnsupportedFeatureException("OscarSearchWithSg: compass");
		case Node::IN_OP:
			throw sserialize::UnsupportedFeatureException("OscarSearchWithSg: in");
		case Node::NEAR_OP:
			throw sserialize::UnsupportedFeatureException("OscarSearchWithSg: near");
		case Node::RELEVANT_ELEMENT_OP:
			throw sserialize::UnsupportedFeatureException("OscarSearchWithSg: relevant item");
		case Node::QUERY_EXCLUSIVE_CELLS:
			throw sserialize::UnsupportedFeatureException("OscarSearchWithSg: query exclusive cells");
		default:
			break;
		};
		break;
	case Node::BINARY_OP:
		switch(node->subType) {
		case Node::SET_OP:
			switch (node->value.at(0)) {
			case '+':
				return calc(node->children.front()) + calc(node->children.back());
			case '/':
			case ' ':
				return calc(node->children.front()) / calc(node->children.back());
			case '-':
				return calc(node->children.front()) - calc(node->children.back());
			case '^':
				return calc(node->children.front()) ^ calc(node->children.back());
			default:
				return CQRType();
			};
			break;
		case Node::BETWEEN_OP:
			throw sserialize::UnsupportedFeatureException("OscarSearchWithSg: between query");
		default:
			break;
		};
		break;
	default:
		break;
	};
	return CQRType();
}
//END SgOpTree
	
template<typename T_CQR_TYPE>
T_CQR_TYPE OscarSearchSgIndex::complete(const std::string& qstr, const sserialize::StringCompleter::QuerryType qt) const {
	using CellInfo = hic::Static::detail::OscarSearchSgIndexCellInfo;
    auto ci = CellInfo::makeRc( sserialize::RCPtrWrapper<Self>(const_cast<OscarSearchSgIndex*>(this)) );
    try {
		Payload::Type t(typeFromCompletion(qstr, qt, m_mixed));
		return T_CQR_TYPE(idxStore().at( t.fmPtr() ), idxStore().at( t.pPtr() ), t.pItemsPtrBegin(), ci, idxStore(), flags());
	}
	catch (const sserialize::OutOfBoundsException & e) {
		return T_CQR_TYPE(ci, idxStore(), flags());
	}
}

template<typename T_CQR_TYPE>
T_CQR_TYPE OscarSearchSgIndex::regions(const std::string& qstr, const sserialize::StringCompleter::QuerryType qt) const {
	using CellInfo = hic::Static::detail::OscarSearchSgIndexCellInfo;
    auto ci = CellInfo::makeRc( sserialize::RCPtrWrapper<Self>(const_cast<OscarSearchSgIndex*>(this)) );
    try {
		Payload::Type t(typeFromCompletion(qstr, qt, m_regions));
		return T_CQR_TYPE(idxStore().at( t.fmPtr() ), idxStore().at( t.pPtr() ), t.pItemsPtrBegin(), ci, idxStore(), flags());
	}
	catch (const sserialize::OutOfBoundsException & e) {
		return T_CQR_TYPE(ci, idxStore(), flags());
	}
}

template<typename T_CQR_TYPE>
T_CQR_TYPE OscarSearchSgIndex::items(const std::string& qstr, const sserialize::StringCompleter::QuerryType qt) const {
	using CellInfo = hic::Static::detail::OscarSearchSgIndexCellInfo;
    auto ci = CellInfo::makeRc( sserialize::RCPtrWrapper<Self>(const_cast<OscarSearchSgIndex*>(this)) );
    try {
		Payload::Type t(typeFromCompletion(qstr, qt, m_items));
		return T_CQR_TYPE(idxStore().at( t.fmPtr() ), idxStore().at( t.pPtr() ), t.pItemsPtrBegin(), ci, idxStore(), flags());
	}
	catch (const sserialize::OutOfBoundsException & e) {
		return T_CQR_TYPE(ci, idxStore(), flags());
	}
}

template<typename T_CQR_TYPE>
T_CQR_TYPE
OscarSearchSgIndex::cell(uint32_t cellId) const {
	throw sserialize::UnimplementedFunctionException("OscarSearchSgIndex::cell");
	return T_CQR_TYPE();
}

template<typename T_CQR_TYPE>
T_CQR_TYPE
OscarSearchSgIndex::region(uint32_t regionId) const {
	throw sserialize::UnimplementedFunctionException("OscarSearchSgIndex::region");
	return T_CQR_TYPE();
}

}//end namespace hic::Static
//END template function implemenations
