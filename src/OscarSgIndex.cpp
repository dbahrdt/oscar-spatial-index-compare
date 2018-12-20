#include "OscarSgIndex.h"

#include "HtmSpatialGrid.h"
#include <boost/range/adaptor/map.hpp>

namespace hic {
namespace {
class WorkerData {
public:
	WorkerData(WorkerData const &) = default;
	WorkerData(OscarSgIndex::TrixelId trixelId, OscarSgIndex::CellId cellId, OscarSgIndex::ItemId itemId) :
	trixelId(trixelId),
	cellId(cellId),
	itemId(itemId)
	{}
	bool operator==(WorkerData const & other) const {
		return trixelId == other.trixelId && cellId == other.cellId && itemId == other.itemId;
	}
public:
	OscarSgIndex::TrixelId trixelId;
	OscarSgIndex::CellId cellId;
	OscarSgIndex::ItemId itemId;
};

}}//end namespace hic

namespace std {
	template<>
	struct hash<hic::WorkerData> {
		hash<hic::OscarSgIndex::TrixelId> trixelIdHash;
		hash<hic::OscarSgIndex::CellId> cellIdHash;
		hash<hic::OscarSgIndex::ItemId> itemIdHash;
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
	
OscarSgIndex::OscarSgIndex(Store const & store, IndexStore const & idxStore, sserialize::RCPtrWrapper<interface::SpatialGrid> const & sg) :
m_store(store),
m_idxStore(idxStore),
m_sg(sg)
{}

OscarSgIndex::~OscarSgIndex() {}

void OscarSgIndex::create(uint32_t threadCount) {
	m_td.clear();
	m_ctm.clear();
	m_ctm.resize(m_store.geoHierarchy().cellSize());
	
	struct State {
		sserialize::ProgressInfo pinfo;
		std::atomic<uint32_t> cellId{0};
		uint32_t cellCount;
		
		sserialize::Static::spatial::GeoHierarchy gh;
		sserialize::Static::spatial::TriangulationGeoHierarchyArrangement tr;
		
		OscarSgIndex * that;
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


void OscarSgIndex::stats() {
	std::cout << "OscarSgIndex::stats:" << std::endl;
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

}//end namespace hic
