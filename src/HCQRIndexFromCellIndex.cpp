#include "HCQRIndexFromCellIndex.h"

#include <sserialize/utility/debuggerfunctions.h>
#include <sserialize/containers/ItemIndexFactory.h>

namespace hic {
namespace detail::HCQRIndexFromCellIndex {
namespace impl {

SpatialGridInfoFromCellIndex::SpatialGridInfoFromCellIndex(SpatialGridPtr const & sg, CellInfoPtr const & ci) :
m_sg(sg),
m_ci(ci)
{
	m_cache.setSize(1 << 16);
	std::unordered_map<PixelId, std::vector<PixelId> > parent2Children;
	{
		std::vector<PixelId> queue;
		{
			std::vector<SpatialGrid::CompressedPixelId> cells = m_ci->cells();
			queue.reserve(cells.size());
			for(auto const & c : cells) {
				queue.push_back(m_ci->pixelId(c));
			}
		}
		
		auto rootPid = m_sg->rootPixelId();
		for(; queue.size();) {
			auto pid = queue.back();
			queue.pop_back();
			auto ppid = m_sg->parent(pid);
			if (ppid != rootPid && !parent2Children.count(ppid)) {
				queue.push_back(ppid);
			}
			parent2Children[ppid].push_back(pid);
		}
	}
	m_t.emplace_back(m_sg->rootPixelId());
	for(std::size_t i(0); i < m_t.size(); ++i) {
		if (!parent2Children.count(m_t.at(i).pid)) {
			SSERIALIZE_NORMAL_ASSERT(m_ci->hasPixel(m_t.at(i).pid));
			continue;
		}
		auto const & c = parent2Children.at(m_t.at(i).pid);
		m_t.at(i).begin = m_t.size();
		m_t.at(i).end = m_t.size()+c.size();
		for(auto x : c) {
			m_t.emplace_back(x);
		}
	}
}

SpatialGridInfoFromCellIndex::SizeType
SpatialGridInfoFromCellIndex::itemCount(PixelId pid) const {
    return items(pid).size();
}

SpatialGridInfoFromCellIndex::ItemIndex
SpatialGridInfoFromCellIndex::items(PixelId pid) const {
	std::unique_lock<std::mutex> cacheLck(m_cacheLock);
    if (m_cache.count(pid)) {
        return m_cache.find(pid);
    }
    cacheLck.unlock();
	sserialize::ItemIndex result;
	auto node = this->node(pid);
	if (node != tree().end()) {
		result = items(*node);
	}
	cacheLck.lock();
    m_cache.insert(pid, result);
	cacheLck.unlock();
	return result;
}


SpatialGridInfoFromCellIndex::Tree::const_iterator
SpatialGridInfoFromCellIndex::node(PixelId pid) const {
	std::vector<PixelId> ancestors;
	ancestors.push_back(pid);
	while(ancestors.back() != tree().front().pid) {
		ancestors.push_back(m_sg->parent(ancestors.back()));
	}
	auto node = tree().begin();
	for(auto rit(ancestors.rbegin()+1), rend(ancestors.rend()); rit != rend; ++rit) {
		for(std::size_t i(node->begin); i < node->end; ++i) {
			if (tree().at(i).pid == *rit) {
				node = tree().begin()+i;
				break;
			}
		}
		if (node->pid != *rit) {
			break;
		}
	}
	if (node->pid == pid) {
		return node;
	}
	else {
		return tree().end();
	}
}

sserialize::ItemIndex
SpatialGridInfoFromCellIndex::items(Node const & node) const {
	if (node.size()) {
		std::vector<sserialize::ItemIndex> tmp;
		tmp.reserve(node.size());
		for(auto i(node.begin); i < node.end; ++i) {
			tmp.push_back(items(tree().at(i)));
		}
		return sserialize::ItemIndex::unite(tmp);
	}
	else {
		return m_ci->items(node.pid);
	}
}

SpatialGridInfoFromCellIndex::Tree const &
SpatialGridInfoFromCellIndex::tree() const {
	return m_t;
}

SpatialGridInfoFromCellIndex::PixelId
SpatialGridInfoFromCellIndex::pixelId(CompressedPixelId const & cpid) const {
	return m_ci->pixelId(cpid);
}


SpatialGridInfoFromCellIndexWithIndex::SpatialGridInfoFromCellIndexWithIndex(SpatialGridPtr const & sg, CellInfoPtr const & ci) :
SpatialGridInfoFromCellIndex(sg, ci)
{
	sserialize::ItemIndexFactory idxFactory;
	idxFactory.setIndexFile(sserialize::UByteArrayAdapter(0, sserialize::MM_FAST_FILEBASED));
	idxFactory.setType(this->ci()->items(this->ci()->pixelId(CompressedPixelId(0))).type());
	idxFactory.setDeduplication(true);
	idxFactory.setCheckIndex(false);
	m_ti.resize(tree().size());
	
	struct Recurser {
		sserialize::ItemIndexFactory & idxF;
		SpatialGridInfoFromCellIndexWithIndex & that;
		sserialize::ItemIndex operator()(std::size_t nodePos) {
			auto const & node = that.tree().at(nodePos);
			sserialize::ItemIndex result;
			if (node.size()) {
				std::vector<sserialize::ItemIndex> tmp;
				tmp.reserve(node.size());
				for(auto i(node.begin); i < node.end; ++i) {
					tmp.push_back((*this)(i));
				}
				result = sserialize::ItemIndex::unite(tmp);
			}
			else {
				result = that.ci()->items(node.pid);
			}
			that.m_ti.at(nodePos) = idxF.addIndex(result);
			return result;
		}
		Recurser(sserialize::ItemIndexFactory & idxF, SpatialGridInfoFromCellIndexWithIndex & that) :
		idxF(idxF),
		that(that)
		{}
	};
	Recurser(idxFactory, *this)(0);
	idxFactory.flush();
	m_idxStore = sserialize::Static::ItemIndexStore(idxFactory.getFlushedData());
}


SpatialGridInfoFromCellIndexWithIndex::ItemIndex
SpatialGridInfoFromCellIndexWithIndex::items(PixelId pid) const {
	auto node = this->node(pid);
	if (node != this->tree().end()) {
		return m_idxStore.at(m_ti.at(node - this->tree().begin()));
	}
	else {
		return sserialize::ItemIndex();
	}
}

}//end namespace impl

}//end namespace detail::HCQRIndexFromCellIndex

HCQRIndexFromCellIndex::HCQRIndexFromCellIndex(
    SpatialGridPtr const & sg,
    SpatialGridInfoPtr const & sgi,
    CellIndexPtr const & ci
) :
m_sg(sg),
m_sgi(sgi),
m_ci(ci)
{}

HCQRIndexFromCellIndex::~HCQRIndexFromCellIndex() {}

sserialize::StringCompleter::SupportedQuerries
HCQRIndexFromCellIndex::getSupportedQueries() const {
    return m_ci->getSupportedQueries();
}

HCQRIndexFromCellIndex::HCQRPtr
HCQRIndexFromCellIndex::complete(const std::string & qstr, const sserialize::StringCompleter::QuerryType qt) const {
    sserialize::CellQueryResult cqr = ci().complete(qstr, qt);
    HCQRPtr result( new MyHCQR(cqr, cqr.idxStore(), m_sg, m_sgi) );
	SSERIALIZE_EXPENSIVE_ASSERT_EQUAL(cqr.flaten(), result->items());
	return result;
}

HCQRIndexFromCellIndex::HCQRPtr
HCQRIndexFromCellIndex::items(const std::string & qstr, const sserialize::StringCompleter::QuerryType qt) const {
    sserialize::CellQueryResult cqr = ci().items(qstr, qt);
    HCQRPtr result( new MyHCQR(cqr, cqr.idxStore(), m_sg, m_sgi) );
	SSERIALIZE_EXPENSIVE_ASSERT_EQUAL(cqr.flaten(), result->items());
	return result;
}

HCQRIndexFromCellIndex::HCQRPtr
HCQRIndexFromCellIndex::regions(const std::string & qstr, const sserialize::StringCompleter::QuerryType qt) const {
    sserialize::CellQueryResult cqr = ci().regions(qstr, qt);
    HCQRPtr result( new MyHCQR(cqr, cqr.idxStore(), m_sg, m_sgi) );
	SSERIALIZE_EXPENSIVE_ASSERT_EQUAL(cqr.flaten(), result->items());
	return result;
}

HCQRIndexFromCellIndex::HCQRPtr
HCQRIndexFromCellIndex::cell(uint32_t cellId) const {
    sserialize::CellQueryResult cqr = ci().cell(cellId);
    HCQRPtr result( new MyHCQR(cqr, cqr.idxStore(), m_sg, m_sgi) );
	SSERIALIZE_EXPENSIVE_ASSERT_EQUAL(cqr.flaten(), result->items());
	return result;
	
}

HCQRIndexFromCellIndex::HCQRPtr
HCQRIndexFromCellIndex::region(uint32_t regionId) const {
    sserialize::CellQueryResult cqr = ci().region(regionId);
    HCQRPtr result( new MyHCQR(cqr, cqr.idxStore(), m_sg, m_sgi) );
	SSERIALIZE_EXPENSIVE_ASSERT_EQUAL(cqr.flaten(), result->items());
	return result;
}

HCQRIndexFromCellIndex::SpatialGridInfo const &
HCQRIndexFromCellIndex::sgi() const {
    return *m_sgi;
}

HCQRIndexFromCellIndex::SpatialGrid const &
HCQRIndexFromCellIndex::sg() const {
    return *m_sg;
}

HCQRIndexFromCellIndex::CellIndex const &
HCQRIndexFromCellIndex::ci() const {
    return *m_ci;
}

}//end namespace
