#include "static-htm-index.h"
#include <sserialize/strings/unicode_case_functions.h>
#include <sserialize/Static/Version.h>
#include <sserialize/spatial/TreedCQR.h>

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
m_type(decltype(m_type)(sserialize::Static::ensureVersion(d, MetaData::version, d.getUint8(0)).getUint8(1))),
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
SpatialGridInfo::cPixelCount() const {
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

OscarSearchSgIndex::OscarSearchSgIndex(const sserialize::UByteArrayAdapter & d, const sserialize::Static::ItemIndexStore & idxStore) :
m_sq(sserialize::Static::ensureVersion(d, MetaData::version, d.at(0)).at(1)),
m_sgInfo( d+2 ),
m_trie(d+(2+sgInfo().getSizeInBytes())),
m_mixed(d+(2+sgInfo().getSizeInBytes()+m_trie.getSizeInBytes())),
m_regions(d+(2+sgInfo().getSizeInBytes()+m_trie.getSizeInBytes()+m_mixed.getSizeInBytes())),
m_items(d+(2+sgInfo().getSizeInBytes()+m_trie.getSizeInBytes()+m_mixed.getSizeInBytes()+m_regions.getSizeInBytes())),
m_idxStore(idxStore)
{
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


sserialize::RCPtrWrapper<OscarSearchSgIndex>
OscarSearchSgIndex::make(const sserialize::UByteArrayAdapter & d, const sserialize::Static::ItemIndexStore & idxStore) {
    return sserialize::RCPtrWrapper<OscarSearchSgIndex>( new OscarSearchSgIndex(d, idxStore) );
}

OscarSearchSgIndex::~OscarSearchSgIndex() {}

sserialize::UByteArrayAdapter::SizeType
OscarSearchSgIndex::getSizeInBytes() const {
    return 0;
}

sserialize::Static::ItemIndexStore const &
OscarSearchSgIndex::idxStore() const {
    return m_idxStore;
}

int
OscarSearchSgIndex::flags() const {
    return m_flags;
}

std::ostream &
OscarSearchSgIndex::printStats(std::ostream & out) const {
	out << "OscarSearchSgIndex::BEGIN_STATS" << std::endl;
	m_trie.printStats(out);
	out << "OscarSearchSgIndex::END_STATS" << std::endl;
	return out;
}

sserialize::StringCompleter::SupportedQuerries
OscarSearchSgIndex::getSupportedQueries() const {
    return sserialize::StringCompleter::SupportedQuerries(m_sq);
}

OscarSearchSgIndex::Payload::Type
OscarSearchSgIndex::typeFromCompletion(const std::string& qs, const sserialize::StringCompleter::QuerryType qt, Payloads const & pd) const {
	std::string qstr;
	if (m_sq & sserialize::StringCompleter::SQ_CASE_INSENSITIVE) {
		qstr = sserialize::unicode_to_lower(qs);
	}
	else {
		qstr = qs;
	}
	auto pos = m_trie.find(qstr, (qt & sserialize::StringCompleter::QT_SUBSTRING || qt & sserialize::StringCompleter::QT_PREFIX));
	
	if (pos == m_trie.npos) {
		throw sserialize::OutOfBoundsException("OscarSearchSgIndex::typeFromCompletion");
	}
	
	Payload p( pd.at(pos) );
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
			throw sserialize::OutOfBoundsException("OscarSearchSgIndex::typeFromCompletion");
		}
	}
	else if (p.types() & sserialize::StringCompleter::QT_EXACT) { //qt is either prefix, suffix, exact
		t = p.type(sserialize::StringCompleter::QT_EXACT);
	}
	else {
		throw sserialize::OutOfBoundsException("OscarSearchSgIndex::typeFromCompletion");
	}
	return t;
}

//BEGIN SgOpTree

SgOpTree::SgOpTree(sserialize::RCPtrWrapper<hic::Static::OscarSearchSgIndex> const & d) :
m_d(d)
{}



//END SgOpTree

//BEGIN Static::detail::OscarSearchSgIndexCellInfo
namespace detail {


OscarSearchSgIndexCellInfo::OscarSearchSgIndexCellInfo(const sserialize::RCPtrWrapper<IndexType> & d) :
m_d(d)
{}
OscarSearchSgIndexCellInfo::~OscarSearchSgIndexCellInfo()
{}


OscarSearchSgIndexCellInfo::RCType
OscarSearchSgIndexCellInfo::makeRc(const sserialize::RCPtrWrapper<IndexType> & d) {
	return sserialize::RCPtrWrapper<sserialize::interface::CQRCellInfoIface>( new OscarSearchSgIndexCellInfo(d) );
}

OscarSearchSgIndexCellInfo::SizeType
OscarSearchSgIndexCellInfo::cellSize() const {
	return m_d->sgInfo().cPixelCount();
}
sserialize::spatial::GeoRect
OscarSearchSgIndexCellInfo::cellBoundary(CellId cellId) const {
	return m_d->sg().bbox(m_d->sgInfo().sgIndex(cellId));
}

OscarSearchSgIndexCellInfo::SizeType
OscarSearchSgIndexCellInfo::cellItemsCount(CellId cellId) const {
	return m_d->idxStore().idxSize(cellItemsPtr(cellId));
}

OscarSearchSgIndexCellInfo::IndexId
OscarSearchSgIndexCellInfo::cellItemsPtr(CellId cellId) const {
	return m_d->sgInfo().itemIndexId(cellId);
}

}//end namespace detail
//END Static::detail::OscarSearchSgIndexCellInfo

//BEGIN OscarSearchSgCompleter

void
OscarSearchSgCompleter::energize(std::string const & files) {
	auto indexData = sserialize::UByteArrayAdapter::openRo(files + "/index", false);
	auto searchData = sserialize::UByteArrayAdapter::openRo(files + "/search", false);
	auto idxStore = sserialize::Static::ItemIndexStore(indexData);
	m_d = hic::Static::OscarSearchSgIndex::make(searchData, idxStore);
}

sserialize::CellQueryResult
OscarSearchSgCompleter::complete(std::string const & str, bool treedCqr) {
	SgOpTree opTree(m_d);
	opTree.parse(str);
	if (treedCqr) {
		return opTree.calc<sserialize::TreedCellQueryResult>().toCQR();
	}
	else {
		return opTree.calc<sserialize::CellQueryResult>();
	}
}

//END OscarSearchSgCompleter
	
}//end namespace hic::Static
