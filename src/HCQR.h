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
    virtual OperatorReturnValue compactified() const;
    virtual OperatorReturnValue allToFull() const;
};

}//end namespace interface

namespace interface {

class SpatialGridItems: public sserialize::RefCountObject {
public:
    using PixelId = SpatialGrid::PixelId;
    using ItemIndexId = uint32_t;
    using SizeType = uint32_t;
    using ItemIndex = sserialize::ItemIndex;
public:
    SpatialGridItems();
    virtual ~SpatialGridItems();
    virtual SizeType size(PixelId pid) const = 0;
    virtual ItemIndexId indexId(PixelId pid) const = 0;
    virtual ItemIndex items(PixelId pid) const = 0;
};

}//end namespace

namespace impl {

///In memory variant
class HCQRSpatialGrid: public interface::HCQR {
public:
    using PixelId = hic::interface::SpatialGrid::PixelId;
    using ItemIndexId = uint32_t;
    using ItemIndex = sserialize::ItemIndex;
    using Parent = interface::HCQR;
    using Self = HCQRSpatialGrid;
public:
    HCQRSpatialGrid(
        sserialize::Static::ItemIndexStore idxStore,
        sserialize::RCPtrWrapper<hic::interface::SpatialGrid> sg,
        sserialize::RCPtrWrapper<hic::interface::SpatialGridItems> sgi
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
    OperatorReturnValue compactified() const override;
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
    public:
        TreeNode();
        ~TreeNode();
    public:
        static std::unique_ptr<TreeNode> make_unique(PixelId pixelId, bool fullMatch = true, bool fetched = false, uint32_t itemIndexId = 0);
    public:
        PixelId pixelId() const;
        bool fullMatch() const;
        bool isFetched() const;
        uint32_t itemIndexId() const;
        ///children HAVE to be sorted according to their pixelId
        Children const & children() const;
        Children & children();
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
    std::unordered_map<ItemIndexId, sserialize::ItemIndex> m_fetchedItems;
    sserialize::RCPtrWrapper<hic::interface::SpatialGrid> m_sg;
    sserialize::RCPtrWrapper<hic::interface::SpatialGridItems> m_sgi;
};

} //end namespace impl

}//end namespace hic