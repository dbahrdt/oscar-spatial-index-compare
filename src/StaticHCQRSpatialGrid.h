#pragma once

#include <sserialize/storage/UByteArrayAdapter.h>

#include "HCQR.h"

namespace hic::Static::detail::HCQRSpatialGrid {
	
///Improved variant: only store nextNode ptr for small distances
///For large distances one can simply explore the tree
class Tree;


class NodePosition final {
public:
	using SpatialGrid = hic::interface::SpatialGrid;
	static constexpr sserialize::UByteArrayAdapter::SizeType InvalidOffset = std::numeric_limits<sserialize::UByteArrayAdapter::SizeType>::max();
public:
	NodePosition();
	NodePosition(SpatialGrid::PixelId parent, sserialize::UByteArrayAdapter::SizeType dataOffset);
	~NodePosition();
public:
	SpatialGrid::PixelId parent() const;
	///Absolute data offset
	sserialize::UByteArrayAdapter::SizeType dataOffset() const;
	bool isRootNode() const;
	bool valid() const;
private:
	SpatialGrid::PixelId m_parent;
	sserialize::UByteArrayAdapter::SizeType m_do;
};

/**
 * Tree nodes are stored in-order. A node does not store a pointer to its children.
 * Since the tree is stored in-order, we know that the first child comes directly after the node.
 * We then store in each node either a pointer to its right sibling or if it is the last child, to its parent.
 * The Tree class provides convinience functions to create, modify and navigate such a tree.
 * 
 * 
 * struct TreeNode {
 *   u8  flags;
 *   u32 nextNode; //Relative position from the BEGINNING of this node to the beginning of the next node. In case of NEXT_NODE_IS_PARENT, this is a negative offset! since the parent comes before
 *   v32 childNumber = SpatialGrid::childPosition(parent.pixelId, this.pixelId); //not present on root node
 *   v32 itemIndexId; //present if flags & (HAS_INDEX)
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
	
	static constexpr sserialize::UByteArrayAdapter::SizeType MinimumDataSize = 5;
	static constexpr sserialize::UByteArrayAdapter::SizeType MaximumDataSize = 16;
public:
	TreeNode();
	///Directly initialize with ABSOLUTE pixelId
	TreeNode(PixelId pid, int flags, uint32_t itemIndexId = 0, uint32_t nextNodeOffset = 0, uint8_t padding = 0);
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
	bool isRoot() const;
	bool hasSibling() const;
	bool hasParent() const;

	uint32_t itemIndexId() const;
	int flags() const;

	///relative data offset, if hasParent(), then the offset should be interpreted to be a negative offset
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
	PixelId m_pid{std::numeric_limits<PixelId>::max()};
	uint32_t m_itemIndexId{0};
	uint32_t m_nextNodeOffset{0};
	uint8_t m_f{0};
	uint8_t m_padding{0}; //number of padding Bytes
};

/**
 * struct Tree {
 *   u32 dataSize;
 *   UByteArrayAdapter data;
 * };
 * 
 * The tree ist stored in-order.
 * 
 */

class Tree {
public:
	using SpatialGrid = hic::interface::SpatialGrid;
	using Node = TreeNode;
	using NodePosition = NodePosition;
	class NodeInfo final {
	public:
		NodeInfo();
		NodeInfo(Node const & node, NodePosition const & np);
		~NodeInfo();
	public:
		bool valid() const;
	public:
		Node & node();
		Node const & node() const;
		NodePosition const & position() const;
	private:
		Node m_n;
		NodePosition m_np;
	};
	class ChildrenIterator final {
	public:
		ChildrenIterator(Tree const & tree, NodeInfo const & ni);
		ChildrenIterator(Tree const & tree, NodePosition const & parent);
		~ChildrenIterator();
	public:
		void next();
		bool valid() const;
	public:
		Node const & node() const;
		NodePosition const & position() const;
		NodeInfo const & info() const;
	private:
		Tree const & m_tree;
		NodeInfo m_ni;
	};
	class MetaData {
	public:
		static constexpr sserialize::UByteArrayAdapter::SizeType StorageSize = 4;
	public:
		MetaData(MetaData && other);
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
		enum Positions : sserialize::UByteArrayAdapter::OffsetType {
			DATA_SIZE=0,
			__END=4
		};
	private:
		sserialize::UByteArrayAdapter m_d;
	};
public:
	Tree(Tree && other);
	Tree(sserialize::UByteArrayAdapter const & data, sserialize::RCPtrWrapper<SpatialGrid> const & sg);
	~Tree();
	///Create at dest.putPtr()
	static Tree create(sserialize::UByteArrayAdapter dest, sserialize::RCPtrWrapper<SpatialGrid> const & sg);
	///Create at dest.putPtr()
	static Tree create(sserialize::UByteArrayAdapter dest, hic::impl::HCQRSpatialGrid const & src);
public:
	sserialize::UByteArrayAdapter::OffsetType getSizeInBytes() const;
	sserialize::UByteArrayAdapter data() const;
	bool hasNodes() const;
public:
	NodePosition begin() const;
	NodePosition end() const;
	NodePosition rootNodePosition() const;
	ChildrenIterator children(NodePosition const & np) const;
	ChildrenIterator children(NodeInfo const & ni) const;
	NodePosition nextNode(NodePosition const & np) const;
	NodeInfo nextNode(NodeInfo const & ni) const;
	NodePosition firstChild(NodePosition const & np) const;
	NodeInfo firstChild(NodeInfo const & ni) const;
	NodePosition parent(NodePosition const & np) const;
	NodeInfo parent(NodeInfo const & ni) const;
	Node node(NodePosition const & np) const;
	NodeInfo nodeInfo(NodePosition const & np) const;
public: //modifying access
	NodePosition push(Node const & node);
	template<typename... Args>
	NodePosition push(Args... args) { return push(Node(std::forward<Args>(args)...)); }
	void pop(NodePosition pos);
	///Throws TypeOverflowException if update is not possible due to space restrictions
	void update(NodePosition const & pos, Node const & node);
public:
	void updateNextNode(NodePosition const & target, NodePosition const & nextNodePosition);
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
	using TreeNodeInfo = Tree::NodeInfo;
	
	using Node = Tree::Node;
	using NodePosition = Tree::NodePosition;
	using NodeInfo = Tree::NodeInfo;
public:
    HCQRSpatialGrid(hic::impl::HCQRSpatialGrid const & other);
    HCQRSpatialGrid(
		sserialize::UByteArrayAdapter const & data,
        sserialize::Static::ItemIndexStore idxStore,
        sserialize::RCPtrWrapper<hic::interface::SpatialGrid> sg,
        sserialize::RCPtrWrapper<hic::interface::SpatialGridInfo> sgi
    );
    HCQRSpatialGrid(
		Tree && tree,
        sserialize::Static::ItemIndexStore idxStore,
		std::vector<sserialize::ItemIndex> fetchedItems,
        sserialize::RCPtrWrapper<hic::interface::SpatialGrid> sg,
        sserialize::RCPtrWrapper<hic::interface::SpatialGridInfo> sgi
    );
	//empty one
	HCQRSpatialGrid(
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
    inline sserialize::Static::ItemIndexStore const & idxStore() const { return m_items; }
    inline auto const & fetchedItems() const { return m_fetchedItems; }
    inline auto const & sg() const { return *m_sg; }
    inline auto const & sgi() const { return *m_sgi; } 
    inline auto const & sgPtr() const { return m_sg; }
    inline auto const & sgiPtr() const { return m_sgi; } 
	inline Tree const & tree() const { return m_tree; }
private:
    struct OpHelper;
private:
    inline auto & fetchedItems() { return m_fetchedItems; }
	inline Tree & tree() { return m_tree; }
private:
    Tree m_tree;
    sserialize::Static::ItemIndexStore m_items;
    std::vector<sserialize::ItemIndex> m_fetchedItems;
    sserialize::RCPtrWrapper<hic::interface::SpatialGrid> m_sg;
    sserialize::RCPtrWrapper<hic::interface::SpatialGridInfo> m_sgi;
};


} //end namespace hic::impl
