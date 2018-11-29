#pragma once
#include <lsst/sphgeom/HtmPixelization.h>
#include <liboscar/OsmKeyValueObjectStore.h>
#include <sserialize/Static/ItemIndexStore.h>
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
	OscarHtmIndex(Store const & store, IndexStore const & idxStore, int levels);
	virtual ~OscarHtmIndex();
public:
	void create();
public:
	void stats();
private:
	Store m_store;
	IndexStore m_idxStore;
	lsst::sphgeom::HtmPixelization m_hp;
	TrixelData m_td;
	CellTrixelMap m_ctm;
};

} //end namespace hic
