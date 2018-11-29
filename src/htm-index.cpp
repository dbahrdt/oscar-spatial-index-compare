#include "htm-index.h"
#include <lsst/sphgeom/LonLat.h>
#include <sserialize/iterator/RangeGenerator.h>
#include <sserialize/mt/ThreadPool.h>
#include <sserialize/algorithm/hashspecializations.h>
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



void OscarHtmIndex::create() {
	m_td.clear();
	m_ctm.clear();
	m_ctm.resize(m_store.geoHierarchy().cellSize());
	
	struct State {
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
				uint32_t cellId = state->cellId.fetch_add(std::memory_order_relaxed);
				if (cellId > state->cellCount) {
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
			auto cell = state->gh.cell(cellId);
			auto cellIdx = state->that->m_idxStore.at( cell.itemPtr() );
		
			for(uint32_t itemId : cellIdx) {
				auto item = state->that->m_store.at(itemId);
				if (item.payload().cells().size() > 1) {
					item.geoShape().visitPoints([this,itemId](const sserialize::Static::spatial::GeoPoint & p) {
						this->m_tcd.emplace(
							this->state->that->m_hp.index(
								lsst::sphgeom::UnitVector3d(
									lsst::sphgeom::LonLat::fromDegrees(p.lon(), p.lat())
								)
							),
							this->state->tr.cellId(p),
							itemId
						);
					});
				}
				else {
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
				state->that->m_ctm.at(x.cellId).insert(x.trixelId);
			}
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
	
	sserialize::ThreadPool::execute(Worker(&state, &cfg), 0, sserialize::ThreadPool::CopyTaskTag());
	
	//Sort item ids
	for(auto & x : m_td) {
		for(auto & y : x.second) {
			std::sort(y.second.begin(), y.second.end());
		}
	}
	
}
	
	
}//end namespace hic
