#include "StaticHCQRTextIndex.h"
#include "StaticHCQRSpatialGrid.h"
#include <sserialize/strings/unicode_case_functions.h>
#include <sserialize/Static/Version.h>

namespace hic::Static {
namespace detail::StaticHCQRTextIndex {
	

Payload::Payload() :
m_types(QuerryType::QT_NONE)
{}

Payload::Payload(sserialize::UByteArrayAdapter const & d) :
m_types(d.at(0)),
m_d(d+1)
{}

Payload::~Payload() {}

Payload::QuerryType
Payload::types() const {
	return QuerryType(m_types & 0xF);
}

Payload::Type
Payload::type(QuerryType qt) const {
	return typeData(qt);
}

sserialize::UByteArrayAdapter
Payload::typeData(QuerryType qt) const {
	qt = sserialize::StringCompleter::toAvailable(qt, types());
	if (qt == sserialize::StringCompleter::QT_NONE) {
		throw sserialize::OutOfBoundsException("OscarSearchSgIndex::typeFromCompletion");
	}
	uint32_t pos = sserialize::Static::CellTextCompleter::Payload::qt2Pos(qt, types());
	sserialize::UByteArrayAdapter d(m_d);
	for(uint32_t i(0); i < pos; ++i) {
		hic::Static::detail::HCQRSpatialGrid::Tree::MetaData md(d);
		d += md.StorageSize + md.dataSize();
	}
	return d;
}
	
}//end namespace detail::StaticHCQRTextIndex
	
sserialize::RCPtrWrapper<StaticHCQRTextIndex>
StaticHCQRTextIndex::make(const sserialize::UByteArrayAdapter & d, const sserialize::Static::ItemIndexStore & idxStore) {
	return sserialize::RCPtrWrapper<StaticHCQRTextIndex>( new StaticHCQRTextIndex(d, idxStore) );
}

StaticHCQRTextIndex::~StaticHCQRTextIndex()
{}

sserialize::UByteArrayAdapter::SizeType
StaticHCQRTextIndex::getSizeInBytes() const {
	return 2+m_sgInfo->getSizeInBytes()+m_trie.getSizeInBytes()+m_mixed.getSizeInBytes()+m_items.getSizeInBytes()+m_regions.getSizeInBytes();
}

sserialize::Static::ItemIndexStore const &
StaticHCQRTextIndex::idxStore() const {
	return m_idxStore;
}

int
StaticHCQRTextIndex::flags() const {
	return m_flags;
}

std::ostream &
StaticHCQRTextIndex::printStats(std::ostream & out) const {
	out << "StaticHCQRTextIndex::BEGIN_STATS" << std::endl;
	m_trie.printStats(out);
	out << "StaticHCQRTextIndex::END_STATS" << std::endl;
	return out;
	return out;
}

sserialize::StringCompleter::SupportedQuerries
StaticHCQRTextIndex::getSupportedQueries() const {
	return sserialize::StringCompleter::SupportedQuerries(m_sq);
}

StaticHCQRTextIndex::HCQRPtr
StaticHCQRTextIndex::complete(const std::string & qstr, const sserialize::StringCompleter::QuerryType qt) const {
	using MyHCQR = hic::Static::impl::HCQRSpatialGrid;
    try {
		Payload::Type t(typeFromCompletion(qstr, qt, m_mixed));
		return HCQRPtr( new MyHCQR(t, idxStore(), sgPtr(), sgiPtr()) );
	}
	catch (const sserialize::OutOfBoundsException & e) {
		return HCQRPtr();
	}
}

StaticHCQRTextIndex::HCQRPtr
StaticHCQRTextIndex::items(const std::string & qstr, const sserialize::StringCompleter::QuerryType qt) const {
	using MyHCQR = hic::Static::impl::HCQRSpatialGrid;
    try {
		Payload::Type t(typeFromCompletion(qstr, qt, m_items));
		return HCQRPtr( new MyHCQR(t, idxStore(), sgPtr(), sgiPtr()) );
	}
	catch (const sserialize::OutOfBoundsException & e) {
		return HCQRPtr();
	}
}

StaticHCQRTextIndex::HCQRPtr
StaticHCQRTextIndex::regions(const std::string & qstr, const sserialize::StringCompleter::QuerryType qt) const {
	using MyHCQR = hic::Static::impl::HCQRSpatialGrid;
    try {
		Payload::Type t(typeFromCompletion(qstr, qt, m_regions));
		return HCQRPtr( new MyHCQR(t, idxStore(), sgPtr(), sgiPtr()) );
	}
	catch (const sserialize::OutOfBoundsException & e) {
		return HCQRPtr();
	}
}

StaticHCQRTextIndex::HCQRPtr
StaticHCQRTextIndex::cell(uint32_t cellId) const {
	throw sserialize::UnimplementedFunctionException("OscarSearchSgIndex::cell");
}

StaticHCQRTextIndex::HCQRPtr
StaticHCQRTextIndex::region(uint32_t regionId) const {
	throw sserialize::UnimplementedFunctionException("OscarSearchSgIndex::region");
}
	
StaticHCQRTextIndex::SpatialGridInfo const &
StaticHCQRTextIndex::sgInfo() const {
	return *m_sgInfo;
}

std::shared_ptr<StaticHCQRTextIndex::SpatialGridInfo> const &
StaticHCQRTextIndex::sgInfoPtr() const {
	return m_sgInfo;
}

hic::interface::SpatialGrid const &
StaticHCQRTextIndex::sg() const {
	return *m_sg;
}

sserialize::RCPtrWrapper<hic::interface::SpatialGrid> const &
StaticHCQRTextIndex::sgPtr() const {
	return m_sg;
}

StaticHCQRTextIndex::StaticHCQRTextIndex(const sserialize::UByteArrayAdapter & d, const sserialize::Static::ItemIndexStore & idxStore) :
m_sq(sserialize::Static::ensureVersion(d, MetaData::version, d.at(0)).at(1)),
m_sgInfo( std::make_shared<SpatialGridInfo>(d+2) ),
m_trie(d+(2+sgInfo().getSizeInBytes())),
m_mixed(d+(2+sgInfo().getSizeInBytes()+m_trie.getSizeInBytes())),
m_regions(d+(2+sgInfo().getSizeInBytes()+m_trie.getSizeInBytes()+m_mixed.getSizeInBytes())),
m_items(d+(2+sgInfo().getSizeInBytes()+m_trie.getSizeInBytes()+m_mixed.getSizeInBytes()+m_regions.getSizeInBytes())),
m_idxStore(idxStore)
{
	using SpatialGridInfoImp = hic::detail::HCQRIndexFromCellIndex::impl::SpatialGridInfoFromCellIndexWithIndex;

	auto cellInfoPtr = sserialize::RCPtrWrapper<HCQRCellInfo>( new HCQRCellInfo(this->idxStore(), sgInfoPtr()) );
	m_sgi.reset( new SpatialGridInfoImp(sgPtr(), cellInfoPtr) );
}

StaticHCQRTextIndex::Payload::Type
StaticHCQRTextIndex::typeFromCompletion(const std::string& qs, sserialize::StringCompleter::QuerryType qt, Payloads const & pd) const {
	std::string qstr;
	if (m_sq & sserialize::StringCompleter::SQ_CASE_INSENSITIVE) {
		qstr = sserialize::unicode_to_lower(qs);
	}
	else {
		qstr = qs;
	}
	auto pos = m_trie.find(qstr, (qt & sserialize::StringCompleter::QT_SUBSTRING || qt & sserialize::StringCompleter::QT_PREFIX));
	
	if (pos == m_trie.npos) {
		throw sserialize::OutOfBoundsException("StaticHCQRTextIndex::typeFromCompletion");
	}
	return pd.at(pos).type(qt);
}

}//end namespace hic::Static
