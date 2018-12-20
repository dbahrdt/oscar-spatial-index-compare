#include "OscarSearchSgIndex.h"

#include <sserialize/Static/Map.h>
#include <sserialize/mt/ThreadPool.h>

namespace hic {

//BEGIN OscarSearchSgIndex

//BEGIN OscarSearchSgIndex::QueryTypeData

bool
OscarSearchSgIndex::QueryTypeData::valid() const {
	return fmTrixels != std::numeric_limits<uint32_t>::max() && pmTrixels != std::numeric_limits<uint32_t>::max();
}

bool
OscarSearchSgIndex::Entry::hasQueryType(sserialize::StringCompleter::QuerryType qt) const {
	return data.at( toPosition(qt) ).valid();
}

OscarSearchSgIndex::QueryTypeData const &
OscarSearchSgIndex::Entry::at(sserialize::StringCompleter::QuerryType qt) const {
	return data.at( toPosition(qt) );
}

OscarSearchSgIndex::QueryTypeData &
OscarSearchSgIndex::Entry::at(sserialize::StringCompleter::QuerryType qt) {
	return data.at( toPosition(qt) );
}

//END OscarSearchSgIndex::QueryTypeData
//BEGIN OscarSearchSgIndex::Entry

std::size_t
OscarSearchSgIndex::Entry::toPosition(sserialize::StringCompleter::QuerryType qt) {
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
		throw sserialize::OutOfBoundsException("OscarSearchSgIndex::Entry::toPosition");
	};
}

//END OscarSearchSgIndex::Entry
//BEGIN OscarSearchSgIndex::WorkerBase


void
OscarSearchSgIndex::WorkerBase::TrixelItems::add(TrixelId trixelId, ItemId itemId) {
	entries.emplace_back(Entry{trixelId, itemId});
}

void
OscarSearchSgIndex::WorkerBase::TrixelItems::clear() {
	entries.clear();
}

void
OscarSearchSgIndex::WorkerBase::TrixelItems::process() {
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

OscarSearchSgIndex::WorkerBase::WorkerBase(State * state, Config * cfg) : 
m_state(state),
m_cfg(cfg)
{}

OscarSearchSgIndex::WorkerBase::WorkerBase(WorkerBase const & other) :
m_state(other.m_state),
m_cfg(other.m_cfg)
{}

void
OscarSearchSgIndex::WorkerBase::operator()() {
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
OscarSearchSgIndex::WorkerBase::process(uint32_t strId, sserialize::StringCompleter::QuerryType qt) {
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
OscarSearchSgIndex::WorkerBase::flush(uint32_t strId, sserialize::StringCompleter::QuerryType qt) {
	SSERIALIZE_EXPENSIVE_ASSERT_EXEC(std::set<uint32_t> strItems)

	std::vector<TrixelId> fmTrixels;
	std::vector<TrixelId> pmTrixels;
	OscarSearchSgIndex::QueryTypeData & d = m_bufferEntry.at(qt);
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
			std::cerr << std::endl << "OscarSearchSgIndex: Items of entry " << strId << " = " << state().trie.strAt(strId) << " with qt=" << qt << " differ" << std::endl;
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
OscarSearchSgIndex::WorkerBase::flush(uint32_t strId) {
	flush(strId, std::move(m_bufferEntry));
	m_bufferEntry = Entry();
}

//END OscarSearchSgIndex::WorkerBase
//BEGIN OscarSearchSgIndex::InMemoryFlusher


OscarSearchSgIndex::InMemoryFlusher::InMemoryFlusher(State * state, Config * cfg) :
WorkerBase(state, cfg)
{}

OscarSearchSgIndex::InMemoryFlusher::InMemoryFlusher(InMemoryFlusher const & other) :
WorkerBase(other)
{}

OscarSearchSgIndex::InMemoryFlusher::~InMemoryFlusher() {}

void OscarSearchSgIndex::InMemoryFlusher::flush(uint32_t strId, Entry && entry) {
	state().that->m_d.at(strId) = std::move(entry);
}

//END OscarSearchSgIndex::InMemoryFlusher
//BEGIN OscarSearchSgIndex::NoOpFlusher


OscarSearchSgIndex::NoOpFlusher::NoOpFlusher(State * state, Config * cfg) :
WorkerBase(state, cfg)
{}

OscarSearchSgIndex::NoOpFlusher::NoOpFlusher(InMemoryFlusher const & other) :
WorkerBase(other)
{}

OscarSearchSgIndex::NoOpFlusher::~NoOpFlusher() {}

void OscarSearchSgIndex::NoOpFlusher::flush(uint32_t strId, Entry && entry) {}

//END OscarSearchSgIndex::NoOpFlusher
//BEGIN OscarSearchSgIndex::SerializationFlusher

OscarSearchSgIndex::SerializationFlusher::SerializationFlusher(SerializationState * sstate, State * state, Config * cfg) :
WorkerBase(state, cfg),
m_sstate(sstate)
{}

OscarSearchSgIndex::SerializationFlusher::SerializationFlusher(const SerializationFlusher & other) :
WorkerBase(other),
m_sstate(other.m_sstate)
{}

void
OscarSearchSgIndex::SerializationFlusher::flush(uint32_t strId, Entry && entry) {
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

//END OscarSearchSgIndex::SerializationFlusher
		
OscarSearchSgIndex::OscarSearchSgIndex(std::shared_ptr<Completer> cmp, std::shared_ptr<OscarSgIndex> ohi) :
m_cmp(cmp),
m_ohi(ohi)
{}


void OscarSearchSgIndex::computeTrixelItems() {
	if (m_trixelItems.size()) {
		throw sserialize::InvalidAlgorithmStateException("OscarSearchSgIndex::computeTrixelItems: already computed!");
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

void OscarSearchSgIndex::create(uint32_t threadCount, FlusherType ft) {
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
	
	state.pinfo.begin(state.strCount, "OscarSearchSgIndex: processing");
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
OscarSearchSgIndex::create(sserialize::UByteArrayAdapter & dest, uint32_t threadCount) {
	
	computeTrixelItems();
	
	auto ctc = this->ctc();
	auto trie = this->trie();
	//OscarSearchSgIndex
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
	
	//OscarSearchSgIndex::Trie
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
	
	state.pinfo.begin(state.strCount, "OscarSearchSgIndex: processing");
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

OscarSearchSgIndex::TrieType
OscarSearchSgIndex::trie() const {
	auto triePtr = ctc().trie().as<CellTextCompleter::FlatTrieType>();
	if (!triePtr) {
		throw sserialize::MissingDataException("OscarSearchSgIndex: No geocell completer with flat trie");
	}
	return triePtr->trie();
}

OscarSearchSgIndex::CellTextCompleter OscarSearchSgIndex::ctc() const {
	if(!m_cmp->textSearch().hasSearch(liboscar::TextSearch::OOMGEOCELL)) {
		throw sserialize::MissingDataException("OscarSearchSgIndex: No geocell completer");
	}
	return m_cmp->textSearch().get<liboscar::TextSearch::OOMGEOCELL>();
}

sserialize::UByteArrayAdapter &
OscarSearchSgIndex::serialize(sserialize::UByteArrayAdapter & dest) const {
	auto ctc = this->ctc();
	auto trie = this->trie();
	//OscarSearchSgIndex
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
	
	//OscarSearchSgIndex::Trie
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
operator<<(sserialize::UByteArrayAdapter & dest, OscarSearchSgIndex::Entry const & entry) {
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
				OscarSearchSgIndex::QueryTypeData const & qtd = entry.at(x);
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
//END OscarSearchSgIndex

}//end namespace hic
