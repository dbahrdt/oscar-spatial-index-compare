#pragma once
#include <sserialize/utility/refcounting.h>
#include <sserialize/containers/ItemIndex.h>
#include <sserialize/Static/ItemIndexStore.h>
#include "SpatialGrid.h"

namespace hic::interface {

class HCQR: public sserialize::RefCountObject {
public:
    using Self = HCQR;
    using SizeType = uint32_t;
    using HCQRPtr = sserialize::RCPtrWrapper<Self>;
    using ItemIndex = sserialize::ItemIndex;
public:
    HCQR();
    virtual ~HCQR();
public:
    virtual SizeType depth() const = 0;
    virtual SizeType numberOfItems() const = 0;
    virtual ItemIndex items() const = 0;
public:
    virtual HCQRPtr operator/(Self const & other) const = 0;
    virtual HCQRPtr operator+(Self const & other) const = 0;
    virtual HCQRPtr operator-(Self const & other) const = 0;
public:
    ///@param maxPMLevel the highest level up to which merging of partial-match nodes should be considered
    ///note that the level of the root-node is 0.
    virtual HCQRPtr compactified(SizeType maxPMLevel) const = 0;
    ///param level up to which the tree should be expanded
    virtual HCQRPtr expanded(SizeType level) const = 0;
    virtual HCQRPtr allToFull() const = 0;
};

class SpatialGridInfo: public sserialize::RefCountObject {
public:
    using PixelId = SpatialGrid::PixelId;
	using CompressedPixelId = SpatialGrid::CompressedPixelId;
    using SizeType = uint32_t;
    using ItemIndex = sserialize::ItemIndex;
public:
    SpatialGridInfo() {}
    virtual ~SpatialGridInfo() {}
    virtual SizeType itemCount(PixelId pid) const = 0;
    virtual ItemIndex items(PixelId pid) const = 0;
	virtual PixelId pixelId(CompressedPixelId const & cpid) const = 0;
};

}//end namespace hic::interface

namespace hic::impl {
namespace detail::HCQRSpatialGrid {

/**
	* We assume the following: 
	* A Node is either an internal node and only has children OR a leaf node.
	* A Node is either a full-match node or a partial-match node
	* 
	*/
class TreeNode final {
public:
	using Children = std::vector<std::unique_ptr<TreeNode>>;
	using PixelId = hic::interface::SpatialGrid::PixelId;
	enum : int {IS_INTERNAL=0x0, IS_PARTIAL_MATCH=0x1, IS_FULL_MATCH=0x2, IS_FETCHED=0x4} Flags;
public:
	TreeNode();
	TreeNode(TreeNode const &) = delete;
	~TreeNode() {}
	//copies flags, pixelId and itemIndexId if IS_FETCHED is false 
	std::unique_ptr<TreeNode> shallowCopy() const; 
	//copies flags, pixelId if IS_FETCHED is true and sets the new fetchedItemIndexId 
	std::unique_ptr<TreeNode> shallowCopy(uint32_t fetchedItemIndexId) const;
	//copies flags, pixelId if isInternal() is true
	std::unique_ptr<TreeNode> shallowCopy(Children && newChildren) const; 
public:
	static std::unique_ptr<TreeNode> make_unique(PixelId pixelId, int flags = 0, uint32_t itemIndexId = 0);
public:
	inline PixelId pixelId() const { return m_pid; }
	inline bool isInternal() const { return children().size(); }
	inline bool isLeaf() const { return !children().size(); }
	inline bool isFullMatch() const { return m_f & IS_FULL_MATCH; }
	inline bool isFetched() const { return m_f & IS_FETCHED; }

	inline uint32_t itemIndexId() const { return m_itemIndexId; }
	///children HAVE to be sorted according to their pixelId
	inline Children const & children() const { return m_children; }
	inline int flags() const { return m_f; }
public:
	inline void setItemIndexId(uint32_t id) { m_itemIndexId = id; }
	inline void setFlags(int f) { m_f = f; }
	inline Children & children() { return m_children; }
private:
	TreeNode(PixelId pixelId, int flags, uint32_t itemIndexId);
private:
	PixelId m_pid;
	int m_f;
	uint32_t m_itemIndexId;
	Children m_children;
};
	
} //end namespace detail::HCQRSpatialGrid

///In memory variant
class HCQRSpatialGrid: public hic::interface::HCQR {
public:
    using PixelId = hic::interface::SpatialGridInfo::PixelId;
    using CompressedPixelId = hic::interface::SpatialGridInfo::CompressedPixelId;
    using ItemIndexId = uint32_t;
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
    ItemIndex items() const override;
public:
    HCQRPtr operator/(Parent::Self const & other) const override;
    HCQRPtr operator+(Parent::Self const & other) const override;
    HCQRPtr operator-(Parent::Self const & other) const override;
public:
    HCQRPtr compactified(SizeType maxPMLevel = 0) const override;
    HCQRPtr expanded(SizeType level) const override;
    HCQRPtr allToFull() const override;
private:
	using TreeNode = detail::HCQRSpatialGrid::TreeNode;
    struct HCQRSpatialGridOpHelper;
private:
    sserialize::ItemIndex items(TreeNode const & node) const;
private:
    sserialize::Static::ItemIndexStore const & idxStore() const { return m_items; }
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

} //end namespace hic::impl

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
        clevel[pId] = TreeNode::make_unique(pId, TreeNode::IS_FULL_MATCH);
    }
    for(auto it(pmCells.begin()), end(pmCells.end()); it != end; ++it, ++pmIndexIt) {
        PixelId pId = this->sgi().pixelId( CompressedPixelId(*it) );
        clevel[pId] = TreeNode::make_unique(pId, TreeNode::IS_PARTIAL_MATCH,*pmIndexIt);
    }
    while (clevel.size() > 1) {
        std::unordered_map<PixelId, std::unique_ptr<TreeNode>> plevel;
        for(auto & x : clevel) {
            PixelId pPId = this->sg().parent( x.second->pixelId() );
            auto & parent = plevel[pPId];
            if (!parent) {
                parent = TreeNode::make_unique(pPId);
            }
            parent->children().emplace_back( std::move(x.second) );
        }
        clevel = std::move(plevel);
        ///children have to be sorted according to their PixelId
        for(auto & x : clevel) {
            using std::sort;
            sort(x.second->children().begin(), x.second->children().end(),
                [](std::unique_ptr<TreeNode> const & a, std::unique_ptr<TreeNode> const & b) -> bool {
                    return a->pixelId() < b->pixelId();
                }
            );
        }
    }
    m_root = std::move(clevel.begin()->second);
}


} //end namespace hic::impl
