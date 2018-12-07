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
#include <boost/range/adaptor/map.hpp>
#include <tuple>

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
	
OscarHtmIndex::OscarHtmIndex(Store const & store, IndexStore const & idxStore, int levels) :
m_store(store),
m_idxStore(idxStore),
m_hp(levels)
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
		Worker(Worker const &) = default;
		
		
		void operator()() {
			while(true) {
				uint32_t cellId = state->cellId.fetch_add(1, std::memory_order_relaxed);
				if (cellId >= state->cellCount) {
					break;
				}
				
				process(cellId);
				
				if (m_tcd.size() > cfg->workerCacheSize) {
					flush();
				}
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
					auto tmp = item.payload().cells();
					std::set<uint32_t> itemCells(tmp.begin(), tmp.end());
					item.geoShape().visitPoints([this,itemId, &itemCells](const sserialize::Static::spatial::GeoPoint & p) {
						std::set<uint32_t> cellIds = this->state->tr.cellIds(p);
						auto htmIndex = this->state->that->m_hp.index(
							lsst::sphgeom::UnitVector3d(
								lsst::sphgeom::LonLat::fromDegrees(p.lon(), p.lat())
							)
						);
						if (!cellIds.size()) {
							cellIds.insert(0);
						}
						for(uint32_t myId : cellIds) {
							SSERIALIZE_CHEAP_ASSERT_NOT_EQUAL(myId, std::numeric_limits<uint32_t>::max());
							if (itemCells.count(myId)) {
								this->m_tcd.emplace(htmIndex, myId, itemId);
							}
						}
					});
				}
				else {
					SSERIALIZE_CHEAP_ASSERT_EQUAL(cellId, item.payload().cells().at(0));
					item.geoShape().visitPoints([this,cellId,itemId](const sserialize::Static::spatial::GeoPoint & p) {
						this->m_tcd.emplace(
							this->state->that->m_hp.index(
								lsst::sphgeom::UnitVector3d(
									lsst::sphgeom::LonLat::fromDegrees(p.lon(), p.lat())
								)
							),
							cellId,
							itemId
						);
					});
				}
			}
		}
		
		void flush() {
			std::lock_guard<std::mutex> lck(state->flushLock);
			for(Data const & x : m_tcd) {
				state->that->m_td[x.trixelId][x.cellId].emplace_back(x.itemId);
				if (x.cellId != std::numeric_limits<uint32_t>::max()) {
					state->that->m_ctm.at(x.cellId).insert(x.trixelId);
				}
				else {
					std::cout << "Item " << x.itemId << "is in invalid cell" << std::endl;
				}
			}
			#ifdef SSERIALIZE_EXPENSIVE_ASSERT_ENABLED
			{
				std::map<CellId, std::set<ItemId> > cellItems;
				for(const Data & x : m_tcd) {
					cellItems[x.cellId].insert(x.itemId);
				}
				for(const auto & x : cellItems) {
					sserialize::ItemIndex realCellItems = state->that->m_idxStore.at( state->gh.cellItemsPtr(x.first) );
					sserialize::ItemIndex myCellItems(std::vector<uint32_t>(x.second.begin(), x.second.end()));
					sserialize::ItemIndex diff = myCellItems - realCellItems;
					SSERIALIZE_EXPENSIVE_ASSERT_EQUAL(uint32_t(0), diff.size());
				}
			}
			#endif
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
	
	// #ifdef SSERIALIZE_EXPENSIVE_ASSERT_ENABLED
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
	// #endif
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
		trixelAreas.push_back( (12700/2)*(12700/2) * m_hp.triangle(x.first).getBoundingCircle().getArea() );
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


OscarSearchHtmIndex::OscarSearchHtmIndex(std::shared_ptr<Completer> cmp, std::shared_ptr<OscarHtmIndex> ohi) :
m_cmp(cmp),
m_ohi(ohi)
{}

void OscarSearchHtmIndex::create(uint32_t threadCount) {
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
	
	struct State {
		std::atomic<uint32_t> strId{0};
		uint32_t strCount;
		
		sserialize::Static::ItemIndexStore idxStore;
		sserialize::Static::spatial::GeoHierarchy gh;
		std::vector<uint32_t> trixelItemSize;
		TrieType trie;
		
		std::mutex flushLock;
		OscarSearchHtmIndex * that{0};
		
		sserialize::ProgressInfo pinfo;
	};
	
	struct Config {
		std::size_t workerCacheSize;
	};
	
	class Worker {
	public:
		using CellTextCompleter = sserialize::Static::CellTextCompleter;
	public:
		Worker(State * state, Config * cfg) : state(state), cfg(cfg) {}
		Worker(const Worker &) = default;
	public:
		void operator()() {
			while(true) {
				uint32_t strId = state->strId.fetch_add(1, std::memory_order_relaxed);
				if (strId >= state->strCount) {
					break;
				}
				process(strId);
				flush(strId);
				state->pinfo(state->strId);
			}
		};
		void process(uint32_t strId) {
			trixel2Items.clear();

			CellTextCompleter::Payload payload = state->trie.at(strId);
			CellTextCompleter::Payload::Type typeData = payload.type(sserialize::StringCompleter::QT_SUBSTRING);
			if (!typeData.valid()) {
				std::cerr << "Invalid trie payload data for string " << strId << " = " << state->trie.strAt(strId) << std::endl;
			}
			sserialize::ItemIndex fmCells = state->idxStore.at( typeData.fmPtr() );
			
			for(auto cellId : fmCells) {
				if (cellId >= state->that->m_ohi->cellTrixelMap().size()) {
					std::cerr << "Invalid cellId for string with id " << strId << " = " << state->trie.strAt(strId) << std::endl;
				}
				auto const & trixels = state->that->m_ohi->cellTrixelMap().at(cellId);
				for(HtmIndexId htmIndex : trixels) {
					TrixelId trixelId = state->that->m_trixelIdMap.trixelId(htmIndex);
					auto const & trixelCells = state->that->m_ohi->trixelData().at(htmIndex);
					auto const & trixelCellItems = trixelCells.at(cellId);
					trixel2Items[trixelId].insert(trixelCellItems.begin(), trixelCellItems.end());
				}
			}
			
			sserialize::ItemIndex pmCells = state->idxStore.at( typeData.pPtr() );
			auto itemIdxIdIt = typeData.pItemsPtrBegin();
			for(auto cellId : pmCells) {
				uint32_t itemIdxId = *itemIdxIdIt;
				sserialize::ItemIndex items = state->idxStore.at(itemIdxId);
				
				auto const & trixels = state->that->m_ohi->cellTrixelMap().at(cellId);
				for(HtmIndexId htmIndex : trixels) {
					TrixelId trixelId = state->that->m_trixelIdMap.trixelId(htmIndex);
					auto const & trixelCells = state->that->m_ohi->trixelData().at(htmIndex);
					sserialize::ItemIndex trixelCellItems( trixelCells.at(cellId) );
					sserialize::ItemIndex pmTrixelCellItems = items / trixelCellItems;
					trixel2Items[trixelId].insert(pmTrixelCellItems.begin(), pmTrixelCellItems.end());
				}
				
				++itemIdxIdIt;
			}
			
		}
		void flush(uint32_t strId) {
			SSERIALIZE_EXPENSIVE_ASSERT_EXEC(std::set<uint32_t> strItems)

			std::vector<TrixelId> fmTrixels;
			std::vector<TrixelId> pmTrixels;
			OscarSearchHtmIndex::Data & d = state->that->m_d.at(strId);
			for(auto & x : trixel2Items) {
				TrixelId trixelId = x.first;
				HtmIndexId htmIndex = state->that->m_trixelIdMap.htmIndex(trixelId);
				if (x.second.size() == state->trixelItemSize.at(trixelId)) { //fullmatch
					fmTrixels.emplace_back(trixelId);
				}
				else {
					pmTrixels.emplace_back(trixelId);
					d.pmItems.emplace_back(state->that->m_idxFactory.addIndex(x.second));
				}
				SSERIALIZE_EXPENSIVE_ASSERT_EXEC(strItems.insert(x.second.begin(), x.second.end()))
			}
			d.fmTrixels = state->that->m_idxFactory.addIndex(fmTrixels);
			d.pmTrixels = state->that->m_idxFactory.addIndex(pmTrixels);
			#ifdef SSERIALIZE_EXPENSIVE_ASSERT_ENABLED
			{
				auto cqr = state->that->ctc().complete(state->trie.strAt(strId), sserialize::StringCompleter::QT_SUBSTRING);
				sserialize::ItemIndex realItems = cqr.flaten();
				if (realItems != strItems) {
					cqr = state->that->ctc().complete(state->trie.strAt(strId), sserialize::StringCompleter::QT_SUBSTRING);
					std::cerr << std::endl << "OscarSearchHtmIndex: Items of entry " << strId << " = " << state->trie.strAt(strId) << " differ" << std::endl;
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
				SSERIALIZE_EXPENSIVE_ASSERT_EQUAL(strId, state->trie.find(state->trie.strAt(strId), false));
			}
			#endif
			trixel2Items.clear();
		}
	private:
		std::map<TrixelId, std::set<IndexId> > trixel2Items;
	private:
		State * state;
		Config * cfg;
	};
	
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
	
	cfg.workerCacheSize = 128*1024*1024/sizeof(uint64_t);
	
	m_d.resize(state.strCount);
	
	state.pinfo.begin(state.strCount, "OscarSearchHtmIndex: processing");
	if (threadCount == 1) {
		Worker(&state, &cfg)();
	}
	else {
		sserialize::ThreadPool::execute(Worker(&state, &cfg), threadCount, sserialize::ThreadPool::CopyTaskTag());
	}
	state.pinfo.end();
	
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
	auto htmIdx = m_d->trixelIdMap().htmIndex(cellId);
	lsst::sphgeom::Box box = m_d->ohi()->htm().triangle(htmIdx).getBoundingBox();
	auto lat = box.getLat();
	auto lon = box.getLon();
	return sserialize::spatial::GeoRect(lat.getA().asDegrees(), lat.getB().asDegrees(), lon.getA().asDegrees(), lon.getB().asDegrees());
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
OscarSearchWithHtm::complete(const std::string & qs, const sserialize::StringCompleter::QuerryType qt) {
	auto trie = m_d->trie();
	std::string qstr;
// 	if (m_sq & sserialize::StringCompleter::SQ_CASE_INSENSITIVE) {
		qstr = sserialize::unicode_to_lower(qs);
// 	}
// 	else {
// 		qstr = qs;
// 	}
	uint32_t pos = trie.find(qstr, (qt & sserialize::StringCompleter::QT_SUBSTRING || qt & sserialize::StringCompleter::QT_PREFIX));
	
	if (pos == trie.npos) {
		return sserialize::CellQueryResult();
	}
	
	const OscarSearchHtmIndex::Data & d = m_d->data().at(pos);
	
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
