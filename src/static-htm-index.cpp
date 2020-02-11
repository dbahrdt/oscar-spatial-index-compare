#include "static-htm-index.h"
#include <sserialize/strings/unicode_case_functions.h>
#include <sserialize/Static/Version.h>
#include <sserialize/spatial/TreedCQR.h>
#include <sserialize/iterator/RangeGenerator.h>
#include <sserialize/mt/ThreadPool.h>
#include <sserialize/spatial/dgg/SimpleGridSpatialGrid.h>
#include <sserialize/spatial/dgg/HCQRIndexFromCellIndex.h>
#include <sserialize/spatial/dgg/HCQRIndex.h>
#include <sserialize/spatial/dgg/HCQRSpatialGrid.h>
#include <sserialize/spatial/dgg/StaticHCQRSpatialGrid.h>
#include <sserialize/spatial/dgg/Static/HCQRCellInfo.h>
#include <sserialize/spatial/dgg/Static/HCQRTextIndex.h>
#include <sserialize/spatial/dgg/Static/SpatialGridRegistry.h>

#include "HtmSpatialGrid.h"
#include "H3SpatialGrid.h"
#include "S2GeomSpatialGrid.h"

namespace hic::Static {

OscarSearchSgIndex::OscarSearchSgIndex(const sserialize::UByteArrayAdapter & d, const sserialize::Static::ItemIndexStore & idxStore) :
m_sq(sserialize::Static::ensureVersion(d, MetaData::version, d.at(0)).at(1)),
m_sgInfo( std::make_shared<SpatialGridInfo>(d+2) ),
m_trie(d+(2+sgInfo().getSizeInBytes())),
m_mixed(d+(2+sgInfo().getSizeInBytes()+m_trie.getSizeInBytes())),
m_regions(d+(2+sgInfo().getSizeInBytes()+m_trie.getSizeInBytes()+m_mixed.getSizeInBytes())),
m_items(d+(2+sgInfo().getSizeInBytes()+m_trie.getSizeInBytes()+m_mixed.getSizeInBytes()+m_regions.getSizeInBytes())),
m_idxStore(idxStore)
{
	m_sg = sserialize::spatial::dgg::Static::SpatialGridRegistry::get().get(sgInfo());
}


sserialize::RCPtrWrapper<OscarSearchSgIndex>
OscarSearchSgIndex::make(const sserialize::UByteArrayAdapter & d, const sserialize::Static::ItemIndexStore & idxStore) {
    return sserialize::RCPtrWrapper<OscarSearchSgIndex>( new OscarSearchSgIndex(d, idxStore) );
}

OscarSearchSgIndex::~OscarSearchSgIndex() {}

sserialize::UByteArrayAdapter::SizeType
OscarSearchSgIndex::getSizeInBytes() const {
	return 2+m_sgInfo->getSizeInBytes()+m_trie.getSizeInBytes()+m_mixed.getSizeInBytes()+m_items.getSizeInBytes()+m_regions.getSizeInBytes();
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

//BEGIN HCQROscarCellIndex


HCQROscarCellIndex::HCQROscarCellIndex(sserialize::RCPtrWrapper<OscarSearchSgIndex> const & base) :
m_base(base)
{}

HCQROscarCellIndex::~HCQROscarCellIndex() {}

sserialize::StringCompleter::SupportedQuerries
HCQROscarCellIndex::getSupportedQueries() const {
	return m_base->getSupportedQueries();
}

HCQROscarCellIndex::CellQueryResult
HCQROscarCellIndex::complete(const std::string & qstr, const sserialize::StringCompleter::QuerryType qt) const {
	return m_base->complete<CellQueryResult>(qstr, qt);
}

HCQROscarCellIndex::CellQueryResult
HCQROscarCellIndex::items(const std::string & qstr, const sserialize::StringCompleter::QuerryType qt) const {
	return m_base->items<CellQueryResult>(qstr, qt);
}

HCQROscarCellIndex::CellQueryResult
HCQROscarCellIndex::regions(const std::string & qstr, const sserialize::StringCompleter::QuerryType qt) const {
	return m_base->regions<CellQueryResult>(qstr, qt);
}

HCQROscarCellIndex::CellQueryResult
HCQROscarCellIndex::cell(uint32_t cellId) const {
	return m_base->cell<CellQueryResult>(cellId);
}

HCQROscarCellIndex::CellQueryResult
HCQROscarCellIndex::region(uint32_t regionId) const {
	return m_base->region<CellQueryResult>(regionId);
}


//END

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
OscarSearchSgCompleter::complete(std::string const & str, bool treedCqr, uint32_t threadCount) {
	SgOpTree opTree(m_d);
	opTree.parse(str);
	if (treedCqr) {
		return opTree.calc<sserialize::TreedCellQueryResult>().toCQR(threadCount);
	}
	else {
		return opTree.calc<sserialize::CellQueryResult>();
	}
}

//END OscarSearchSgCompleter

//BEGIN HCQROscarSearchSgCompleter


sserialize::RCPtrWrapper<sserialize::spatial::dgg::interface::HCQRIndex>
makeOscarSearchSgHCQRIndex(sserialize::RCPtrWrapper<hic::Static::OscarSearchSgIndex> const & d) {
	using HCQRIndexImp = sserialize::spatial::dgg::HCQRIndexFromCellIndex;
	using SpatialGridInfoImp = sserialize::spatial::dgg::detail::HCQRIndexFromCellIndex::impl::SpatialGridInfoFromCellIndexWithIndex;
	using HCQRCellInfo = sserialize::spatial::dgg::Static::HCQRTextIndex::HCQRCellInfo;

	auto cellInfoPtr = sserialize::RCPtrWrapper<HCQRCellInfo>( new HCQRCellInfo(d->idxStore(), d->sgInfoPtr()) );
	HCQRIndexImp::SpatialGridInfoPtr sgi( new SpatialGridInfoImp(d->sgPtr(), cellInfoPtr) );
	HCQRIndexImp::CellIndexPtr ci( new hic::Static::HCQROscarCellIndex(d) );

	sserialize::RCPtrWrapper<HCQRIndexImp> uncachedIndex(
		new HCQRIndexImp(
			d->sgPtr(),
			sgi,
			ci
		)
	);
	return uncachedIndex;
}

//END HCQROscarSearchSgCompleter

//BEGIN OscarSearchHCQRTextIndexCreator

void OscarSearchHCQRTextIndexCreator::run() {
	using CreationConfig = OscarSearchHCQRTextIndexCreator;
	CreationConfig & cfg = *this;

	using HCQRTextIndex = sserialize::spatial::dgg::Static::HCQRTextIndex;
	using SpatialGridInfo = HCQRTextIndex::SpatialGridInfo;
    using Payload = HCQRTextIndex::Payload;
	using Trie = HCQRTextIndex::Trie;
	using Payloads = HCQRTextIndex::Payloads;
	using HCQRPtr = HCQRTextIndex::HCQRPtr;
	using HCQRCellInfo = HCQRTextIndex::HCQRCellInfo;
	
	using CellInfo = hic::Static::detail::OscarSearchSgIndexCellInfo;
	using SpatialGridInfoImp = sserialize::spatial::dgg::detail::HCQRIndexFromCellIndex::impl::SpatialGridInfoFromCellIndexWithIndex;
	
	struct Aux {
		sserialize::RCPtrWrapper<hic::Static::OscarSearchSgIndex> sgIndex;
		CellInfo::RCType ci;
		sserialize::RCPtrWrapper<sserialize::spatial::dgg::Static::HCQRCellInfo> cellInfoPtr;
		sserialize::RCPtrWrapper<sserialize::spatial::dgg::interface::SpatialGridInfo> sgi;
	};
	
	struct State {
		sserialize::ProgressInfo pinfo;
		hic::Static::OscarSearchSgIndex::Payloads const & src;
		std::atomic<std::size_t> i{0};
		
		sserialize::Static::ArrayCreator<sserialize::UByteArrayAdapter> ac;
		std::map<std::size_t, sserialize::UByteArrayAdapter> queue;
		std::mutex flushLock;
		void flush(std::size_t i, sserialize::UByteArrayAdapter && d) {
			std::lock_guard<std::mutex> lck(flushLock);
			if (ac.size() == i) {
				ac.beginRawPut();
				ac.rawPut().put(d);
				ac.endRawPut();
			}
			else {
				queue[i] = std::move(d);
			}
			for(auto it(queue.begin()), end(queue.end()); it != end && it->first == ac.size();) {
				ac.put(it->second);
				it = queue.erase(it);
			}
		}
		State(sserialize::UByteArrayAdapter & dest, hic::Static::OscarSearchSgIndex::Payloads const & src) :
		src(src),
		ac(dest)
		{}
	};
	
	struct Worker {
		std::array<sserialize::StringCompleter::QuerryType, 4> pqt{
			sserialize::StringCompleter::QT_EXACT,
			sserialize::StringCompleter::QT_PREFIX,
			sserialize::StringCompleter::QT_SUFFIX,
			sserialize::StringCompleter::QT_SUBSTRING
		};
	public:
		
		sserialize::UByteArrayAdapter sge2shcqr(HCQRPtr const & hcqr) {
			sserialize::spatial::dgg::Static::impl::HCQRSpatialGrid shcqr(static_cast<sserialize::spatial::dgg::impl::HCQRSpatialGrid const &>(*hcqr));
			return const_cast<sserialize::spatial::dgg::Static::impl::HCQRSpatialGrid const &>(shcqr).tree().data();
		}
		
		sserialize::UByteArrayAdapter sge2cn(HCQRPtr const & hcqr) {
			auto tmpd = sserialize::UByteArrayAdapter(0, sserialize::MM_PROGRAM_MEMORY);
			if (static_cast<sserialize::spatial::dgg::impl::HCQRSpatialGrid const &>(*hcqr).root()) {
				auto bi = sserialize::MultiBitBackInserter(tmpd);
				cnrec(bi, *static_cast<sserialize::spatial::dgg::impl::HCQRSpatialGrid const &>(*hcqr).root());
				bi.flush();
			}
			return tmpd;
		}
		
		void cnrec(sserialize::MultiBitBackInserter & dest, sserialize::spatial::dgg::impl::HCQRSpatialGrid::TreeNode const & node) {
			if (node.children().size()) {
				for(auto const & x : node.children()) {
					cnrec(dest, *x);
				}
			}
			else {
				sserialize::spatial::dgg::Static::detail::HCQRTextIndex::CompactNode::create(node, dest);
			}
		}
		
		sserialize::UByteArrayAdapter sge2payload(hic::Static::OscarSearchSgIndex::Payload::Type const & t) {
			sserialize::CellQueryResult cqr(
				cfg.idxStore.at( t.fmPtr() ),
				cfg.idxStore.at( t.pPtr() ),
				t.pItemsPtrBegin(),
				aux.ci, cfg.idxStore,
				aux.sgIndex->flags()
			);
			
			HCQRPtr hcqr = HCQRPtr(new sserialize::spatial::dgg::impl::HCQRSpatialGrid (cqr, cqr.idxStore(), aux.sgIndex->sgPtr(), aux.sgi));
			if (cfg.compactify) {
				hcqr = hcqr->compactified(cfg.compactLevel);
				static_cast<sserialize::spatial::dgg::impl::HCQRSpatialGrid*>( hcqr.get() )->flushFetchedItems(cfg.idxFactory);
			}
			if (cfg.compactTree) {
				return sge2cn(hcqr);
			}
			else {
				return sge2shcqr(hcqr);
			}
		}
		
		void operator()() {
			std::size_t s = aux.sgIndex->trie().size();
			while(true) {
				std::size_t i = state.i.fetch_add(1, std::memory_order_relaxed);
				if (i >= s) {
					return;
				}
				state.pinfo(i);
				auto t = state.src.at(i);
				sserialize::UByteArrayAdapter data(0, sserialize::MM_PROGRAM_MEMORY);
				data.putUint8(t.types());
				for(auto qt : pqt) {
					if (t.types() & qt) {
						data.put(sge2payload(t.type(qt)));
					}
				}
				state.flush(i, std::move(data));
			}
		}
		void flush();
		Worker(CreationConfig & cfg, Aux const & aux, State & state) :
		cfg(cfg), aux(aux), state(state)
		{}
		Worker(Worker const & other) = default;
	public:
		CreationConfig & cfg;
		Aux const & aux;
		State & state;
	};
	
	cfg.idxFactory.setDeduplication(false);
	cfg.idxFactory.insert(cfg.idxStore);
	cfg.idxFactory.setDeduplication(true);

	
	Aux aux;
	aux.sgIndex = hic::Static::OscarSearchSgIndex::make(cfg.src, cfg.idxStore);
	aux.ci = CellInfo::makeRc(aux.sgIndex);
	aux.cellInfoPtr = sserialize::RCPtrWrapper<HCQRCellInfo>( new HCQRCellInfo(cfg.idxStore, aux.sgIndex->sgInfoPtr()) );
	aux.sgi.reset( new SpatialGridInfoImp(aux.sgIndex->sgPtr(), aux.cellInfoPtr) );
 
	
	cfg.dest.putUint8(1); //version
	cfg.dest.putUint8(cfg.src.at(1)); //sq
	cfg.dest.put( sserialize::UByteArrayAdapter(cfg.dest, 2, aux.sgIndex->sgInfo().getSizeInBytes()) ); //sgInfo
	cfg.dest.put( aux.sgIndex->trie().data() );
	
	{
		State state(cfg.dest, aux.sgIndex->m_mixed);
		state.pinfo.begin(aux.sgIndex->trie().size(), "Processing mixed payload");
		sserialize::ThreadPool::execute(Worker(cfg, aux, state), cfg.threads, sserialize::ThreadPool::CopyTaskTag());
		state.ac.flush();
		state.pinfo.end();
	}
	
	{
		State state(cfg.dest, aux.sgIndex->m_items);
		state.pinfo.begin(aux.sgIndex->trie().size(), "Processing items payload");
		sserialize::ThreadPool::execute(Worker(cfg, aux, state), cfg.threads, sserialize::ThreadPool::CopyTaskTag());
		state.ac.flush();
		state.pinfo.end();
	}
	
	{
		State state(cfg.dest, aux.sgIndex->m_regions);
		state.pinfo.begin(aux.sgIndex->trie().size(), "Processing regions payload");
		sserialize::ThreadPool::execute(Worker(cfg, aux, state), cfg.threads, sserialize::ThreadPool::CopyTaskTag());
		state.ac.flush();
		state.pinfo.end();
	}
}

//END OscarSearchHCQRTextIndexCreator



}//end namespace hic::Static
