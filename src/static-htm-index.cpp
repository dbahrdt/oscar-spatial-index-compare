#include "static-htm-index.h"
#include <sserialize/strings/unicode_case_functions.h>

#include "HtmSpatialGrid.h"
#include "H3SpatialGrid.h"

namespace hic::Static {
	
namespace ssinfo::SpatialGridInfo {


sserialize::UByteArrayAdapter::SizeType
MetaData::getSizeInBytes() const {
	return 3+m_d->trixelId2HtmIndexId().getSizeInBytes()+m_d->htmIndexId2TrixelId().getSizeInBytes()+m_d->trixelItemIndexIds().getSizeInBytes();;
}

sserialize::UByteArrayAdapter::SizeType
MetaData::offset(DataMembers member) const {
	switch(member) {
		case DataMembers::type:
			return 1;
		case DataMembers::levels:
			return 2;
		case DataMembers::trixelId2HtmIndexId:
			return 3;
		case DataMembers::htmIndexId2TrixelId:
			return 3+m_d->trixelId2HtmIndexId().getSizeInBytes();
		case DataMembers::trixelItemIndexIds:
			return 3+m_d->trixelId2HtmIndexId().getSizeInBytes()+m_d->htmIndexId2TrixelId().getSizeInBytes();
		default:
			throw sserialize::InvalidEnumValueException("MetaData");
			break;
	};
	return 0;
}


Data::Data(const sserialize::UByteArrayAdapter & d) :
m_type(decltype(m_type)(d.getUint8(1))),
m_levels(d.getUint8(2)),
m_trixelId2HtmIndexId(d+3),
m_htmIndexId2TrixelId(d+(3+m_trixelId2HtmIndexId.getSizeInBytes())),
m_trixelItemIndexIds(d+(3+m_trixelId2HtmIndexId.getSizeInBytes()+m_htmIndexId2TrixelId.getSizeInBytes()))
{}

} //end namespace ssinfo::HtmInfo
	
SpatialGridInfo::SpatialGridInfo(const sserialize::UByteArrayAdapter & d) :
m_d(d)
{}

SpatialGridInfo::~SpatialGridInfo() {}

sserialize::UByteArrayAdapter::SizeType
SpatialGridInfo::getSizeInBytes() const {
	return MetaData(&m_d).getSizeInBytes();
}

int SpatialGridInfo::levels() const {
	return m_d.levels();
}

SpatialGridInfo::SizeType
SpatialGridInfo::trixelCount() const {
	return m_d.trixelId2HtmIndexId().size();
}

SpatialGridInfo::ItemIndexId
SpatialGridInfo::itemIndexId(CPixelId trixelId) const {
	return m_d.trixelItemIndexIds().at(trixelId);
}

SpatialGridInfo::CPixelId
SpatialGridInfo::cPixelId(SGPixelId htmIndex) const {
	return m_d.htmIndexId2TrixelId().at(htmIndex);
}

SpatialGridInfo::SGPixelId
SpatialGridInfo::sgIndex(CPixelId cPixelId) const {
	return m_d.trixelId2HtmIndexId().at64(cPixelId);
}

OscarSearchHtmIndex::OscarSearchHtmIndex(const sserialize::UByteArrayAdapter & d, const sserialize::Static::ItemIndexStore & idxStore) :
m_sq(d.at(1)),
m_sgInfo( d+2 ),
m_trie( Trie::PrivPtrType(new FlatTrieType(d+(2+sgInfo().getSizeInBytes()))) ),
m_idxStore(idxStore)
{
	SSERIALIZE_VERSION_MISSMATCH_CHECK(MetaData::version, d.at(0), "hic::Static::OscarSearchHtmIndex");
	switch(sgInfo().type()) {
		case SpatialGridInfo::MetaData::SG_HTM:
			m_sg = hic::HtmSpatialGrid::make(sgInfo().levels());
			break;
		case SpatialGridInfo::MetaData::SG_H3:
			m_sg = hic::H3SpatialGrid::make(sgInfo().levels());
			break;
		default:
			throw sserialize::TypeMissMatchException("SpatialGridType is invalid: " + std::to_string(sgInfo().type()));
			break;
	};
}


sserialize::RCPtrWrapper<OscarSearchHtmIndex>
OscarSearchHtmIndex::make(const sserialize::UByteArrayAdapter & d, const sserialize::Static::ItemIndexStore & idxStore) {
    return sserialize::RCPtrWrapper<OscarSearchHtmIndex>( new OscarSearchHtmIndex(d, idxStore) );
}

OscarSearchHtmIndex::~OscarSearchHtmIndex() {}

sserialize::UByteArrayAdapter::SizeType
OscarSearchHtmIndex::getSizeInBytes() const {
    return 0;
}

sserialize::Static::ItemIndexStore const &
OscarSearchHtmIndex::idxStore() const {
    return m_idxStore;
}

int
OscarSearchHtmIndex::flags() const {
    return m_flags;
}

std::ostream &
OscarSearchHtmIndex::printStats(std::ostream & out) const {
	out << "OscarSearchHtmIndex::BEGIN_STATS" << std::endl;
	m_trie.printStats(out);
	out << "OscarSearchHtmIndex::END_STATS" << std::endl;
	return out;
}

sserialize::StringCompleter::SupportedQuerries
OscarSearchHtmIndex::getSupportedQueries() const {
    return sserialize::StringCompleter::SupportedQuerries(m_sq);
}

OscarSearchHtmIndex::Payload::Type
OscarSearchHtmIndex::typeFromCompletion(const std::string& qs, const sserialize::StringCompleter::QuerryType qt) const {
	std::string qstr;
	if (m_sq & sserialize::StringCompleter::SQ_CASE_INSENSITIVE) {
		qstr = sserialize::unicode_to_lower(qs);
	}
	else {
		qstr = qs;
	}
	Payload p( m_trie.at(qstr, (qt & sserialize::StringCompleter::QT_SUBSTRING || qt & sserialize::StringCompleter::QT_PREFIX)) );
	Payload::Type t;
	if (p.types() & qt) {
		t = p.type(qt);
	}
	else if (qt & sserialize::StringCompleter::QT_SUBSTRING) {
		if (p.types() & sserialize::StringCompleter::QT_PREFIX) { //exact suffix matches are either available or not
			t = p.type(sserialize::StringCompleter::QT_PREFIX);
		}
		else if (p.types() & sserialize::StringCompleter::QT_SUFFIX) {
			t = p.type(sserialize::StringCompleter::QT_SUFFIX);
		}
		else if (p.types() & sserialize::StringCompleter::QT_EXACT) {
			t = p.type(sserialize::StringCompleter::QT_EXACT);
		}
		else {
			throw sserialize::OutOfBoundsException("OscarSearchHtmIndex::typeFromCompletion");
		}
	}
	else if (p.types() & sserialize::StringCompleter::QT_EXACT) { //qt is either prefix, suffix, exact
		t = p.type(sserialize::StringCompleter::QT_EXACT);
	}
	else {
		throw sserialize::OutOfBoundsException("OscarSearchHtmIndex::typeFromCompletion");
	}
	return t;
}

//BEGIN HtmOpTree

HtmOpTree::HtmOpTree(sserialize::RCPtrWrapper<hic::Static::OscarSearchHtmIndex> const & d) :
m_d(d)
{}

sserialize::CellQueryResult
HtmOpTree::calc() {
    return Calc(m_d).calc(root());
}

HtmOpTree::Calc::CQRType
HtmOpTree::Calc::Calc::calc(const Node * node) {
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
				throw sserialize::UnsupportedFeatureException("OscarSearchWithHtm: item string query");
			}
			else if (node->subType == Node::STRING_REGION) {
				throw sserialize::UnsupportedFeatureException("OscarSearchWithHtm: region string query");
			}
			else {
				return m_d->complete<CQRType>(qstr, qt);
			}
		}
		case Node::REGION:
			throw sserialize::UnsupportedFeatureException("OscarSearchWithHtm: region query");
		case Node::REGION_EXCLUSIVE_CELLS:
			throw sserialize::UnsupportedFeatureException("OscarSearchWithHtm: region exclusive cells");
		case Node::CELL:
			throw sserialize::UnsupportedFeatureException("OscarSearchWithHtm: cell");
		case Node::CELLS:
			throw sserialize::UnsupportedFeatureException("OscarSearchWithHtm: cells");
		case Node::RECT:
			throw sserialize::UnsupportedFeatureException("OscarSearchWithHtm: rectangle");
		case Node::POLYGON:
			throw sserialize::UnsupportedFeatureException("OscarSearchWithHtm: polygon");
		case Node::PATH:
			throw sserialize::UnsupportedFeatureException("OscarSearchWithHtm: path");
		case Node::POINT:
			throw sserialize::UnsupportedFeatureException("OscarSearchWithHtm: point");
		case Node::ITEM:
			throw sserialize::UnsupportedFeatureException("OscarSearchWithHtm: item");
		default:
			break;
		};
		break;
	case Node::UNARY_OP:
		switch(node->subType) {
		case Node::FM_CONVERSION_OP:
			return calc(node->children.at(0)).allToFull();
		case Node::CELL_DILATION_OP:
			throw sserialize::UnsupportedFeatureException("OscarSearchWithHtm: cell dilation");
		case Node::REGION_DILATION_OP:
			throw sserialize::UnsupportedFeatureException("OscarSearchWithHtm: region dilation");
		case Node::COMPASS_OP:
			throw sserialize::UnsupportedFeatureException("OscarSearchWithHtm: compass");
		case Node::IN_OP:
			throw sserialize::UnsupportedFeatureException("OscarSearchWithHtm: in query");
		case Node::NEAR_OP:
			throw sserialize::UnsupportedFeatureException("OscarSearchWithHtm: near query");
		case Node::RELEVANT_ELEMENT_OP:
			throw sserialize::UnsupportedFeatureException("OscarSearchWithHtm: relevant item query");
		case Node::QUERY_EXCLUSIVE_CELLS:
			throw sserialize::UnsupportedFeatureException("OscarSearchWithHtm: query exclusive cells");
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
			throw sserialize::UnsupportedFeatureException("OscarSearchWithHtm: between query");
		default:
			break;
		};
		break;
	default:
		break;
	};
	return CQRType();
}

//END HtmOpTree

//BEGIN Static::detail::OscarSearchHtmIndexCellInfo
namespace detail {


OscarSearchHtmIndexCellInfo::OscarSearchHtmIndexCellInfo(const sserialize::RCPtrWrapper<IndexType> & d) :
m_d(d)
{}
OscarSearchHtmIndexCellInfo::~OscarSearchHtmIndexCellInfo()
{}


OscarSearchHtmIndexCellInfo::RCType
OscarSearchHtmIndexCellInfo::makeRc(const sserialize::RCPtrWrapper<IndexType> & d) {
	return sserialize::RCPtrWrapper<sserialize::interface::CQRCellInfoIface>( new OscarSearchHtmIndexCellInfo(d) );
}

OscarSearchHtmIndexCellInfo::SizeType
OscarSearchHtmIndexCellInfo::cellSize() const {
	return m_d->sgInfo().trixelCount();
}
sserialize::spatial::GeoRect
OscarSearchHtmIndexCellInfo::cellBoundary(CellId cellId) const {
	return m_d->sg().bbox(m_d->sgInfo().sgIndex(cellId));
}

OscarSearchHtmIndexCellInfo::SizeType
OscarSearchHtmIndexCellInfo::cellItemsCount(CellId cellId) const {
	return m_d->idxStore().idxSize(cellItemsPtr(cellId));
}

OscarSearchHtmIndexCellInfo::IndexId
OscarSearchHtmIndexCellInfo::cellItemsPtr(CellId cellId) const {
	return m_d->sgInfo().itemIndexId(cellId);
}
	
}//end namespace detail
//END Static::detail::OscarSearchHtmIndexCellInfo

//BEGIN OscarSearchHtmCompleter

void
OscarSearchHtmCompleter::energize(std::string const & files) {
	auto indexData = sserialize::UByteArrayAdapter::openRo(files + "/index", false);
	auto searchData = sserialize::UByteArrayAdapter::openRo(files + "/search", false);
	auto idxStore = sserialize::Static::ItemIndexStore(indexData);
	m_d = hic::Static::OscarSearchHtmIndex::make(searchData, idxStore);
}

sserialize::CellQueryResult
OscarSearchHtmCompleter::complete(std::string const & str) {
	HtmOpTree opTree(m_d);
	opTree.parse(str);
	return opTree.calc();
}

//END OscarSearchHtmCompleter
	
}//end namespace hic::Static
