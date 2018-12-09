#pragma once
#include <lsst/sphgeom/HtmPixelization.h>
#include <liboscar/StaticOsmCompleter.h>
#include <liboscar/AdvancedOpTree.h>
#include <sserialize/containers/ItemIndexFactory.h>
#include <sserialize/spatial/CellQueryResult.h>
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
	struct QueryTypeData {
		IndexId fmTrixels{std::numeric_limit<uint32_t>::max()};
		IndexId pmTrixels{std::numeric_limit<uint32_t>::max()};
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
public:
	OscarSearchHtmIndex(std::shared_ptr<Completer> cmp, std::shared_ptr<OscarHtmIndex> ohi);
public:
	void create(uint32_t threadCount);
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
	sserialize::CellQueryResult complete(std::string const & qstr, const sserialize::StringCompleter::QuerryType qt);
	sserialize::CellQueryResult complete(liboscar::AdvancedOpTree const & optree);
private:
	sserialize::CellQueryResult process(const liboscar::AdvancedOpTree::Node * node);
private:
	std::shared_ptr<OscarSearchHtmIndex> m_d;
	sserialize::Static::ItemIndexStore m_idxStore;
	OscarSearchHtmIndexCellInfo::RCType m_ci;
};

} //end namespace hic
