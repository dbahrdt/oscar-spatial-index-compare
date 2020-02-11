#include "OscarSearchWithSg.h"
#include <sserialize/strings/unicode_case_functions.h>


namespace hic {

//BEGIN OscarSearchSgIndexCellInfo

OscarSearchSgIndexCellInfo::OscarSearchSgIndexCellInfo(std::shared_ptr<OscarSearchSgIndex> & d) :
m_d(d)
{}
OscarSearchSgIndexCellInfo::~OscarSearchSgIndexCellInfo()
{}


sserialize::RCPtrWrapper<sserialize::interface::CQRCellInfoIface>
OscarSearchSgIndexCellInfo::makeRc(std::shared_ptr<OscarSearchSgIndex> & d) {
	return sserialize::RCPtrWrapper<sserialize::interface::CQRCellInfoIface>( new OscarSearchSgIndexCellInfo(d) );
}

OscarSearchSgIndexCellInfo::SizeType
OscarSearchSgIndexCellInfo::cellSize() const {
	return m_d->trixelItems().size();
}
sserialize::spatial::GeoRect
OscarSearchSgIndexCellInfo::cellBoundary(CellId cellId) const {
	return m_d->ohi()->sg().bbox(m_d->trixelIdMap().htmIndex(cellId));
}

OscarSearchSgIndexCellInfo::SizeType
OscarSearchSgIndexCellInfo::cellItemsCount(CellId cellId) const {
	return m_d->idxFactory().idxSize(cellItemsPtr(cellId));
}

OscarSearchSgIndexCellInfo::IndexId
OscarSearchSgIndexCellInfo::cellItemsPtr(CellId cellId) const {
	return m_d->trixelItems().at(cellId);
}
	
//END

//BEGIN OscarSearchWithSg

OscarSearchWithSg::OscarSearchWithSg(std::shared_ptr<OscarSearchSgIndex> d) :
m_d(d),
m_idxStore(m_d->idxFactory().asItemIndexStore()),
m_ci(OscarSearchSgIndexCellInfo::makeRc(m_d))
{}

OscarSearchWithSg::~OscarSearchWithSg()
{}

sserialize::CellQueryResult
OscarSearchWithSg::complete(const std::string & qs, sserialize::StringCompleter::QuerryType qt) {
	auto trie = m_d->trie();
	std::string qstr;
	if (m_d->ctc().getSupportedQuerries() & sserialize::StringCompleter::SQ_CASE_INSENSITIVE) {
		qstr = sserialize::unicode_to_lower(qs);
	}
	else {
		qstr = qs;
	}
	uint32_t pos = trie.find(qstr, qt);
	
	if (pos == trie.npos) {
		return sserialize::CellQueryResult();
	}
	
	OscarSearchSgIndex::Entry const & e = m_d->data().at(pos);

	if (!e.hasQueryType(qt)) {
		if (qt & sserialize::StringCompleter::QT_SUBSTRING) {
			if (e.hasQueryType(sserialize::StringCompleter::QT_PREFIX)) { //exact suffix matches are either available or not
				qt = sserialize::StringCompleter::QT_PREFIX;
			}
			else if (e.hasQueryType(sserialize::StringCompleter::QT_SUFFIX)) {
				qt = sserialize::StringCompleter::QT_SUFFIX;
			}
			else if (e.hasQueryType(sserialize::StringCompleter::QT_EXACT)) {
				qt = sserialize::StringCompleter::QT_EXACT;
			}
		}
		else if (e.hasQueryType(sserialize::StringCompleter::QT_EXACT)) { //qt is either prefix, suffix, exact
			qt = sserialize::StringCompleter::QT_EXACT;
		}
	}
	
	
	if (!e.hasQueryType(qt)) {
		return sserialize::CellQueryResult();
	}

	OscarSearchSgIndex::QueryTypeData const & d = e.at(qt);

	return sserialize::CellQueryResult(
										m_idxStore.at(d.fmTrixels),
										m_idxStore.at(d.pmTrixels),
										d.pmItems.begin(),
										m_ci,
										m_idxStore,
										sserialize::CellQueryResult::FF_CELL_GLOBAL_ITEM_IDS);
}


sserialize::CellQueryResult
OscarSearchWithSg::complete(liboscar::AdvancedOpTree const & optree) {
	return process(optree.root());
}

sserialize::CellQueryResult
OscarSearchWithSg::process(const liboscar::AdvancedOpTree::Node * node) {
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
				throw sserialize::UnsupportedFeatureException("OscarSearchWithSg: item string query");
			}
			else if (node->subType == Node::STRING_REGION) {
				throw sserialize::UnsupportedFeatureException("OscarSearchWithSg: region string query");
			}
			else {
				return complete(qstr, qt);
			}
		}
		case Node::REGION:
			throw sserialize::UnsupportedFeatureException("OscarSearchWithSg: region query");
		case Node::REGION_EXCLUSIVE_CELLS:
			throw sserialize::UnsupportedFeatureException("OscarSearchWithSg: region exclusive cells");
		case Node::CELL:
			throw sserialize::UnsupportedFeatureException("OscarSearchWithSg: cell");
		case Node::CELLS:
			throw sserialize::UnsupportedFeatureException("OscarSearchWithSg: cells");
		case Node::RECT:
			throw sserialize::UnsupportedFeatureException("OscarSearchWithSg: rectangle");
		case Node::POLYGON:
			throw sserialize::UnsupportedFeatureException("OscarSearchWithSg: polygon");
		case Node::PATH:
			throw sserialize::UnsupportedFeatureException("OscarSearchWithSg: path");
		case Node::POINT:
			throw sserialize::UnsupportedFeatureException("OscarSearchWithSg: point");
		case Node::ITEM:
			throw sserialize::UnsupportedFeatureException("OscarSearchWithSg: item");
		default:
			break;
		};
		break;
	case Node::UNARY_OP:
		switch(node->subType) {
		case Node::FM_CONVERSION_OP:
			return process(node->children.at(0)).allToFull();
		case Node::CELL_DILATION_OP:
			throw sserialize::UnsupportedFeatureException("OscarSearchWithSg: cell dilation");
		case Node::REGION_DILATION_BY_ITEM_COVERAGE_OP:
		case Node::REGION_DILATION_BY_CELL_COVERAGE_OP:
			throw sserialize::UnsupportedFeatureException("OscarSearchWithSg: region dilation");
		case Node::COMPASS_OP:
			throw sserialize::UnsupportedFeatureException("OscarSearchWithSg: compass");
		case Node::IN_OP:
			throw sserialize::UnsupportedFeatureException("OscarSearchWithSg: in query");
		case Node::NEAR_OP:
			throw sserialize::UnsupportedFeatureException("OscarSearchWithSg: near query");
		case Node::RELEVANT_ELEMENT_OP:
			throw sserialize::UnsupportedFeatureException("OscarSearchWithSg: relevant item query");
		case Node::QUERY_EXCLUSIVE_CELLS:
			throw sserialize::UnsupportedFeatureException("OscarSearchWithSg: query exclusive cells");
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
			throw sserialize::UnsupportedFeatureException("OscarSearchWithSg: between query");
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
