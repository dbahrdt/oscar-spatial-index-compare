#include "StaticHCQRTextIndex.h"
#include "StaticHCQRSpatialGrid.h"
#include <sserialize/strings/unicode_case_functions.h>
#include <sserialize/Static/Version.h>

namespace hic::Static {
namespace detail::HCQRTextIndex {
	

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
	
}//end namespace detail::HCQRTextIndex
	
sserialize::RCPtrWrapper<HCQRTextIndex>
HCQRTextIndex::make(const sserialize::UByteArrayAdapter & d, const sserialize::Static::ItemIndexStore & idxStore) {
	return sserialize::RCPtrWrapper<HCQRTextIndex>( new HCQRTextIndex(d, idxStore) );
}

HCQRTextIndex::~HCQRTextIndex()
{}

sserialize::UByteArrayAdapter::SizeType
HCQRTextIndex::getSizeInBytes() const {
	return 2+m_sgInfo->getSizeInBytes()+m_trie.getSizeInBytes()+m_mixed.getSizeInBytes()+m_items.getSizeInBytes()+m_regions.getSizeInBytes();
}

sserialize::Static::ItemIndexStore const &
HCQRTextIndex::idxStore() const {
	return m_idxStore;
}

int
HCQRTextIndex::flags() const {
	return m_flags;
}

std::ostream &
HCQRTextIndex::printStats(std::ostream & out) const {
	out << "HCQRTextIndex::BEGIN_STATS" << std::endl;
	m_trie.printStats(out);
	out << "HCQRTextIndex::END_STATS" << std::endl;
	return out;
	return out;
}

sserialize::StringCompleter::SupportedQuerries
HCQRTextIndex::getSupportedQueries() const {
	return sserialize::StringCompleter::SupportedQuerries(m_sq);
}

HCQRTextIndex::HCQRPtr
HCQRTextIndex::complete(const std::string & qstr, const sserialize::StringCompleter::QuerryType qt) const {
	using MyHCQR = hic::Static::impl::HCQRSpatialGrid;
    try {
		Payload::Type t(typeFromCompletion(qstr, qt, m_mixed));
		return HCQRPtr( new MyHCQR(t, idxStore(), sgPtr(), sgiPtr()) );
	}
	catch (const sserialize::OutOfBoundsException & e) {
		return HCQRPtr();
	}
}

HCQRTextIndex::HCQRPtr
HCQRTextIndex::items(const std::string & qstr, const sserialize::StringCompleter::QuerryType qt) const {
	using MyHCQR = hic::Static::impl::HCQRSpatialGrid;
    try {
		Payload::Type t(typeFromCompletion(qstr, qt, m_items));
		return HCQRPtr( new MyHCQR(t, idxStore(), sgPtr(), sgiPtr()) );
	}
	catch (const sserialize::OutOfBoundsException & e) {
		return HCQRPtr();
	}
}

HCQRTextIndex::HCQRPtr
HCQRTextIndex::regions(const std::string & qstr, const sserialize::StringCompleter::QuerryType qt) const {
	using MyHCQR = hic::Static::impl::HCQRSpatialGrid;
    try {
		Payload::Type t(typeFromCompletion(qstr, qt, m_regions));
		return HCQRPtr( new MyHCQR(t, idxStore(), sgPtr(), sgiPtr()) );
	}
	catch (const sserialize::OutOfBoundsException & e) {
		return HCQRPtr();
	}
}

HCQRTextIndex::HCQRPtr
HCQRTextIndex::cell(uint32_t cellId) const {
	throw sserialize::UnimplementedFunctionException("OscarSearchSgIndex::cell");
}

HCQRTextIndex::HCQRPtr
HCQRTextIndex::region(uint32_t regionId) const {
	throw sserialize::UnimplementedFunctionException("OscarSearchSgIndex::region");
}
	
HCQRTextIndex::SpatialGridInfo const &
HCQRTextIndex::sgInfo() const {
	return *m_sgInfo;
}

std::shared_ptr<HCQRTextIndex::SpatialGridInfo> const &
HCQRTextIndex::sgInfoPtr() const {
	return m_sgInfo;
}

hic::interface::SpatialGrid const &
HCQRTextIndex::sg() const {
	return *m_sg;
}

sserialize::RCPtrWrapper<hic::interface::SpatialGrid> const &
HCQRTextIndex::sgPtr() const {
	return m_sg;
}


hic::interface::SpatialGridInfo const &
HCQRTextIndex::sgi() const {
	return *m_sgi;
}

sserialize::RCPtrWrapper<hic::interface::SpatialGridInfo> const &
HCQRTextIndex::sgiPtr() const {
	return m_sgi;
}

HCQRTextIndex::HCQRTextIndex(const sserialize::UByteArrayAdapter & d, const sserialize::Static::ItemIndexStore & idxStore) :
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

HCQRTextIndex::Payload::Type
HCQRTextIndex::typeFromCompletion(const std::string& qs, sserialize::StringCompleter::QuerryType qt, Payloads const & pd) const {
	std::string qstr;
	if (m_sq & sserialize::StringCompleter::SQ_CASE_INSENSITIVE) {
		qstr = sserialize::unicode_to_lower(qs);
	}
	else {
		qstr = qs;
	}
	auto pos = m_trie.find(qstr, (qt & sserialize::StringCompleter::QT_SUBSTRING || qt & sserialize::StringCompleter::QT_PREFIX));
	
	if (pos == m_trie.npos) {
		throw sserialize::OutOfBoundsException("HCQRTextIndex::typeFromCompletion");
	}
	return pd.at(pos).type(qt);
}

void
HCQRTextIndex::fromOscarSearchSgIndex(CreationConfig & cfg)
{
	using CellInfo = hic::Static::detail::OscarSearchSgIndexCellInfo;
	using SpatialGridInfoImp = hic::detail::HCQRIndexFromCellIndex::impl::SpatialGridInfoFromCellIndexWithIndex;
	std::array<sserialize::StringCompleter::QuerryType, 4> pqt{
		sserialize::StringCompleter::QT_EXACT,
		sserialize::StringCompleter::QT_PREFIX,
		sserialize::StringCompleter::QT_SUFFIX,
		sserialize::StringCompleter::QT_SUBSTRING
	};
	auto idxStore = cfg.idxFactory.asItemIndexStore();
	auto sgIndex = hic::Static::OscarSearchSgIndex::make(cfg.src, idxStore);
	auto ci = CellInfo::makeRc(sgIndex);
	auto cellInfoPtr = sserialize::RCPtrWrapper<HCQRCellInfo>( new HCQRCellInfo(idxStore, sgIndex->sgInfoPtr()) );
	sserialize::RCPtrWrapper<hic::interface::SpatialGridInfo> sgi( new SpatialGridInfoImp(sgIndex->sgPtr(), cellInfoPtr) );
 
	auto sge2shcqr = [&sgIndex, &ci, &sgi, &idxStore, &cfg](hic::Static::OscarSearchSgIndex::Payload::Type const & t) {
		sserialize::CellQueryResult cqr(
			idxStore.at( t.fmPtr() ),
			idxStore.at( t.pPtr() ),
			t.pItemsPtrBegin(),
			ci, idxStore,
			sgIndex->flags()
		);
		
		HCQRPtr hcqr = HCQRPtr(new hic::impl::HCQRSpatialGrid (cqr, cqr.idxStore(), sgIndex->sgPtr(), sgi));
		if (cfg.compactify) {
			hcqr = hcqr->compactified(cfg.compactLevel);
			static_cast<hic::impl::HCQRSpatialGrid*>( hcqr.get() )->flushFetchedItems(cfg.idxFactory);
		}
		hic::Static::impl::HCQRSpatialGrid shcqr(static_cast<hic::impl::HCQRSpatialGrid const &>(*hcqr));
		return const_cast<hic::Static::impl::HCQRSpatialGrid const &>(shcqr).tree().data();
	};
	
	auto transformPayload = [&sge2shcqr, &pqt,&sgIndex](hic::Static::OscarSearchSgIndex::Payloads const & src, sserialize::Static::ArrayCreator<sserialize::UByteArrayAdapter> && dest) {
		for(uint32_t i(0), s(sgIndex->trie().size()); i < s; ++i) {
			auto t = src.at(i);
			dest.beginRawPut();
			dest.rawPut().putUint8(t.types());
			for(auto qt : pqt) {
				if (t.types() & qt) {
					dest.rawPut().put(sge2shcqr(t.type(qt)));
				}
			}
			dest.endRawPut();
		}
		dest.flush();
	};
	
	cfg.dest.putUint8(1); //version
	cfg.dest.putUint8(cfg.src.at(1)); //sq
	cfg.dest.put( sserialize::UByteArrayAdapter(cfg.dest, 2, sgIndex->sgInfo().getSizeInBytes()) ); //sgInfo
	cfg.dest.put( sgIndex->trie().data() );
	
	transformPayload(sgIndex->m_mixed, cfg.dest);
	transformPayload(sgIndex->m_items, cfg.dest);
	transformPayload(sgIndex->m_regions, cfg.dest);
	
}

}//end namespace hic::Static
