#pragma once
#include <sserialize/utility/refcounting.h>
#include <sserialize/containers/ItemIndex.h>
#include <sserialize/Static/ItemIndexStore.h>
#include "SpatialGrid.h"

namespace hic {
namespace interface {

class HCQR: public sserialize::RefCountObject {
public:
    using Self = HCQR;
    using SizeType = uint32_t;
    using OperatorReturnValue = sserialize::RCPtrWrapper<Self>;
public:
    HCQR();
    virtual ~HCQR();
public:
    virtual SizeType depth() const;
    virtual SizeType numberOfItems() const;
public:
    virtual OperatorReturnValue operator/(Self const & other) const = 0;
    virtual OperatorReturnValue operator+(Self const & other) const = 0;
    virtual OperatorReturnValue operator-(Self const & other) const = 0;
public:
    ///@param maxPMLevel the highest level up to which merging of partial-match nodes should be considered
    virtual OperatorReturnValue compactified(SizeType maxPMLevel) const = 0;
    virtual OperatorReturnValue allToFull() const = 0;
};

}//end namespace interface

namespace interface {

class SpatialGridInfo: public sserialize::RefCountObject {
public:
    using PixelId = SpatialGrid::PixelId;
    using CompressedPixelId = hic::interface::SpatialGrid::CompressedPixelId;
    using ItemIndexId = uint32_t;
    using SizeType = uint32_t;
    using ItemIndex = sserialize::ItemIndex;
public:
    SpatialGridInfo();
    virtual ~SpatialGridInfo();
    virtual ItemIndexId itemIndexId(PixelId pid) const = 0;
public:
    virtual CompressedPixelId cPixelId(PixelId pixelId) const = 0;
    virtual PixelId pixelId(CompressedPixelId cPixelId) const = 0;
};

}//end namespace

namespace impl {

///In memory variant
class HCQRSpatialGrid: public interface::HCQR {
public:
    using PixelId = hic::interface::SpatialGridInfo::PixelId;
    using CompressedPixelId = hic::interface::SpatialGridInfo::CompressedPixelId;
    using ItemIndexId = uint32_t;
    using ItemIndex = sserialize::ItemIndex;
    using Parent = interface::HCQR;
    using Self = HCQRSpatialGrid;
public:
    HCQRSpatialGrid(
        sserialize::Static::ItemIndexStore idxStore,
        sserialize::RCPtrWrapper<hic::interface::SpatialGrid> sg,
        sserialize::RCPtrWrapper<hic::interface::SpatialGridInfo> sgi
    );
    template<typename T_PM_ITEM_INDEX_ID_ITERATOR>
    HCQRSpatialGrid(
        sserialize::ItemIndex const & fmCells,
        sserialize::ItemIndex const & pmCells,
        T_PM_ITEM_INDEX_ID_ITERATOR pmIndexIt,
        sserialize::Static::ItemIndexStore idxStore,
        sserialize::RCPtrWrapper<hic::interface::SpatialGrid> sg,
        sserialize::RCPtrWrapper<hic::interface::SpatialGridInfo> sgi
    );
    virtual ~HCQRSpatialGrid();
public:
    SizeType depth() const override;
    SizeType numberOfItems() const override;
public:
    OperatorReturnValue operator/(Parent::Self const & other) const override;
    OperatorReturnValue operator+(Parent::Self const & other) const override;
    OperatorReturnValue operator-(Parent::Self const & other) const override;
public:
    OperatorReturnValue compactified(SizeType maxPMLevel = 0) const override;
    OperatorReturnValue allToFull() const override;
private:
    /**
     * We assume the following: 
     * Internal nodes may contain items as well
     * BUT it shall not be necessary to include them in set operations of children
     * EXCEPT if the node does NOT have children
     * 
     */
    class TreeNode {
    public:
        using Children = std::vector<std::unique_ptr<TreeNode>>;
        enum : int {HAS_INDEX=0x1, IS_FULL_MATCH=0x2|HAS_INDEX, IS_FETCHED=0x4|HAS_INDEX } Flags;
    public:
        TreeNode();
        TreeNode(TreeNode const &) = delete;
        ~TreeNode();
        std::unique_ptr<TreeNode> shallowCopy() const; 
    public:
        static std::unique_ptr<TreeNode> make_unique(PixelId pixelId, int flags = 0, uint32_t itemIndexId = 0);
    public:
        inline PixelId pixelId() const { return m_pid; }
        inline bool fullMatch() const { return m_f & IS_FULL_MATCH; }
        inline bool isFetched() const { return m_f & IS_FETCHED; }
        inline bool hasIndex() const { return m_f & HAS_INDEX; }

        inline uint32_t itemIndexId() const { return m_itemIndexId; }
        ///children HAVE to be sorted according to their pixelId
        inline Children const & children() const { return m_children; }
        inline Children & children() { return m_children; }
    private:
        PixelId m_pid;
        int m_f;
        uint32_t m_itemIndexId;
        Children m_children;
    };
    struct HCQRSpatialGridOpHelper;
private:
    sserialize::ItemIndex items(TreeNode const & node) const;
private:
    auto const & items() const { return m_items; }
    auto const & fetchedItems() const { return m_fetchedItems; }
    auto const & sg() const { return *m_sg; }
    auto const & sgi() const { return *m_sgi; } 
private:
    std::unique_ptr<TreeNode> m_root;
    sserialize::Static::ItemIndexStore m_items;
    std::vector<sserialize::ItemIndex> m_fetchedItems;
    sserialize::RCPtrWrapper<hic::interface::SpatialGrid> m_sg;
    sserialize::RCPtrWrapper<hic::interface::SpatialGridInfo> m_sgi;
};

} //end namespace impl

}//end namespace hic

//Implementation of template functions
namespace hic::impl {


template<typename T_PM_ITEM_INDEX_ID_ITERATOR>
HCQRSpatialGrid::HCQRSpatialGrid(
    sserialize::ItemIndex const & fmCells,
    sserialize::ItemIndex const & pmCells,
    T_PM_ITEM_INDEX_ID_ITERATOR pmIndexIt,
    sserialize::Static::ItemIndexStore idxStore,
    sserialize::RCPtrWrapper<hic::interface::SpatialGrid> sg,
    sserialize::RCPtrWrapper<hic::interface::SpatialGridInfo> sgi
) :
HCQRSpatialGrid(idxStore, sg, sgi)
{

    std::unordered_map<PixelId, std::unique_ptr<TreeNode>> clevel;
    clevel.reserve(fmCells.size()+pmCells.size());
    for(uint32_t x : fmCells) {
        PixelId pId = this->sgi().pixelId( CompressedPixelId(x) );
        clevel[pId] = std::make_unique<TreeNode>(pId, true);
    }
    for(auto it(pmCells.begin()), end(pmCells.end()); it != end; ++it, ++pmIndexIt) {
        PixelId pId = this->sgi().pixelId( CompressedPixelId(*it) );
        clevel[pId] = std::make_unique<TreeNode>(pId, false, false,*pmIndexIt);
    }
    while (clevel.size() > 1) {
        std::unordered_map<PixelId, std::unique_ptr<TreeNode>> plevel;
        for(auto & x : clevel) {
            PixelId pPId = this->sg().parent( x.second->pixelId() );
            auto & parent = plevel[pPId];
            if (!parent) {
                parent = std::make_unique<TreeNode>(pPId);
            }
            parent->children().emplace_back( std::move(x.second) );
        }
        clevel = std::move(plevel);
    }
    m_root = std::move(clevel.begin()->second);
}


} //end namespace hic::impl