#pragma once
#include <sserialize/utility/refcounting.h>
#include <sserialize/containers/ItemIndex.h>
#include <sserialize/Static/ItemIndexStore.h>
#include <sserialize/spatial/CellQueryResult.h>
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
	virtual SizeType numberOfNodes() const = 0;
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

namespace hic::impl::detail::HCQRSpatialGrid {

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
	enum : int {NONE=0x0, IS_INTERNAL=0x1, IS_PARTIAL_MATCH=0x2, IS_FULL_MATCH=0x4, IS_FETCHED=0x8} Flags;
	static constexpr uint32_t npos = std::numeric_limits<uint32_t>::max();
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
	static std::unique_ptr<TreeNode> make_unique(PixelId pixelId, int flags, uint32_t itemIndexId = npos);
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
public:
	bool valid() const;
private:
	TreeNode(PixelId pixelId, int flags, uint32_t itemIndexId);
private:
	PixelId m_pid;
	int m_f;
	uint32_t m_itemIndexId;
	Children m_children;
};
	
} //end namespace hic::impl::detail::HCQRSpatialGrid

namespace hic::impl {

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
    HCQRSpatialGrid(
		sserialize::CellQueryResult const & cqr,
        sserialize::Static::ItemIndexStore idxStore,
        sserialize::RCPtrWrapper<hic::interface::SpatialGrid> sg,
        sserialize::RCPtrWrapper<hic::interface::SpatialGridInfo> sgi
    );
    ~HCQRSpatialGrid() override;
public:
    SizeType depth() const override;
    SizeType numberOfItems() const override;
	SizeType numberOfNodes() const override;
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
