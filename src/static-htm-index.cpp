#include "static-htm-index.h"
#include <sserialize/strings/unicode_case_functions.h>
#include <lsst/sphgeom/LonLat.h>
#include <lsst/sphgeom/Circle.h>
#include <lsst/sphgeom/Box.h>

namespace hic::Static {
	
	
HtmInfo::HtmInfo(const sserialize::UByteArrayAdapter & d) :
m_numLevels(d.getUint8(1)),
m_trixelId2HtmIndexId(d+2),
m_htmIndexId2TrixelId(d+(2+m_trixelId2HtmIndexId.getSizeInBytes())),
m_trixelItemIndexIds(d+(2+m_trixelId2HtmIndexId.getSizeInBytes()+m_htmIndexId2TrixelId.getSizeInBytes()))
{}

HtmInfo::~HtmInfo() {}

sserialize::UByteArrayAdapter::SizeType
HtmInfo::getSizeInBytes() const {
	return 1+m_trixelId2HtmIndexId.getSizeInBytes()+m_htmIndexId2TrixelId.getSizeInBytes()+m_trixelItemIndexIds.getSizeInBytes();
}

int HtmInfo::levels() const {
	return m_numLevels;
}

HtmInfo::SizeType
HtmInfo::trixelCount() const {
	return m_trixelId2HtmIndexId.size();
}

HtmInfo::IndexId
HtmInfo::trixelItemIndexId(TrixelId trixelId) const {
	return m_trixelItemIndexIds.at(trixelId);
}

HtmInfo::TrixelId
HtmInfo::trixelId(HtmIndexId htmIndex) const {
	return m_htmIndexId2TrixelId.at(htmIndex);
}

HtmInfo::HtmIndexId
HtmInfo::htmIndex(TrixelId trixelId) const {
	return m_trixelId2HtmIndexId.at(trixelId);
}

OscarSearchHtmIndex::OscarSearchHtmIndex(const sserialize::UByteArrayAdapter & d, const sserialize::Static::ItemIndexStore & idxStore) :
m_sq(d.at(1)),
m_htmInfo( d+2 ),
m_trie( Trie::PrivPtrType(new FlatTrieType(d+(2+m_htmInfo.getSizeInBytes()))) ),
m_idxStore(idxStore),
m_hp(m_htmLevels)
{
	SSERIALIZE_VERSION_MISSMATCH_CHECK(MetaData::version, d.at(0), "hic::Static::OscarSearchHtmIndex");
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
	return m_d->htmInfo().trixelCount();
}
sserialize::spatial::GeoRect
OscarSearchHtmIndexCellInfo::cellBoundary(CellId cellId) const {
	auto htmIdx = m_d->htmInfo().htmIndex(cellId);
	lsst::sphgeom::Box box = m_d->htm().triangle(htmIdx).getBoundingBox();
	auto lat = box.getLat();
	auto lon = box.getLon();
	return sserialize::spatial::GeoRect(lat.getA().asDegrees(), lat.getB().asDegrees(), lon.getA().asDegrees(), lon.getB().asDegrees());
}

OscarSearchHtmIndexCellInfo::SizeType
OscarSearchHtmIndexCellInfo::cellItemsCount(CellId cellId) const {
	return m_d->idxStore().idxSize(cellItemsPtr(cellId));
}

OscarSearchHtmIndexCellInfo::IndexId
OscarSearchHtmIndexCellInfo::cellItemsPtr(CellId cellId) const {
	return m_d->htmInfo().trixelItemIndexId(cellId);
}
	
}//end namespace detail
//END Static::detail::OscarSearchHtmIndexCellInfo

}//end namespace hic::Static
