#include "StaticHCQRTextIndex.h"
#include "StaticHCQRSpatialGrid.h"
#include <sserialize/strings/unicode_case_functions.h>
#include <sserialize/Static/Version.h>
#include <sserialize/mt/ThreadPool.h>

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


void
CompactNode::create(hic::impl::HCQRSpatialGrid::TreeNode const & src, sserialize::MultiBitBackInserter & dest) {
	if (src.isFullMatch()) {
		dest.push_back(1, 1);
	}
	else {
		dest.push_back(0, 1);
	}
	auto pixelIdBits = sserialize::fastLog2(src.pixelId());
	dest.push_back(pixelIdBits, 5);
	dest.push_back(src.pixelId(), pixelIdBits);
	
	if (!src.isFullMatch()) {
		auto idxIdBits = sserialize::fastLog2(src.itemIndexId());
		dest.push_back(idxIdBits, 5);
		dest.push_back(src.itemIndexId(), idxIdBits);
	}
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
	
	struct Aux {
		sserialize::RCPtrWrapper<hic::Static::OscarSearchSgIndex> sgIndex;
		CellInfo::RCType ci;
		sserialize::RCPtrWrapper<HCQRCellInfo> cellInfoPtr;
		sserialize::RCPtrWrapper<hic::interface::SpatialGridInfo> sgi;
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
		ac(dest),
		src(src)
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
			hic::Static::impl::HCQRSpatialGrid shcqr(static_cast<hic::impl::HCQRSpatialGrid const &>(*hcqr));
			return const_cast<hic::Static::impl::HCQRSpatialGrid const &>(shcqr).tree().data();
		}
		
		sserialize::UByteArrayAdapter sge2cn(HCQRPtr const & hcqr) {
			auto tmpd = sserialize::UByteArrayAdapter(0, sserialize::MM_PROGRAM_MEMORY);
			if (static_cast<hic::impl::HCQRSpatialGrid const &>(*hcqr).root()) {
				auto bi = sserialize::MultiBitBackInserter(tmpd);
				cnrec(bi, *static_cast<hic::impl::HCQRSpatialGrid const &>(*hcqr).root());
				bi.flush();
			}
			return tmpd;
		}
		
		void cnrec(sserialize::MultiBitBackInserter & dest, hic::impl::HCQRSpatialGrid::TreeNode const & node) {
			if (node.children().size()) {
				for(auto const & x : node.children()) {
					cnrec(dest, *x);
				}
			}
			else {
				detail::HCQRTextIndex::CompactNode::create(node, dest);
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
			
			HCQRPtr hcqr = HCQRPtr(new hic::impl::HCQRSpatialGrid (cqr, cqr.idxStore(), aux.sgIndex->sgPtr(), aux.sgi));
			if (cfg.compactify) {
				hcqr = hcqr->compactified(cfg.compactLevel);
				static_cast<hic::impl::HCQRSpatialGrid*>( hcqr.get() )->flushFetchedItems(cfg.idxFactory);
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

}//end namespace hic::Static
