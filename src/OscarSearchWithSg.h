#pragma once

#include <liboscar/AdvancedOpTree.h>

#include "OscarSearchSgIndex.h"


namespace hic {

class OscarSearchSgIndexCellInfo: public sserialize::interface::CQRCellInfoIface {
public:
	using RCType = sserialize::RCPtrWrapper<sserialize::interface::CQRCellInfoIface>;
public:
	OscarSearchSgIndexCellInfo(std::shared_ptr<OscarSearchSgIndex> & d);
	virtual ~OscarSearchSgIndexCellInfo() override;
public:
	static RCType makeRc(std::shared_ptr<OscarSearchSgIndex> & d);
public:
	virtual SizeType cellSize() const override;
	virtual sserialize::spatial::GeoRect cellBoundary(CellId cellId) const override;
	virtual SizeType cellItemsCount(CellId cellId) const override;
	virtual IndexId cellItemsPtr(CellId cellId) const override;
private:
	std::shared_ptr<OscarSearchSgIndex> m_d;
};

class OscarSearchWithSg {
public:
	OscarSearchWithSg(std::shared_ptr<OscarSearchSgIndex> d);
	~OscarSearchWithSg();
public:
	sserialize::CellQueryResult complete(std::string const & qstr, sserialize::StringCompleter::QuerryType qt);
	sserialize::CellQueryResult complete(liboscar::AdvancedOpTree const & optree);
private:
	sserialize::CellQueryResult process(const liboscar::AdvancedOpTree::Node * node);
private:
	std::shared_ptr<OscarSearchSgIndex> m_d;
	sserialize::Static::ItemIndexStore m_idxStore;
	OscarSearchSgIndexCellInfo::RCType m_ci;
};

} //end namespace hic
