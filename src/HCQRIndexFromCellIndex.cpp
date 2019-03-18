#include "HCQRIndexFromCellIndex.h"

namespace hic {
namespace detail::HCQRIndexFromCellIndex {
namespace impl {

SpatialGridInfoFromCellIndex::SpatialGridInfoFromCellIndex(SpatialGridPtr const & sg, CellInfoPtr const & ci) :
m_sg(sg),
m_ci(ci)
{}

SpatialGridInfoFromCellIndex::SizeType
SpatialGridInfoFromCellIndex::itemCount(PixelId pid) const {
    return items(pid).size();
}

SpatialGridInfoFromCellIndex::ItemIndex
SpatialGridInfoFromCellIndex::items(PixelId pid) const {
    if (m_cache.count(pid)) {
        return m_cache.find(pid);
    }
    struct Recurser {
        SpatialGridInfoFromCellIndex const & that;
        Recurser(SpatialGridInfoFromCellIndex const & that) : that(that) {}
        ItemIndex operator()(PixelId pid) {
            if (that.m_ci->hasPixel(pid)) {
                return that.m_ci->items(pid);
            }
            else if (that.m_sg->level(pid) < that.m_ci->level()) {
                std::vector<ItemIndex> tmp;
                auto numChildren = that.m_sg->childrenCount(pid);
                for(decltype(numChildren) i(0); i < numChildren; ++i) {
                    tmp.emplace_back( (*this)(that.m_sg->index(pid, i) );
                }
                return ItemIndex::unite(tmp);
            }
            else {
                return ItemIndex();
            }
        }
    };
    Recurser rec(*this);
    ItemIndex result = rec(pid);
    m_cache.insert(pid, result);
    return result;
}

}//end namespace impl

}//end namespace detail::HCQRIndexFromCellIndex

}//end namespace
