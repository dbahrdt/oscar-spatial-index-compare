#pragma once

#include <sserialize/storage/UByteArrayAdapter.h>

#include "HCQR.h"

namespace hic::Static::detail::HCQRSpatialGrid {
	
class Tree;

///Improved variant: only store nextNode ptr for small distances
///For large distances one can simply explore the tree
///don't use absolute positions, but rather relative positions

/**
 * struct TreeNode {
 *   u8  flags;
 *   u32 nextNode; //Absolute position to the next node
 *   v32 childNumber; //Which child this is relative to the parent node (not present on root node)
 *   v32 itemIndexId; //present if flags & (IS_PARTIAL_MATCH | IS_FETCHED)
 *   u8 numberOfPaddingBytes; //present if flags & HAS_PADDING, defaults to zero
 *   u8[numberOfPaddingBytes] padding;
 * };
 * 
 */
	
class TreeNode {
public:
	using SpatialGrid = hic::interface::SpatialGrid;
	using PixelId = SpatialGrid::PixelId;
	enum : int {NONE=0x0,
		//Flags compatible with hic::impl::detail::HCQRSpatialGrid::TreeNode::Flags
		IS_INTERNAL=0x1, IS_PARTIAL_MATCH=0x2, IS_FULL_MATCH=0x4, IS_FETCHED=0x8,
		//Stuff not comptabile with hic::impl::detail::HCQRSpatialGrid::TreeNode::Flags
		HAS_PADDING=0x10,
		NEXT_NODE_IS_PARENT=0x20,
		IS_ROOT_NODE=0x40,
		HAS_INDEX=IS_PARTIAL_MATCH|IS_FETCHED,
		IS_LEAF=IS_PARTIAL_MATCH|IS_FULL_MATCH|IS_FETCHED
	} Flags;
	
	static constexpr sserialize::UByteArrayAdapter::SizeType MinimumDataSize = 6;
	static constexpr sserialize::UByteArrayAdapter::SizeType MaximumDataSize = 16;
public:
	///Directly initialize with ABSOLUTE pixelId
	TreeNode(PixelId pid, int flags, uint32_t itemIndexId, uint32_t nextNodeOffset = 0, uint8_t padding = 0);
	TreeNode(sserialize::UByteArrayAdapter const & data, PixelId parent, SpatialGrid const & sg);
	TreeNode(TreeNode const & other) = default;
	~TreeNode();
	sserialize::UByteArrayAdapter::OffsetType minSizeInBytes(SpatialGrid const & sg) const;
	///includes padding
	sserialize::UByteArrayAdapter::OffsetType getSizeInBytes(SpatialGrid const & sg) const;
public:
	PixelId pixelId() const;
	bool isInternal() const;
	bool isLeaf() const;
	bool isFullMatch() const;
	bool isFetched() const;
	bool hasSibling() const;
	bool hasParent() const;

	uint32_t itemIndexId() const;
	int flags() const;

	uint32_t nextNodeOffset() const;
public:
	void setPixelId(PixelId v);
	void setFlags(int v);
	void setItemIndexId(uint32_t v);
	void setNextNodeOffset(uint32_t v);
public:
	///update the given memory location with the new data encoding the tree pixelId relative to its parent
	static void update(sserialize::UByteArrayAdapter dest, TreeNode const & node, uint32_t targetSize, SpatialGrid const & sg);
private:
	uint8_t numberOfPaddingBytes() const;
private:
	PixelId m_pid;
	uint32_t m_itemIndexId;
	uint32_t m_nextNodeOffset;
	uint8_t m_f;
	uint8_t m_padding; //number of padding Bytes
};

/**
 * struct Tree {
 *   u32 dataSize;
 *   UByteArrayAdapter data;
 * };
 * 
 */

class Tree {
public:
	using SpatialGrid = hic::interface::SpatialGrid;
	using Node = TreeNode;
	class NodePosition final {
	public:
		NodePosition(SpatialGrid::PixelId parent, sserialize::UByteArrayAdapter::SizeType dataOffset);
		~NodePosition();
	public:
		SpatialGrid::PixelId parent() const;
		sserialize::UByteArrayAdapter::SizeType dataOffset() const;
		bool isRootNode() const;
	private:
		SpatialGrid::PixelId m_parent;
		sserialize::UByteArrayAdapter::SizeType m_do;
	};
	class ChildrenIterator final {
	public:
		ChildrenIterator(Tree const & tree, NodePosition const & parent);
		~ChildrenIterator();
	public:
		void next();
		bool valid() const;
	public:
		Node const & node() const;
		NodePosition const & position() const;
	private:
		Tree const & m_tree;
		Node m_n;
		NodePosition m_np;
	};
	class MetaData {
	public:
		static constexpr uint32_t StorageSize = 4;
	public:
		MetaData(sserialize::UByteArrayAdapter const & d);
		~MetaData();
	public:
		sserialize::UByteArrayAdapter::OffsetType dataSize() const;
	public:
		///return data to the whole tree
		sserialize::UByteArrayAdapter treeData() const;
	public:
		void setDataSize(uint32_t v);
	private:
		enum class Positions : uint32_t {
			DATA_SIZE=0
		};
	private:
		sserialize::UByteArrayAdapter m_d;
	};
public:
	Tree(sserialize::UByteArrayAdapter const & data, sserialize::RCPtrWrapper<SpatialGrid> const & sg);
	~Tree();
	///Create at dest.putPtr()
	Tree create(sserialize::UByteArrayAdapter dest, sserialize::RCPtrWrapper<SpatialGrid> const & sg);
	///Create at dest.putPtr()
	Tree create(sserialize::UByteArrayAdapter dest, hic::impl::HCQRSpatialGrid const & src);
public:
	sserialize::UByteArrayAdapter::OffsetType getSizeInBytes() const;
	sserialize::UByteArrayAdapter data() const;
	bool hasNodes() const;
public:
	NodePosition rootNodePosition() const;
	ChildrenIterator children(NodePosition const & np) const;
	NodePosition parent(Node const & node, NodePosition const & np) const;
	Node get(NodePosition const & np) const;
public: //modifying access
	NodePosition push(Node const & node);
	void pop(NodePosition pos);
	///Throws TypeOverflowException if update is not possible due to space restrictions
	void update(NodePosition pos, Node const & node);
private:
	sserialize::UByteArrayAdapter & nodeData();
	sserialize::UByteArrayAdapter const & nodeData() const;
public:
	MetaData m_md;
	sserialize::UByteArrayAdapter m_nd; //node data
	sserialize::RCPtrWrapper<SpatialGrid> m_sg;
};

}//end namespace hic::Static::detail::HCQRSpatialGrid

namespace hic::Static::impl {

class HCQRSpatialGrid: public hic::interface::HCQR {
public:
    using PixelId = hic::interface::SpatialGridInfo::PixelId;
    using CompressedPixelId = hic::interface::SpatialGridInfo::CompressedPixelId;
    using ItemIndexId = uint32_t;
    using Parent = interface::HCQR;
    using Self = HCQRSpatialGrid;
	using PixelLevel = hic::interface::SpatialGrid::Level;
	using Tree = hic::Static::detail::HCQRSpatialGrid::Tree;
	using TreeNode = Tree::Node;
	using TreeNodePosition = Tree::NodePosition;
public:
    HCQRSpatialGrid(
		sserialize::UByteArrayAdapter const & data,
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
public:
	TreeNode root() const;
	TreeNodePosition rootNodePosition() const;
public:
    sserialize::ItemIndex items(TreeNodePosition const & node) const;
	PixelLevel level(TreeNode const & node) const;
public:
    sserialize::Static::ItemIndexStore const & idxStore() const { return m_items; }
    auto const & fetchedItems() const { return m_fetchedItems; }
    auto const & sg() const { return *m_sg; }
    auto const & sgi() const { return *m_sgi; } 
    auto const & sgPtr() const { return m_sg; }
    auto const & sgiPtr() const { return m_sgi; } 
private:
    struct OpHelper;
private:
	Tree const & tree() const;
	Tree & tree() const;
private:
    Tree m_tree;
    sserialize::Static::ItemIndexStore m_items;
    std::vector<sserialize::ItemIndex> m_fetchedItems;
    sserialize::RCPtrWrapper<hic::interface::SpatialGrid> m_sg;
    sserialize::RCPtrWrapper<hic::interface::SpatialGridInfo> m_sgi;
};


} //end namespace hic::impl
