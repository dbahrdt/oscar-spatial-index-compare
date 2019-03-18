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


class HCQRIndexFromCellIndex: public interface::HCQRIndex {
public:
    using CellIndex = hic::detail::HCQRIndexFromCellIndex::interface::CellIndex;
    using CellIndexPtr = sserialize::RCPtrWrapper<CellIndex>

    using SpatialGridPtr = sserialize::RCPtrWrapper<SpatialGridPtr>;
    using SpatialGridInfoPtr = sserialize::RCPtrWrapper<SpatialGridInfo>;

    using PixelId = SpatialGrid::PixelId;
public:
    HCQRIndexFromCellIndex(
        SpatialGridPtr const & sg,
        SpatialGridInfoPtr const & sgi,
        CellIndexPtr const & ci
    );
    virtual ~HCQRIndexFromCellIndex();
public:
    sserialize::StringCompleter::SupportedQuerries getSupportedQueries() const;
	HCQRPtr complete(const std::string & qstr, const sserialize::StringCompleter::QuerryType qt) const override;
	HCQRPtr items(const std::string & qstr, const sserialize::StringCompleter::QuerryType qt) const override;
	HCQRPtr regions(const std::string & qstr, const sserialize::StringCompleter::QuerryType qt) const override;
public:
	SpatialGridInfo const & sgi() const override;
	SpatialGrid const & sg() const override;
public:
    CellIndex const & ci() const;

}//end namespace
