#include "htm-index.h"
#include <lsst/sphgeom/LonLat.h>
#include <lsst/sphgeom/Circle.h>
#include <lsst/sphgeom/Box.h>
#include <sserialize/iterator/RangeGenerator.h>
#include <sserialize/mt/ThreadPool.h>
#include <sserialize/algorithm/hashspecializations.h>
#include <sserialize/stats/ProgressInfo.h>
#include <sserialize/stats/statfuncs.h>
#include <sserialize/strings/stringfunctions.h>
#include <sserialize/strings/unicode_case_functions.h>
#include <sserialize/utility/assert.h>
#include <sserialize/Static/Map.h>
#include <boost/range/adaptor/map.hpp>
#include <sserialize/utility/debuggerfunctions.h>
#include <tuple>
#include <chrono>

#include "HtmSpatialGrid.h"

namespace hic {
namespace {
class WorkerData {
public:
	WorkerData(WorkerData const &) = default;
	WorkerData(OscarHtmIndex::TrixelId trixelId, OscarHtmIndex::CellId cellId, OscarHtmIndex::ItemId itemId) :
	trixelId(trixelId),
	cellId(cellId),
	itemId(itemId)
	{}
	bool operator==(WorkerData const & other) const {
		return trixelId == other.trixelId && cellId == other.cellId && itemId == other.itemId;
	}
public:
	OscarHtmIndex::TrixelId trixelId;
	OscarHtmIndex::CellId cellId;
	OscarHtmIndex::ItemId itemId;
};

}}//end namespace hic

namespace std {
	template<>
	struct hash<hic::WorkerData> {
		hash<hic::OscarHtmIndex::TrixelId> trixelIdHash;
		hash<hic::OscarHtmIndex::CellId> cellIdHash;
		hash<hic::OscarHtmIndex::ItemId> itemIdHash;
		inline std::size_t operator()(const hic::WorkerData & v) const {
			size_t seed = 0;
			::hash_combine(seed, v.trixelId, trixelIdHash);
			::hash_combine(seed, v.cellId, cellIdHash);
			::hash_combine(seed, v.itemId, itemIdHash);
			return seed;
		}
	};
} //end namespace std

namespace hic {
	
OscarHtmIndex::OscarHtmIndex(Store const & store, IndexStore const & idxStore, sserialize::RCPtrWrapper<interface::SpatialGrid> const & sg) :
m_store(store),
m_idxStore(idxStore),
m_sg(sg)
{}

OscarHtmIndex::~OscarHtmIndex() {}

void OscarHtmIndex::create(uint32_t threadCount) {
	m_td.clear();
	m_ctm.clear();
	m_ctm.resize(m_store.geoHierarchy().cellSize());
	
	struct State {
		sserialize::ProgressInfo pinfo;
		std::atomic<uint32_t> cellId{0};
		uint32_t cellCount;
		
		sserialize::Static::spatial::GeoHierarchy gh;
		sserialize::Static::spatial::TriangulationGeoHierarchyArrangement tr;
		
		OscarHtmIndex * that;
		std::mutex flushLock;
	};
	
	struct Config {
		std::size_t workerCacheSize;
	};
	
	class Worker {
	public:
		using Data = WorkerData;
	public:
		Worker(State * state, Config * cfg) : state(state), cfg(cfg) {}
		Worker(Worker const & other) : state(other.state), cfg(other.cfg) {}
		
		void operator()() {
			while(true) {
				uint32_t cellId = state->cellId.fetch_add(1, std::memory_order_relaxed);
				if (cellId >= state->cellCount) {
					break;
				}
				
				process(cellId);
				
// 				if (m_tcd.size() > cfg->workerCacheSize) {
					flush();
// 				}
			}
			flush();
		}
		
		void process(uint32_t cellId) {
			state->pinfo(state->cellId);
			auto cell = state->gh.cell(cellId);
			auto cellIdx = state->that->m_idxStore.at( cell.itemPtr() );
		
			for(uint32_t itemId : cellIdx) {
				auto item = state->that->m_store.at(itemId);
				if (item.payload().cells().size() > 1) {
					item.geoShape().visitPoints([this,cellId,itemId](const sserialize::Static::spatial::GeoPoint & p) {
						std::set<uint32_t> cellIds = this->state->tr.cellIds(p);
						if (!cellIds.size()) {
							cellIds.insert(0);
						}
						if (!cellIds.count(cellId)) {
							return;
						}
						this->m_tcd.emplace(
							this->state->that->sg().index(p.lat(), p.lon()),
							cellId,
							itemId
						);
					});
				}
				else {
					SSERIALIZE_CHEAP_ASSERT_EQUAL(cellId, item.payload().cells().at(0));
					item.geoShape().visitPoints([this,cellId,itemId](const sserialize::Static::spatial::GeoPoint & p) {
						this->m_tcd.emplace(
							this->state->that->sg().index(p.lat(), p.lon()),
							cellId,
							itemId
						);
					});
				}
			}
		}
		
		void flush() {
			#ifdef SSERIALIZE_EXPENSIVE_ASSERT_ENABLED
			{
				std::map<CellId, std::set<ItemId> > cellItems;
				for(const Data & x : m_tcd) {
					cellItems[x.cellId].insert(x.itemId);
				}
				for(const auto & x : cellItems) {
					sserialize::ItemIndex realCellItems = state->that->m_idxStore.at( state->gh.cellItemsPtr(x.first) );
					sserialize::ItemIndex myCellItems(std::vector<uint32_t>(x.second.begin(), x.second.end()));
					sserialize::breakHereIf(myCellItems != realCellItems);
					SSERIALIZE_EXPENSIVE_ASSERT(myCellItems == realCellItems);
				}
			}
			#endif
			std::lock_guard<std::mutex> lck(state->flushLock);
			for(Data const & x : m_tcd) {
				state->that->m_td[x.trixelId][x.cellId].emplace_back(x.itemId);
				if (x.cellId != std::numeric_limits<uint32_t>::max()) {
					state->that->m_ctm.at(x.cellId).insert(x.trixelId);
				}
				else {
					std::cerr << std::endl << "Item " << x.itemId << "is in invalid cell" << std::endl;
				}
			}
			m_tcd.clear();
		}
	private:
		State * state;
		Config * cfg;
		std::unordered_set<Data> m_tcd;
	};
	
	State state;
	Config cfg;

	state.gh = m_store.geoHierarchy();
	state.tr = m_store.regionArrangement();
	state.that = this;
	state.cellCount = state.gh.cellSize();
	
	cfg.workerCacheSize = 128*1024*1024 / sizeof(Worker::Data);
	
	state.pinfo.begin(state.cellCount, "HtmIndex: processing");
	if (threadCount == 1) {
		Worker(&state, &cfg)();
	}
	else {
		sserialize::ThreadPool::execute(Worker(&state, &cfg), 0, sserialize::ThreadPool::CopyTaskTag());
	}
	state.pinfo.end();
	
	//Sort item ids
	for(auto & x : m_td) {
		for(auto & y : x.second) {
			std::sort(y.second.begin(), y.second.end());
			auto e = std::unique(y.second.begin(), y.second.end());
			y.second.resize(e - y.second.begin());
		}
	}
	
	#ifdef SSERIALIZE_EXPENSIVE_ASSERT_ENABLED
	{
		std::vector<std::set<ItemId> > cellItems(m_ctm.size());
		for(auto & x : m_td) {
			for(auto & y : x.second) {
				cellItems.at(y.first).insert(y.second.begin(), y.second.end());
			}
		}
		for(uint32_t cellId(0), s(cellItems.size()); cellId < s; ++cellId) {
			sserialize::ItemIndex realCellItems = m_idxStore.at( state.gh.cellItemsPtr(cellId) );
			SSERIALIZE_EXPENSIVE_ASSERT(realCellItems == cellItems.at(cellId));
		}
	}
	#endif
}


void OscarHtmIndex::stats() {
	std::cout << "OscarHtmIndex::stats:" << std::endl;
	std::cout << "#htm-pixels: " << m_td.size() << std::endl;
	std::unordered_map<TrixelId, double> trixelItemCount;
	std::unordered_map<TrixelId, double> trixelCellCount;
	std::vector<double> trixelAreas;
	
	for(auto & x : m_td) {
		for(auto & y : x.second) {
			trixelItemCount[x.first] += y.second.size();
		}
		trixelCellCount[x.first] += x.second.size();
		trixelAreas.push_back( (12700/2)*(12700/2) * sg().area(x.first));
	}
	
	auto tic_range = trixelItemCount | boost::adaptors::map_values;
	std::cout << "Trixel item counts:" << std::endl;
	sserialize::statistics::StatPrinting::print(std::cout, tic_range.begin(), tic_range.end());
	
	
	auto tcc_range = trixelCellCount | boost::adaptors::map_values;
	std::cout << "Trixel cell counts:" << std::endl;
	sserialize::statistics::StatPrinting::print(std::cout, tcc_range.begin(), tcc_range.end());
	
	std::cout << "Trixel area:" << std::endl;
	sserialize::statistics::StatPrinting::print(std::cout, trixelAreas.begin(), trixelAreas.end());
	
}

//BEGIN OscarSearchHtmIndex

//BEGIN OscarSearchHtmIndex::QueryTypeData

bool
OscarSearchHtmIndex::QueryTypeData::valid() const {
	return fmTrixels != std::numeric_limits<uint32_t>::max() && pmTrixels != std::numeric_limits<uint32_t>::max();
}

bool
OscarSearchHtmIndex::Entry::hasQueryType(sserialize::StringCompleter::QuerryType qt) const {
	return data.at( toPosition(qt) ).valid();
}

OscarSearchHtmIndex::QueryTypeData const &
OscarSearchHtmIndex::Entry::at(sserialize::StringCompleter::QuerryType qt) const {
	return data.at( toPosition(qt) );
}

OscarSearchHtmIndex::QueryTypeData &
OscarSearchHtmIndex::Entry::at(sserialize::StringCompleter::QuerryType qt) {
	return data.at( toPosition(qt) );
}

//END OscarSearchHtmIndex::QueryTypeData
//BEGIN OscarSearchHtmIndex::Entry

std::size_t
OscarSearchHtmIndex::Entry::toPosition(sserialize::StringCompleter::QuerryType qt) {
	switch (qt) {
	case sserialize::StringCompleter::QT_EXACT:
		return 0;
	case sserialize::StringCompleter::QT_PREFIX:
		return 1;
	case sserialize::StringCompleter::QT_SUFFIX:
		return 2;
	case sserialize::StringCompleter::QT_SUBSTRING:
		return 3;
	default:
		throw sserialize::OutOfBoundsException("OscarSearchHtmIndex::Entry::toPosition");
	};
}

//END OscarSearchHtmIndex::Entry
//BEGIN OscarSearchHtmIndex::WorkerBase


void
OscarSearchHtmIndex::WorkerBase::TrixelItems::add(TrixelId trixelId, ItemId itemId) {
	entries.emplace_back(Entry{trixelId, itemId});
}

void
OscarSearchHtmIndex::WorkerBase::TrixelItems::clear() {
	entries.clear();
}

void
OscarSearchHtmIndex::WorkerBase::TrixelItems::process() {
	using std::sort;
	using std::unique;
	sort(entries.begin(), entries.end(), [](Entry const & a, Entry const & b) {
		return (a.trixelId == b.trixelId ? a.itemId < b.itemId : a.trixelId < b.trixelId);
	});
	auto it = unique(entries.begin(), entries.end(), [](Entry const & a, Entry const & b) {
		return a.trixelId == b.trixelId && a.itemId == b.itemId;
	});
	entries.resize(it - entries.begin());
}

OscarSearchHtmIndex::WorkerBase::WorkerBase(State * state, Config * cfg) : 
m_state(state),
m_cfg(cfg)
{}

OscarSearchHtmIndex::WorkerBase::WorkerBase(WorkerBase const & other) :
m_state(other.m_state),
m_cfg(other.m_cfg)
{}

void
OscarSearchHtmIndex::WorkerBase::operator()() {
	while(true) {
		uint32_t strId = state().strId.fetch_add(1, std::memory_order_relaxed);
		if (strId >= state().strCount) {
			break;
		}
		for(auto qt : state().queryTypes) {
			process(strId, qt);
		}
		flush(strId);
		state().pinfo(state().strId);
	}
};

void
OscarSearchHtmIndex::WorkerBase::process(uint32_t strId, sserialize::StringCompleter::QuerryType qt) {
	CellTextCompleter::Payload payload = state().trie.at(strId);
	if ((payload.types() & qt) == sserialize::StringCompleter::QT_NONE) {
		return;
	}
	CellTextCompleter::Payload::Type typeData = payload.type(qt);
	if (!typeData.valid()) {
		std::cerr << std::endl << "Invalid trie payload data for string " << strId << " = " << state().trie.strAt(strId) << std::endl;
	}
	sserialize::ItemIndex fmCells = state().idxStore.at( typeData.fmPtr() );
	
	for(auto cellId : fmCells) {
		if (cellId >= state().that->m_ohi->cellTrixelMap().size()) {
			std::cerr << std::endl << "Invalid cellId for string with id " << strId << " = " << state().trie.strAt(strId) << std::endl;
		}
		auto const & trixels = state().that->m_ohi->cellTrixelMap().at(cellId);
		for(HtmIndexId htmIndex : trixels) {
			TrixelId trixelId = state().that->m_trixelIdMap.trixelId(htmIndex);
			auto const & trixelCells = state().that->m_ohi->trixelData().at(htmIndex);
			auto const & trixelCellItems = trixelCells.at(cellId);
			buffer.add(trixelId, trixelCellItems.begin(), trixelCellItems.end());
		}
	}
	
	sserialize::ItemIndex pmCells = state().idxStore.at( typeData.pPtr() );
	auto itemIdxIdIt = typeData.pItemsPtrBegin();
	for(auto cellId : pmCells) {
		uint32_t itemIdxId = *itemIdxIdIt;
		sserialize::ItemIndex items = state().idxStore.at(itemIdxId);
		
		auto const & trixels = state().that->m_ohi->cellTrixelMap().at(cellId);
		for(HtmIndexId htmIndex : trixels) {
			TrixelId trixelId = state().that->m_trixelIdMap.trixelId(htmIndex);
			auto const & trixelCellItems = state().that->m_ohi->trixelData().at(htmIndex).at(cellId);
			{
				auto fit = items.begin();
				auto fend = items.end();
				auto sit = trixelCellItems.begin();
				auto send = trixelCellItems.end();
				for(; fit!= fend && sit != send;) {
					if (*fit < *sit) {
						++fit;
					}
					else if (*sit < *fit) {
						++sit;
					}
					else {
						buffer.add(trixelId, *sit);
						++fit;
						++sit;
					}
				}
			}
		}
		
		++itemIdxIdIt;
	}
	flush(strId, qt);
}

void
OscarSearchHtmIndex::WorkerBase::flush(uint32_t strId, sserialize::StringCompleter::QuerryType qt) {
	SSERIALIZE_EXPENSIVE_ASSERT_EXEC(std::set<uint32_t> strItems)

	std::vector<TrixelId> fmTrixels;
	std::vector<TrixelId> pmTrixels;
	OscarSearchHtmIndex::QueryTypeData & d = m_bufferEntry.at(qt);
	buffer.process();
	for(auto it(buffer.entries.begin()), end(buffer.entries.end()); it != end;) {
		TrixelId trixelId = it->trixelId;
		for(; it != end && it->trixelId == trixelId; ++it) {
			itemIdBuffer.push_back(it->itemId);
		}
		
		HtmIndexId htmIndex = state().that->m_trixelIdMap.htmIndex(trixelId);
		if (itemIdBuffer.size() == state().trixelItemSize.at(trixelId)) { //fullmatch
			fmTrixels.emplace_back(trixelId);
		}
		else {
			pmTrixels.emplace_back(trixelId);
			d.pmItems.emplace_back(state().that->m_idxFactory.addIndex(itemIdBuffer));
		}
		SSERIALIZE_EXPENSIVE_ASSERT_EXEC(strItems.insert(itemIdBuffer.begin(), itemIdBuffer.end()))
		
		itemIdBuffer.clear();
	}
	d.fmTrixels = state().that->m_idxFactory.addIndex(fmTrixels);
	d.pmTrixels = state().that->m_idxFactory.addIndex(pmTrixels);
	SSERIALIZE_EXPENSIVE_ASSERT_EQUAL(strId, state().trie.find(state().trie.strAt(strId), qt & (sserialize::StringCompleter::QT_PREFIX | sserialize::StringCompleter::QT_SUBSTRING)));
	#ifdef SSERIALIZE_EXPENSIVE_ASSERT_ENABLED
	{
		auto cqr = state().that->ctc().complete(state().trie.strAt(strId), qt);
		sserialize::ItemIndex realItems = cqr.flaten();
		if (realItems != strItems) {
			cqr = state().that->ctc().complete(state().trie.strAt(strId), qt);
			std::cerr << std::endl << "OscarSearchHtmIndex: Items of entry " << strId << " = " << state().trie.strAt(strId) << " with qt=" << qt << " differ" << std::endl;
			sserialize::ItemIndex tmp(std::vector<uint32_t>(strItems.begin(), strItems.end()));
			sserialize::ItemIndex real_broken = realItems - tmp;
			sserialize::ItemIndex broken_real = tmp - realItems;
			std::cerr << "real - broken:" << real_broken.size() << std::endl;
			std::cerr << "broken - real:" << broken_real.size() << std::endl;
			if (real_broken.size() < 10) {
				std::cerr << "real - broken: " << real_broken << std::endl;
			}
			if (broken_real.size() < 10) {
				std::cerr << "broken - real: " << broken_real << std::endl;
			}
		}
		SSERIALIZE_EXPENSIVE_ASSERT(realItems == strItems);
		
	}
	#endif
	buffer.clear();
}

void
OscarSearchHtmIndex::WorkerBase::flush(uint32_t strId) {
	flush(strId, std::move(m_bufferEntry));
	m_bufferEntry = Entry();
}

//END OscarSearchHtmIndex::WorkerBase
//BEGIN OscarSearchHtmIndex::InMemoryFlusher


OscarSearchHtmIndex::InMemoryFlusher::InMemoryFlusher(State * state, Config * cfg) :
WorkerBase(state, cfg)
{}

OscarSearchHtmIndex::InMemoryFlusher::InMemoryFlusher(InMemoryFlusher const & other) :
WorkerBase(other)
{}

OscarSearchHtmIndex::InMemoryFlusher::~InMemoryFlusher() {}

void OscarSearchHtmIndex::InMemoryFlusher::flush(uint32_t strId, Entry && entry) {
	state().that->m_d.at(strId) = std::move(entry);
}

//END OscarSearchHtmIndex::InMemoryFlusher
//BEGIN OscarSearchHtmIndex::NoOpFlusher


OscarSearchHtmIndex::NoOpFlusher::NoOpFlusher(State * state, Config * cfg) :
WorkerBase(state, cfg)
{}

OscarSearchHtmIndex::NoOpFlusher::NoOpFlusher(InMemoryFlusher const & other) :
WorkerBase(other)
{}

OscarSearchHtmIndex::NoOpFlusher::~NoOpFlusher() {}

void OscarSearchHtmIndex::NoOpFlusher::flush(uint32_t strId, Entry && entry) {}

//END OscarSearchHtmIndex::NoOpFlusher
//BEGIN OscarSearchHtmIndex::SerializationFlusher

OscarSearchHtmIndex::SerializationFlusher::SerializationFlusher(SerializationState * sstate, State * state, Config * cfg) :
WorkerBase(state, cfg),
m_sstate(sstate)
{}

OscarSearchHtmIndex::SerializationFlusher::SerializationFlusher(const SerializationFlusher & other) :
WorkerBase(other),
m_sstate(other.m_sstate)
{}

void
OscarSearchHtmIndex::SerializationFlusher::flush(uint32_t strId, Entry && entry) {
	sserialize::UByteArrayAdapter tmp(0, sserialize::MM_PROGRAM_MEMORY);
	tmp << entry;
	std::unique_lock<std::mutex> lock(sstate().lock, std::defer_lock_t());
	if (sstate().lastPushedEntry+1 == strId) {
		lock.lock();
		sstate().lastPushedEntry += 1;
		sstate().ac.put(tmp);
	}
	else {
		lock.lock();
		sstate().queuedEntries[strId] = tmp;
	}
	
	//try to flush queued entries
	SSERIALIZE_CHEAP_ASSERT(lock.owns_lock());
	for(auto it(sstate().queuedEntries.begin()), end(sstate().queuedEntries.end()); it != end;) {
		if (it->first == sstate().lastPushedEntry+1) {
			sstate().lastPushedEntry += 1;
			sstate().ac.put(it->second);
			it = sstate().queuedEntries.erase(it);
		}
		else {
			break;
		}
	}
	
	if (sstate().queuedEntries.size() > cfg().workerCacheSize) {
		//If we are here then we likely have to wait multiple seconds (or even minutes)
		while (true) {
			lock.unlock();
			using namespace std::chrono_literals;
			std::this_thread::sleep_for(1s);
			lock.lock();
			if (sstate().queuedEntries.size() < cfg().workerCacheSize) {
				break;
			}
		}
	}
	
}

//END OscarSearchHtmIndex::SerializationFlusher
		
OscarSearchHtmIndex::OscarSearchHtmIndex(std::shared_ptr<Completer> cmp, std::shared_ptr<OscarHtmIndex> ohi) :
m_cmp(cmp),
m_ohi(ohi)
{}


void OscarSearchHtmIndex::computeTrixelItems() {
	if (m_trixelItems.size()) {
		throw sserialize::InvalidAlgorithmStateException("OscarSearchHtmIndex::computeTrixelItems: already computed!");
	}
	
	std::cout << "Computing trixel items and trixel map..." << std::flush;
	for(auto const & x : m_ohi->trixelData()) {
		HtmIndexId htmIndex = x.first;
		m_trixelIdMap.m_htmIndex2TrixelId[htmIndex] = m_trixelIdMap.m_trixelId2HtmIndex.size();
		m_trixelIdMap.m_trixelId2HtmIndex.emplace_back(htmIndex);
		
		if (x.second.size() > 1) {
			std::set<uint32_t> items;
			for(auto const & y : x.second) {
				items.insert(y.second.begin(), y.second.end());
			}
			m_trixelItems.emplace_back( m_idxFactory.addIndex(items) );
		}
		else {
			m_trixelItems.emplace_back( m_idxFactory.addIndex(x.second.begin()->second) );
		}
	}
	std::cout << "done" << std::endl;
}

void OscarSearchHtmIndex::create(uint32_t threadCount, FlusherType ft) {
	computeTrixelItems();
	
	State state;
	Config cfg;
	
	state.idxStore = m_cmp->indexStore();
	state.gh = m_cmp->store().geoHierarchy();
	state.trie = this->trie();
	state.strCount = state.trie.size();
	state.that = this;
	for(uint32_t ptr : m_trixelItems) {
		state.trixelItemSize.push_back(m_idxFactory.idxSize(ptr));
	}
	{
		std::array<sserialize::StringCompleter::QuerryType, 4> qts{{
			sserialize::StringCompleter::QT_EXACT, sserialize::StringCompleter::QT_PREFIX,
			sserialize::StringCompleter::QT_SUFFIX, sserialize::StringCompleter::QT_SUBSTRING
		}};
		auto sq = this->ctc().getSupportedQuerries();
		for(auto x : qts) {
			if (x & sq) {
				state.queryTypes.emplace_back(x);
			}
		}
	}
	cfg.workerCacheSize = 128*1024*1024/sizeof(uint64_t);
	
	m_d.resize(state.strCount);
	
	state.pinfo.begin(state.strCount, "OscarSearchHtmIndex: processing");
	if (ft == FT_IN_MEMORY) {
		if (threadCount == 1) {
			InMemoryFlusher(&state, &cfg)();
		}
		else {
			sserialize::ThreadPool::execute(InMemoryFlusher(&state, &cfg), threadCount, sserialize::ThreadPool::CopyTaskTag());
		}
	}
	else if (ft == FT_NO_OP) {
		if (threadCount == 1) {
			NoOpFlusher(&state, &cfg)();
		}
		else {
			sserialize::ThreadPool::execute(NoOpFlusher(&state, &cfg), threadCount, sserialize::ThreadPool::CopyTaskTag());
		}
	}
	state.pinfo.end();
}


sserialize::UByteArrayAdapter &
OscarSearchHtmIndex::create(sserialize::UByteArrayAdapter & dest, uint32_t threadCount) {
	
	computeTrixelItems();
	
	auto ctc = this->ctc();
	auto trie = this->trie();
	//OscarSearchHtmIndex
	dest.putUint8(1); //version
	dest.putUint8(ctc.getSupportedQuerries());
	
	//HtmInfo
	dest.putUint8(1); //version
	dest.putUint8(m_ohi->sg().defaultLevel());
	sserialize::BoundedCompactUintArray::create(trixelIdMap().m_trixelId2HtmIndex, dest);
	{
		std::vector<std::pair<uint64_t, uint32_t>> tmp(trixelIdMap().m_htmIndex2TrixelId.begin(), trixelIdMap().m_htmIndex2TrixelId.end());
		std::sort(tmp.begin(), tmp.end());
		sserialize::Static::Map<uint64_t, uint32_t>::create(tmp.begin(), tmp.end(), dest);
	}
	sserialize::BoundedCompactUintArray::create(trixelItems(), dest);
	
	//OscarSearchHtmIndex::Trie
	dest.put(trie.data()); //FlatTrieBase
	dest.putUint8(1); //FlatTrie Version
	
	
	State state;
	Config cfg;
	SerializationState sstate(dest);
	
	state.idxStore = m_cmp->indexStore();
	state.gh = m_cmp->store().geoHierarchy();
	state.trie = this->trie();
	state.strCount = state.trie.size();
	state.that = this;
	for(uint32_t ptr : m_trixelItems) {
		state.trixelItemSize.push_back(m_idxFactory.idxSize(ptr));
	}
	{
		std::array<sserialize::StringCompleter::QuerryType, 4> qts{{
			sserialize::StringCompleter::QT_EXACT, sserialize::StringCompleter::QT_PREFIX,
			sserialize::StringCompleter::QT_SUFFIX, sserialize::StringCompleter::QT_SUBSTRING
		}};
		auto sq = this->ctc().getSupportedQuerries();
		for(auto x : qts) {
			if (x & sq) {
				state.queryTypes.emplace_back(x);
			}
		}
	}
	cfg.workerCacheSize = std::size_t(threadCount)*128*1024*1024/sizeof(uint64_t);
	
	state.pinfo.begin(state.strCount, "OscarSearchHtmIndex: processing");
	if (threadCount == 1) {
		SerializationFlusher(&sstate, &state, &cfg)();
	}
	else {
		sserialize::ThreadPool::execute(SerializationFlusher(&sstate, &state, &cfg), threadCount, sserialize::ThreadPool::CopyTaskTag());
	}
	state.pinfo.end();
	SSERIALIZE_CHEAP_ASSERT_EQUAL(0, sstate.queuedEntries.size());
	
	sstate.ac.flush();
	return dest;
}

OscarSearchHtmIndex::TrieType
OscarSearchHtmIndex::trie() const {
	auto triePtr = ctc().trie().as<CellTextCompleter::FlatTrieType>();
	if (!triePtr) {
		throw sserialize::MissingDataException("OscarSearchHtmIndex: No geocell completer with flat trie");
	}
	return triePtr->trie();
}

OscarSearchHtmIndex::CellTextCompleter OscarSearchHtmIndex::ctc() const {
	if(!m_cmp->textSearch().hasSearch(liboscar::TextSearch::OOMGEOCELL)) {
		throw sserialize::MissingDataException("OscarSearchHtmIndex: No geocell completer");
	}
	return m_cmp->textSearch().get<liboscar::TextSearch::OOMGEOCELL>();
}

sserialize::UByteArrayAdapter &
OscarSearchHtmIndex::serialize(sserialize::UByteArrayAdapter & dest) const {
	auto ctc = this->ctc();
	auto trie = this->trie();
	//OscarSearchHtmIndex
	dest.putUint8(1); //version
	dest.putUint8(ctc.getSupportedQuerries());
	
	//HtmInfo
	dest.putUint8(1); //version
	dest.putUint8(m_ohi->sg().defaultLevel());
	sserialize::BoundedCompactUintArray::create(trixelIdMap().m_trixelId2HtmIndex, dest);
	{
		std::vector<std::pair<uint64_t, uint32_t>> tmp(trixelIdMap().m_htmIndex2TrixelId.begin(), trixelIdMap().m_htmIndex2TrixelId.end());
		std::sort(tmp.begin(), tmp.end());
		sserialize::Static::Map<uint64_t, uint32_t>::create(tmp.begin(), tmp.end(), dest);
	}
	sserialize::BoundedCompactUintArray::create(trixelItems(), dest);
	
	//OscarSearchHtmIndex::Trie
	dest.put(trie.data()); //FlatTrieBase
	dest.putUint8(1); //FlatTrie Version
	sserialize::Static::ArrayCreator<sserialize::UByteArrayAdapter> ac(dest);
	for(std::size_t i(0), s(m_d.size()); i < s; ++i) {
		auto const & x  = m_d[i];
		ac.beginRawPut();
		ac.rawPut() << x;
		ac.endRawPut();
	}
	ac.flush();
	return dest;
}

sserialize::UByteArrayAdapter &
operator<<(sserialize::UByteArrayAdapter & dest, OscarSearchHtmIndex::Entry const & entry) {
	//serializes to sserialize::Static::CellTextCompleter::Payload
	std::array<sserialize::StringCompleter::QuerryType, 4> qts{{
		sserialize::StringCompleter::QT_EXACT, sserialize::StringCompleter::QT_PREFIX,
		sserialize::StringCompleter::QT_SUFFIX, sserialize::StringCompleter::QT_SUBSTRING
	}};
	uint32_t numQt = 0;
	int qt = sserialize::StringCompleter::QT_NONE;
	sserialize::UByteArrayAdapter tmp(0, sserialize::MM_PROGRAM_MEMORY);
	std::vector<sserialize::UByteArrayAdapter::SizeType> streamSizes;
	for(auto x : qts) {
		if (entry.hasQueryType(x)) {
			numQt += 1;
			qt |= x;
			
			sserialize::UByteArrayAdapter::SizeType streamBegin = tmp.tellPutPtr();
			sserialize::RLEStream::Creator rlc(tmp);
			auto const & d = entry.at(x);
			rlc.put(d.fmTrixels);
			rlc.put(d.pmTrixels);
			for(auto y : d.pmItems) {
				rlc.put(y);
			}
			rlc.flush();
			streamSizes.emplace_back(tmp.tellPutPtr() - streamBegin);
		}
	}
	
	SSERIALIZE_EXPENSIVE_ASSERT_EXEC(auto entryBegin = dest.tellPutPtr());
	
	dest.putUint8(qt);
	for(std::size_t i(1), s(streamSizes.size()); i < s; ++i) {
		dest.putVlPackedUint32(streamSizes[i-1]);
	}
	dest.put(tmp);
	
	#ifdef SSERIALIZE_EXPENSIVE_ASSERT_ENABLED
	{
		sserialize::UByteArrayAdapter data(dest);
		data.setPutPtr(entryBegin);
		data.shrinkToPutPtr();
		sserialize::Static::CellTextCompleter::Payload payload(data);
		
		SSERIALIZE_EXPENSIVE_ASSERT_EQUAL(payload.types(), qt);
		for(auto x : qts) {
			if (x & qt) {
				sserialize::Static::CellTextCompleter::Payload::Type t(payload.type(x));
				OscarSearchHtmIndex::QueryTypeData const & qtd = entry.at(x);
				SSERIALIZE_EXPENSIVE_ASSERT_EQUAL(t.fmPtr(), qtd.fmTrixels);
				SSERIALIZE_EXPENSIVE_ASSERT_EQUAL(t.pPtr(), qtd.pmTrixels);
				auto pmItemsIt = t.pItemsPtrBegin();
				for(std::size_t i(0), s(qtd.pmItems.size()); i < s; ++i, ++pmItemsIt) {
					SSERIALIZE_EXPENSIVE_ASSERT_EQUAL(*pmItemsIt, qtd.pmItems[i]);
				}
			}
		}
	}
	#endif
	
	return dest;
}

//BEGIN OscarSearchHtmIndexCellInfo

OscarSearchHtmIndexCellInfo::OscarSearchHtmIndexCellInfo(std::shared_ptr<OscarSearchHtmIndex> & d) :
m_d(d)
{}
OscarSearchHtmIndexCellInfo::~OscarSearchHtmIndexCellInfo()
{}


sserialize::RCPtrWrapper<sserialize::interface::CQRCellInfoIface>
OscarSearchHtmIndexCellInfo::makeRc(std::shared_ptr<OscarSearchHtmIndex> & d) {
	return sserialize::RCPtrWrapper<sserialize::interface::CQRCellInfoIface>( new OscarSearchHtmIndexCellInfo(d) );
}

OscarSearchHtmIndexCellInfo::SizeType
OscarSearchHtmIndexCellInfo::cellSize() const {
	return m_d->trixelItems().size();
}
sserialize::spatial::GeoRect
OscarSearchHtmIndexCellInfo::cellBoundary(CellId cellId) const {
	return m_d->ohi()->sg().bbox(m_d->trixelIdMap().htmIndex(cellId));
}

OscarSearchHtmIndexCellInfo::SizeType
OscarSearchHtmIndexCellInfo::cellItemsCount(CellId cellId) const {
	return m_d->idxFactory().idxSize(cellItemsPtr(cellId));
}

OscarSearchHtmIndexCellInfo::IndexId
OscarSearchHtmIndexCellInfo::cellItemsPtr(CellId cellId) const {
	return m_d->trixelItems().at(cellId);
}
	
//END

//BEGIN OscarSearchWithHtm

OscarSearchWithHtm::OscarSearchWithHtm(std::shared_ptr<OscarSearchHtmIndex> d) :
m_d(d),
m_idxStore(m_d->idxFactory().asItemIndexStore()),
m_ci(OscarSearchHtmIndexCellInfo::makeRc(m_d))
{}

OscarSearchWithHtm::~OscarSearchWithHtm()
{}

sserialize::CellQueryResult
OscarSearchWithHtm::complete(const std::string & qs, sserialize::StringCompleter::QuerryType qt) {
	auto trie = m_d->trie();
	std::string qstr;
	if (m_d->ctc().getSupportedQuerries() & sserialize::StringCompleter::SQ_CASE_INSENSITIVE) {
		qstr = sserialize::unicode_to_lower(qs);
	}
	else {
		qstr = qs;
	}
	uint32_t pos = trie.find(qstr, qt);
	
	if (pos == trie.npos) {
		return sserialize::CellQueryResult();
	}
	
	OscarSearchHtmIndex::Entry const & e = m_d->data().at(pos);

	if (!e.hasQueryType(qt)) {
		if (qt & sserialize::StringCompleter::QT_SUBSTRING) {
			if (e.hasQueryType(sserialize::StringCompleter::QT_PREFIX)) { //exact suffix matches are either available or not
				qt = sserialize::StringCompleter::QT_PREFIX;
			}
			else if (e.hasQueryType(sserialize::StringCompleter::QT_SUFFIX)) {
				qt = sserialize::StringCompleter::QT_SUFFIX;
			}
			else if (e.hasQueryType(sserialize::StringCompleter::QT_EXACT)) {
				qt = sserialize::StringCompleter::QT_EXACT;
			}
		}
		else if (e.hasQueryType(sserialize::StringCompleter::QT_EXACT)) { //qt is either prefix, suffix, exact
			qt = sserialize::StringCompleter::QT_EXACT;
		}
	}
	
	
	if (!e.hasQueryType(qt)) {
		return sserialize::CellQueryResult();
	}

	OscarSearchHtmIndex::QueryTypeData const & d = e.at(qt);

	return sserialize::CellQueryResult(
										m_idxStore.at(d.fmTrixels),
										m_idxStore.at(d.pmTrixels),
										d.pmItems.begin(),
										m_ci,
										m_idxStore,
										sserialize::CellQueryResult::FF_CELL_GLOBAL_ITEM_IDS);
}


sserialize::CellQueryResult
OscarSearchWithHtm::complete(liboscar::AdvancedOpTree const & optree) {
	return process(optree.root());
}

sserialize::CellQueryResult
OscarSearchWithHtm::process(const liboscar::AdvancedOpTree::Node * node) {
	using CQRType = sserialize::CellQueryResult;
	using Node = liboscar::AdvancedOpTree::Node;
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
				return complete(qstr, qt);
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
			return process(node->children.at(0)).allToFull();
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
				return process(node->children.front()) + process(node->children.back());
			case '/':
			case ' ':
				return process(node->children.front()) / process(node->children.back());
			case '-':
				return process(node->children.front()) - process(node->children.back());
			case '^':
				return process(node->children.front()) ^ process(node->children.back());
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

//END
	
}//end namespace hic
