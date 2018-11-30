#pragma once
#include <lsst/sphgeom/HtmPixelization.h>
#include <liboscar/StaticOsmCompleter.h>
#include <liboscar/AdvancedOpTree.h>
#include <sserialize/containers/ItemIndexFactory.h>
#include <sserialize/spatial/CellQueryResult.h>
#include <unordered_map>
#include <map>

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
	void create();
public:
	void stats();
public:
	inline lsst::sphgeom::HtmPixelization const & htm() const { return m_hp; }
	inline TrixelData const & trixelData() const { return m_td; }
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
	using TrieType = sserialize::Static::CellTextCompleter::FlatTrieType::TrieType;
	using TrixelId = uint32_t;
	using HtmIndexId = uint64_t;
	using IndexId = uint32_t;
	struct Data {
		IndexId fmTrixels;
		IndexId pmTrixels;
		std::vector<IndexId> pmItems;
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
	void create();
public:
	sserialize::ItemIndexFactory & idxFactory() { return m_idxFactory; }
public:
	std::shared_ptr<OscarHtmIndex> const & ohi() const { return m_ohi; }
	std::vector<IndexId> const & trixelItems() const { return m_trixelItems; }
	TrixelIdMap const & trixelIdMap() const { return m_trixelIdMap; }
	std::vector<Data> const & data() const { return m_d; }
	TrieType trie() const;
private:
	std::shared_ptr<Completer> m_cmp;
	std::shared_ptr<OscarHtmIndex> m_ohi;
	TrixelIdMap m_trixelIdMap;
	std::vector<IndexId> m_trixelItems;
	sserialize::ItemIndexFactory m_idxFactory;
	std::vector<Data> m_d; //maps from stringId to Data;
};

class OscarSearchHtmIndexCellInfo: public sserialize::interface::CQRCellInfoIface {
public:
	OscarSearchHtmIndexCellInfo(std::shared_ptr<OscarSearchHtmIndex> & d);
	virtual ~OscarSearchHtmIndexCellInfo() override;
public:
	static sserialize::RCPtrWrapper<sserialize::interface::CQRCellInfoIface> makeRc(std::shared_ptr<OscarSearchHtmIndex> & d);
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
};

} //end namespace hic
