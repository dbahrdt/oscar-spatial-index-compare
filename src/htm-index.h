#pragma once
#include <lsst/sphgeom/HtmPixelization.h>
#include <liboscar/StaticOsmCompleter.h>
#include <liboscar/AdvancedOpTree.h>
#include <sserialize/containers/ItemIndexFactory.h>
#include <sserialize/spatial/CellQueryResult.h>
#include <sserialize/containers/MMVector.h>
#include <unordered_map>
#include <map>
#include <limits>

namespace hic {

class OscarHtmIndex {
public:
	using Store = liboscar::Static::OsmKeyValueObjectStore;
	using IndexStore = sserialize::Static::ItemIndexStore;
	using ItemId = uint32_t;
	using CellId = uint32_t;
	using TrixelId = uint64_t;
	using TrixelCellItemMap = std::map<CellId, std::vector<ItemId>>;
	using CellTrixelMap = std::vector<std::set<TrixelId>>;
	using TrixelData = std::unordered_map<TrixelId, TrixelCellItemMap>;
public:
	///This assumes that the gh ist NOT refined and NOT split!
	OscarHtmIndex(Store const & store, IndexStore const & idxStore, int levels);
	virtual ~OscarHtmIndex();
public:
	void create(uint32_t threadCount);
public:
	void stats();
public:
	inline lsst::sphgeom::HtmPixelization const & htm() const { return m_hp; }
	///TrixelId->CellId->ItemId
	///maps trixel to the set of intersected cells and the items intersecting the cell and the trixel
	inline TrixelData const & trixelData() const { return m_td; }
	///CellId->TrixelId
	///Maps cells to trixels intersecting the cell
	inline CellTrixelMap const & cellTrixelMap() const { return m_ctm; }
private:
	Store m_store;
	IndexStore m_idxStore;
	lsst::sphgeom::HtmPixelization m_hp;
	TrixelData m_td;
	CellTrixelMap m_ctm;
};

class OscarSearchHtmIndex {
public:
	using Completer = liboscar::Static::OsmCompleter;
	using CellTextCompleter = sserialize::Static::CellTextCompleter;
	using TrieType = sserialize::Static::CellTextCompleter::FlatTrieType::TrieType;
	using TrixelId = uint32_t;
	using HtmIndexId = uint64_t;
	using IndexId = uint32_t;
	using ItemId = uint32_t;
	struct QueryTypeData {
		IndexId fmTrixels{std::numeric_limits<uint32_t>::max()};
		IndexId pmTrixels{std::numeric_limits<uint32_t>::max()};
		std::vector<IndexId> pmItems;
		bool valid() const;
	};
	struct Entry {
		std::array<QueryTypeData, 4> data;
		bool hasQueryType(sserialize::StringCompleter::QuerryType qt) const;
		QueryTypeData const & at(sserialize::StringCompleter::QuerryType qt) const;
		QueryTypeData & at(sserialize::StringCompleter::QuerryType qt);
		static std::size_t toPosition(sserialize::StringCompleter::QuerryType qt);
	};
	class TrixelIdMap {
	public:
		inline TrixelId trixelId(HtmIndexId htmIndex) const { return m_htmIndex2TrixelId.at(htmIndex); }
		inline HtmIndexId htmIndex(TrixelId trixelId) const { return m_trixelId2HtmIndex.at(trixelId); }
	public:
		std::unordered_map<HtmIndexId, TrixelId> m_htmIndex2TrixelId;
		std::vector<HtmIndexId> m_trixelId2HtmIndex;
	};
	enum FlusherType { FT_IN_MEMORY, FT_NO_OP};
public:
	OscarSearchHtmIndex(std::shared_ptr<Completer> cmp, std::shared_ptr<OscarHtmIndex> ohi);
public:
	void create(uint32_t threadCount, FlusherType ft = FT_IN_MEMORY);
	sserialize::UByteArrayAdapter & create(sserialize::UByteArrayAdapter & dest, uint32_t threadCount);
public:
	sserialize::UByteArrayAdapter & serialize(sserialize::UByteArrayAdapter & dest) const;
public:
	sserialize::ItemIndexFactory & idxFactory() { return m_idxFactory; }
public:
	std::shared_ptr<OscarHtmIndex> const & ohi() const { return m_ohi; }
	std::vector<IndexId> const & trixelItems() const { return m_trixelItems; }
	TrixelIdMap const & trixelIdMap() const { return m_trixelIdMap; }
	std::vector<Entry> const & data() const { return m_d; }
	TrieType trie() const;
	CellTextCompleter ctc() const;
private:
	struct State {
		std::atomic<uint32_t> strId{0};
		uint32_t strCount{0};
		std::vector<sserialize::StringCompleter::QuerryType> queryTypes;

		sserialize::Static::ItemIndexStore idxStore;
		sserialize::Static::spatial::GeoHierarchy gh;
		std::vector<uint32_t> trixelItemSize;
		TrieType trie;
		
		std::mutex flushLock;
		OscarSearchHtmIndex * that{0};
		
		sserialize::ProgressInfo pinfo;
	};
	struct Config {
		std::size_t workerCacheSize{1024};
	};
	class WorkerBase {
	public:
		using CellTextCompleter = sserialize::Static::CellTextCompleter;
		
		struct TrixelItems {
			struct Entry {
				TrixelId trixelId;
				ItemId itemId;
			};
			sserialize::MMVector<Entry> entries{sserialize::MM_SHARED_MEMORY};
			void add(TrixelId trixelId, ItemId itemId);
			template<typename TItemIdIterator>
			void add(TrixelId trixelId, TItemIdIterator begin, TItemIdIterator end) {
				for(; begin != end; ++begin) {
					add(trixelId, *begin);
				}
			}
			void clear();
			void process();
		};
	public:
		WorkerBase(State * state, Config * cfg);
		WorkerBase(const WorkerBase & other);
		virtual ~WorkerBase() {}
	public:
		void operator()();
	protected:
		virtual void flush(uint32_t strId, Entry && entry) = 0;
	protected:
		inline State & state() { return *m_state; }
		inline Config & cfg() { return *m_cfg; }
	private:
		void process(uint32_t strId, sserialize::StringCompleter::QuerryType qt);
		void flush(uint32_t strId, sserialize::StringCompleter::QuerryType qt);
		void flush(uint32_t strId);
		TrixelItems & trixel2Items(sserialize::StringCompleter::QuerryType qt);
	private:
		std::array<TrixelItems, 4> buffer;
		std::vector<uint32_t> itemIdBuffer;
		Entry m_bufferEntry;
	private:
		State * m_state;
		Config * m_cfg;
	};
	class InMemoryFlusher: public WorkerBase {
	public:
		InMemoryFlusher(State * state, Config * cfg);
		InMemoryFlusher(InMemoryFlusher const & other);
		virtual ~InMemoryFlusher() override;
	public:
		virtual void flush(uint32_t strId, Entry && entry) override;
	};
	class NoOpFlusher: public WorkerBase {
	public:
		NoOpFlusher(State * state, Config * cfg);
		NoOpFlusher(InMemoryFlusher const & other);
		virtual ~NoOpFlusher() override;
	public:
		virtual void flush(uint32_t strId, Entry && entry) override;
	};
	
	struct SerializationState {
		std::mutex lock;
		sserialize::Static::ArrayCreator<sserialize::UByteArrayAdapter> ac;
		int64_t lastPushedEntry{-1}; //this way we don't have to explicitly check for 0
		std::map<uint32_t, sserialize::UByteArrayAdapter> queuedEntries;
		
		SerializationState(sserialize::UByteArrayAdapter & dest) : ac(dest) {}
	};
	
	class SerializationFlusher: public WorkerBase {
	public:
		SerializationFlusher(SerializationState * sstate, State * state, Config * cfg);
		SerializationFlusher(const SerializationFlusher & other);
		virtual ~SerializationFlusher() override {}
	public:
		virtual void flush(uint32_t strId, Entry && entry) override;
	protected:
		inline SerializationState & sstate() { return *m_sstate; }
	private:
		SerializationState * m_sstate;
	};
private:
	void computeTrixelItems();
private:
	std::shared_ptr<Completer> m_cmp;
	std::shared_ptr<OscarHtmIndex> m_ohi;
	TrixelIdMap m_trixelIdMap;
	std::vector<IndexId> m_trixelItems;
	sserialize::ItemIndexFactory m_idxFactory;
	std::vector<Entry> m_d; //maps from stringId to Entry;
};


sserialize::UByteArrayAdapter & operator<<(sserialize::UByteArrayAdapter & other, OscarSearchHtmIndex::Entry const & entry);

class OscarSearchHtmIndexCellInfo: public sserialize::interface::CQRCellInfoIface {
public:
	using RCType = sserialize::RCPtrWrapper<sserialize::interface::CQRCellInfoIface>;
public:
	OscarSearchHtmIndexCellInfo(std::shared_ptr<OscarSearchHtmIndex> & d);
	virtual ~OscarSearchHtmIndexCellInfo() override;
public:
	static RCType makeRc(std::shared_ptr<OscarSearchHtmIndex> & d);
public:
	virtual SizeType cellSize() const override;
	virtual sserialize::spatial::GeoRect cellBoundary(CellId cellId) const override;
	virtual SizeType cellItemsCount(CellId cellId) const override;
	virtual IndexId cellItemsPtr(CellId cellId) const override;
private:
	std::shared_ptr<OscarSearchHtmIndex> m_d;
};

class OscarSearchWithHtm {
public:
	OscarSearchWithHtm(std::shared_ptr<OscarSearchHtmIndex> d);
	~OscarSearchWithHtm();
public:
	sserialize::CellQueryResult complete(std::string const & qstr, sserialize::StringCompleter::QuerryType qt);
	sserialize::CellQueryResult complete(liboscar::AdvancedOpTree const & optree);
private:
	sserialize::CellQueryResult process(const liboscar::AdvancedOpTree::Node * node);
private:
	std::shared_ptr<OscarSearchHtmIndex> m_d;
	sserialize::Static::ItemIndexStore m_idxStore;
	OscarSearchHtmIndexCellInfo::RCType m_ci;
};

} //end namespace hic
