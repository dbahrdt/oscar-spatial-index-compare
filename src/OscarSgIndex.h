#pragma once

#include <liboscar/StaticOsmCompleter.h>

#include "SpatialGrid.h"

namespace hic {
	
class OscarSgIndex {
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
	OscarSgIndex(Store const & store, IndexStore const & idxStore, sserialize::RCPtrWrapper<interface::SpatialGrid> const & sg);
	virtual ~OscarSgIndex();
public:
	void create(uint32_t threadCount);
public:
	void stats();
public:
	///TrixelId->CellId->ItemId
	///maps trixel to the set of intersected cells and the items intersecting the cell and the trixel
	inline TrixelData const & trixelData() const { return m_td; }
	///CellId->TrixelId
	///Maps cells to trixels intersecting the cell
	inline CellTrixelMap const & cellTrixelMap() const { return m_ctm; }
	inline interface::SpatialGrid const & sg() const { return *m_sg; }
private:
	Store m_store;
	IndexStore m_idxStore;
	sserialize::RCPtrWrapper<interface::SpatialGrid> m_sg;
	TrixelData m_td;
	CellTrixelMap m_ctm;
};
	
}//end namespace hic
