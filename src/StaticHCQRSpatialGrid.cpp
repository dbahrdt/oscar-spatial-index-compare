#include "StaticHCQRSpatialGrid.h"
#include <sserialize/storage/pack_unpack_functions.h>
#include <sserialize/utility/debuggerfunctions.h>
#include <sserialize/containers/ItemIndexFactory.h>

namespace hic::Static::detail::HCQRSpatialGrid {

//BEGIN NodePosition
	

NodePosition::NodePosition() :
m_do(InvalidOffset)
{}

NodePosition::NodePosition(SpatialGrid::PixelId parent, sserialize::UByteArrayAdapter::SizeType dataOffset) :
m_parent(parent),
m_do(dataOffset)
{}

NodePosition::~NodePosition() {}

NodePosition::SpatialGrid::PixelId
NodePosition::parent() const {
	SSERIALIZE_CHEAP_ASSERT(valid());
	return m_parent;
}

sserialize::UByteArrayAdapter::SizeType
NodePosition::dataOffset() const {
	SSERIALIZE_CHEAP_ASSERT(valid());
	return m_do;
}

bool
NodePosition::isRootNode() const {
	return m_do == 0;
}

bool
NodePosition::valid() const {
	return m_do != InvalidOffset;
}

//END NodePosition
	
//BEGIN TreeNode

TreeNode::TreeNode() {}

TreeNode::TreeNode(PixelId pid, int flags) :
m_pid(pid),
m_f(flags)
{}

TreeNode::TreeNode(PixelId pid, int flags, uint32_t itemIndexId) :
m_pid(pid),
m_itemIndexId(itemIndexId),
m_f(flags)
{
	SSERIALIZE_CHEAP_ASSERT(flags & HAS_INDEX);
}

TreeNode::TreeNode(PixelId pid, int flags, uint32_t itemIndexId, uint32_t nextNodeOffset, uint8_t padding) :
m_pid(pid),
m_itemIndexId(itemIndexId),
m_nextNodeOffset(nextNodeOffset),
m_f(flags),
m_padding(padding)
{}

TreeNode::TreeNode(sserialize::UByteArrayAdapter const & data, PixelId parent, SpatialGrid const & sg) {
	sserialize::UByteArrayAdapter::OffsetType p = 0;
	int len;
	
	m_f = data.getUint8(p);
	p += 1;
	
	m_nextNodeOffset = data.getUint32(p);
	p += sserialize::SerializationInfo<uint32_t>::length;
	
	if (m_f & IS_ROOT_NODE) {
		m_pid = sg.rootPixelId();
	}
	else {
		auto childPos = data.getVlPackedUint32(p, &len);
		SSERIALIZE_CHEAP_ASSERT_LARGER(len, 0);
		p += len;
		m_pid = sg.index(parent, childPos);
	}
	
	if (m_f & HAS_INDEX) {
		m_itemIndexId = data.getVlPackedUint32(p, &len);
		SSERIALIZE_CHEAP_ASSERT_LARGER(len, 0);
		p += len;
	}
	
	if (m_f & HAS_PADDING) {
		m_padding = data.getUint8(p);
		p += 1+m_padding;
	}
}

TreeNode::~TreeNode() {}

bool
TreeNode::valid() const {
	if (!m_f) {
		return false;
	}
	if (m_pid == SpatialGrid::NoPixelId) {
		return false;
	}
	if ( (m_f & HAS_INDEX) && m_itemIndexId == sserialize::Static::ItemIndexStore::npos) {
		return false;
	}
	if (!isRoot() && m_nextNodeOffset == std::numeric_limits<uint32_t>::max()) {
		return false;
	}
	if ((m_f & IS_INTERNAL) && (m_f & IS_LEAF)) {
		return false;
	}
	return true;
}

sserialize::UByteArrayAdapter::OffsetType
TreeNode::minSizeInBytes(SpatialGrid const & sg) const {
	sserialize::UByteArrayAdapter::OffsetType ds = 0;
	ds += sserialize::SerializationInfo<uint8_t>::length;
	ds += sserialize::SerializationInfo<uint32_t>::length;
	
	if (!(flags() & IS_ROOT_NODE)) {
		auto parent = sg.parent(m_pid);
		auto childPos = sg.childPosition(parent, pixelId());
		ds += sserialize::psize_v<uint32_t>(childPos);
	}
	if (flags() & HAS_INDEX) {
		ds += sserialize::psize_v<uint32_t>(itemIndexId());
	}
	//don't include padding
	return ds;
}

sserialize::UByteArrayAdapter::OffsetType
TreeNode::getSizeInBytes(SpatialGrid const & sg) const {
	auto ds = minSizeInBytes(sg);
	if (flags() & HAS_PADDING) {
		ds += sserialize::SerializationInfo<uint8_t>::length+m_padding;
	}
	return ds;
}

bool
TreeNode::operator==(TreeNode const & other) const {
	return m_pid == other.m_pid && m_itemIndexId == other.m_itemIndexId && m_nextNodeOffset == other.m_nextNodeOffset && m_f == other.m_f && m_padding == other.m_padding;
}

TreeNode::PixelId
TreeNode::pixelId() const {
	return m_pid;
}

bool
TreeNode::isInternal() const {
	return m_f & IS_INTERNAL;
}

bool
TreeNode::isLeaf() const {
	return m_f & IS_LEAF;
}

bool
TreeNode::isFullMatch() const {
	return m_f & IS_FULL_MATCH;
}

bool
TreeNode::isFetched() const {
	return m_f & IS_FETCHED;
}

bool
TreeNode::isRoot() const {
	return m_f & IS_ROOT_NODE;
}

bool
TreeNode::hasSibling() const {
	return !hasParent() && !isRoot();
}

bool
TreeNode::hasParent() const {
	return m_f & NEXT_NODE_IS_PARENT;
}

uint32_t
TreeNode::itemIndexId() const {
	return m_itemIndexId;
}

int
TreeNode::flags() const{
	return m_f;
}

uint32_t
TreeNode::nextNodeOffset() const {
	return m_nextNodeOffset;
}

void
TreeNode::update(sserialize::UByteArrayAdapter dest, TreeNode const & node, uint32_t targetSize, SpatialGrid const & sg) {
	auto minDataSize = node.minSizeInBytes(sg);
	
	if (minDataSize > targetSize) {
		throw sserialize::TypeOverflowException("Not enough space");
	}
	SSERIALIZE_CHEAP_ASSERT_SMALLER_OR_EQUAL(targetSize, TreeNode::MaximumDataSize);
	
	dest.resetPutPtr();
	
	int flags = node.flags() & (~HAS_PADDING);
	
	if (minDataSize < targetSize) {
		flags |= HAS_PADDING;
	}
	dest.putUint8(flags);
	dest.putUint32(node.nextNodeOffset());
	
	if (!node.isRoot()) {
		dest.putVlPackedUint32(sg.childPosition(sg.parent(node.pixelId()), node.pixelId()));
	}
	
	if (flags & HAS_INDEX) {
		dest.putVlPackedUint32(node.itemIndexId());
	}

	if (flags & HAS_PADDING) {
		dest.putUint8(targetSize - minDataSize - 1);
	}
}

void
TreeNode::setPixelId(PixelId v) {
	m_pid = v;
}

void
TreeNode::setFlags(int v) {
	m_f = v;
}

void
TreeNode::setItemIndexId(uint32_t v) {
	m_itemIndexId = v;
}

void
TreeNode::setNextNodeOffset(uint32_t v) {
	m_nextNodeOffset = v;
}

std::ostream &
operator<<(std::ostream & out, TreeNode const & node) {
	out << "hic::Static::detail::HCQRSpatialGrid::TreeNode(pid=" << node.pixelId() << ", flags=" << node.flags() << ", itemIndexId=" << node.itemIndexId() << ", nextNodeOffset=" << node.nextNodeOffset() << ", padding=?)";
	return out;
}

//END TreeNode

//BEGIN Tree::MetaData

Tree::MetaData::MetaData(MetaData && other) :
m_d(std::move(other.m_d))
{}

Tree::MetaData::MetaData(sserialize::UByteArrayAdapter const & d) :
m_d(d, 0, Positions::__END)
{}

Tree::MetaData::MetaData::~MetaData() {}

sserialize::UByteArrayAdapter::OffsetType
Tree::MetaData::dataSize() const {
	return m_d.getUint32( Positions::DATA_SIZE );
}

sserialize::UByteArrayAdapter
Tree::MetaData::treeData() const {
	sserialize::UByteArrayAdapter tmp(m_d);
	tmp.growStorage(dataSize());
	return tmp;
}

void
Tree::MetaData::setDataSize(uint32_t v) {
	m_d.putUint32(Positions::DATA_SIZE, v);
}

//END Tree::MetaData

//BEGIN Tree::NodeInfo

Tree::NodeInfo::NodeInfo() {}

Tree::NodeInfo::NodeInfo(Node const & node, NodePosition const & np) :
m_n(node),
m_np(np)
{}

Tree::NodeInfo::~NodeInfo() {}

bool
Tree::NodeInfo::valid() const {
	return position().valid();
}

Tree::Node &
Tree::NodeInfo::node() {
	return m_n;
}

Tree::Node const &
Tree::NodeInfo::node() const {
	return m_n;
}

Tree::NodePosition const &
Tree::NodeInfo::position() const {
	return m_np;
}

//END Tree::NodeInfo

//BEGIN Tree::ChildrenIterator

Tree::ChildrenIterator::ChildrenIterator(Tree const & tree, NodeInfo const & parent) :
m_tree(tree),
m_ni(tree.firstChild(parent))
{}

Tree::ChildrenIterator::ChildrenIterator(Tree const & tree, NodePosition const & parent) :
ChildrenIterator(tree, tree.nodeInfo(parent))
{}

Tree::ChildrenIterator::~ChildrenIterator()
{}

void
Tree::ChildrenIterator::next() {
	SSERIALIZE_CHEAP_ASSERT(m_ni.valid());
	if (info().node().hasSibling()) {
		m_ni = m_tree.nextNode(m_ni);
	}
	else {
		m_ni = NodeInfo();
	}
}

bool
Tree::ChildrenIterator::valid() const {
	return info().valid();
}

Tree::Node const &
Tree::ChildrenIterator::node() const {
	return info().node();
}

NodePosition const &
Tree::ChildrenIterator::position() const {
	return info().position();
}

Tree::NodeInfo const &
Tree::ChildrenIterator::info() const {
	return m_ni;
}

//END Tree::ChildrenIterator

//BEGIN Tree


Tree::Tree(Tree && other) :
m_md(std::move(other.m_md)),
m_nd(std::move(other.m_nd)),
m_sg(std::move(other.m_sg))
{}

Tree::Tree(sserialize::UByteArrayAdapter const & data, sserialize::RCPtrWrapper<SpatialGrid> const & sg) :
m_md(data),
m_nd(data+MetaData::StorageSize),
m_sg(sg)
{
	m_nd.setPutPtr(m_md.dataSize());
}

Tree::~Tree() {}

Tree
Tree::create(sserialize::UByteArrayAdapter dest, sserialize::RCPtrWrapper<SpatialGrid> const & sg) {
	dest.shrinkToPutPtr();
	dest.putUint32(0);
	dest.resetPtrs();
	return Tree(dest, sg);
}

namespace inlinetests {
	
struct TreeFromHCQRSpatialGrid {
	Tree const & tree;
	TreeFromHCQRSpatialGrid(Tree const & tree) : tree(tree) {}
	bool equalTopology(Tree::NodePosition const & np, hic::impl::HCQRSpatialGrid::TreeNode const & src) {
		Tree::Node node( tree.node(np) );
		
		if ((node.flags() & Tree::Node::__COMPATIBLE_FLAGS) != (src.flags() & Tree::Node::__COMPATIBLE_FLAGS)) {
			return false;
		}
		if (node.pixelId() != src.pixelId()) {
			return false;
		}
		if (node.isLeaf() != src.isLeaf()) {
			return false;
		}
		if (node.isInternal() != src.isInternal()) {
			return false;
		}
		if (node.isLeaf()) {
			return true;
		}
		
		uint32_t i(0);
		auto cit(tree.children(np));
		for(; cit.valid() && i < src.children().size(); cit.next(), ++i) {
			SSERIALIZE_EXPENSIVE_ASSERT(tree.node(cit.position()).valid());
			if (!equalTopology(cit.position(), *src.children().at(i))) {
				return false;
			}
		}
		
		if (cit.valid() || i < src.children().size()) {
			return false;
		}
		
		return true;
	}
};
	
}//end namespace inlinetests

///Create at dest.putPtr()
Tree
Tree::create(sserialize::UByteArrayAdapter dest, hic::impl::HCQRSpatialGrid const & src) {
	struct Recurser {
		using Source = hic::impl::HCQRSpatialGrid;
		Source const & src;
		Tree & tree;
		
		Recurser(Source const & src, Tree & tree) : src(src), tree(tree) {}
		
		Node convert(Source::TreeNode const & node) const {
			if (node.flags() & Node::HAS_INDEX) {
				return Node(node.pixelId(), node.flags(), node.itemIndexId());
			}
			else {
				return Node(node.pixelId(), node.flags());
			}
		}
		
		void operator()(Source::TreeNode const & root) {
			SSERIALIZE_CHEAP_ASSERT_EQUAL(src.sg().rootPixelId(), root.pixelId());
			Node rootNode(convert(root));
			rootNode.setFlags(rootNode.flags() | Node::IS_ROOT_NODE);
			NodePosition np = tree.push(rootNode);
			
			if (root.isLeaf()) {
				return;
			}
			NodePosition last = processChildren(root);
			tree.updateNextNode(last, np);
			SSERIALIZE_EXPENSIVE_ASSERT(inlinetests::TreeFromHCQRSpatialGrid(tree).equalTopology(np, root));
		}
		
		NodePosition processChildren(Source::TreeNode const & node) {
			SSERIALIZE_CHEAP_ASSERT(node.isInternal());
			
			NodePosition prev = rec(*node.children().at(0));
			for(uint32_t i(1), s(node.children().size()); i < s; ++i) {
				NodePosition newPrev = rec(*node.children().at(i));
				tree.updateNextNode(prev, newPrev);
				prev = newPrev;
			}
			return prev;
		}
		
		NodePosition rec(Source::TreeNode const & node) {
			NodePosition np = tree.push(convert(node));
			if (node.isLeaf()) {
				return np;
			}
			NodePosition last = processChildren(node);
			tree.updateNextNode(last, np);
			SSERIALIZE_EXPENSIVE_ASSERT(inlinetests::TreeFromHCQRSpatialGrid(tree).equalTopology(np, node));
			return np;
		}
	};
	
	Tree tree( create(dest, src.sgPtr()) );
	
	if (src.root()) {
		Recurser r(src, tree);
		r(*src.root());
		SSERIALIZE_EXPENSIVE_ASSERT(inlinetests::TreeFromHCQRSpatialGrid(tree).equalTopology(tree.rootNodePosition(), *src.root()));
	}
	
	return tree;
}

sserialize::UByteArrayAdapter::OffsetType
Tree::getSizeInBytes() const {
	return m_md.dataSize()+MetaData::StorageSize;
}

sserialize::UByteArrayAdapter
Tree::data() const {
	return m_md.treeData();
}

bool
Tree::hasNodes() const {
	return m_md.dataSize();
}

sserialize::UByteArrayAdapter &
Tree::nodeData() {
	return m_nd;
}

sserialize::UByteArrayAdapter const &
Tree::nodeData() const {
	return m_nd;
}

Tree::NodePosition
Tree::rootNodePosition() const {
	return NodePosition(0, 0);
}

Tree::NodePosition
Tree::nextNode(NodePosition const & np) const {
	return nextNode(nodeInfo(np)).position();
}

Tree::NodeInfo
Tree::nextNode(NodeInfo const & ni) const {
	if (ni.position().isRootNode()) {
		throw sserialize::OutOfBoundsException("Node does not have a next node");
	}
	if (ni.node().hasParent()) {
		auto grandParentPId = m_sg->parent(ni.position().parent());
		NodePosition pnp(grandParentPId, ni.position().dataOffset() - ni.node().nextNodeOffset());
		SSERIALIZE_CHEAP_ASSERT_LARGER(ni.position().dataOffset(), pnp.dataOffset());
		return nodeInfo(pnp);
	}
	else { //next node is a sibling
		NodePosition nnp(ni.position().parent(), ni.position().dataOffset()+ni.node().nextNodeOffset());
		SSERIALIZE_CHEAP_ASSERT_SMALLER(ni.position().dataOffset(), nnp.dataOffset());
		return nodeInfo(nnp);
	}
}

Tree::NodeInfo
Tree::parent(NodeInfo const & ni) const {
	if (ni.position().isRootNode()) {
		throw sserialize::OutOfBoundsException("Node does not have a parent");
	}
	if (ni.node().hasParent()) {
		return nextNode(ni);
	}
	else {
		auto nni = nextNode(ni);
		while(!nni.node().hasParent()) {
			nni = nextNode(nni);
		}
		return nextNode(nni);
	}
}

Tree::NodePosition
Tree::firstChild(NodePosition const & np) const {
	return firstChild(nodeInfo(np)).position();
}

Tree::NodeInfo
Tree::firstChild(NodeInfo const & ni) const {
	if (!ni.node().isInternal()) {
		throw sserialize::OutOfBoundsException("Node has no children");
	}
	return nodeInfo(NodePosition(ni.node().pixelId(), ni.position().dataOffset()+ni.node().getSizeInBytes(*m_sg)));
}

Tree::NodePosition
Tree::parent(NodePosition const & np) const {
	return parent(nodeInfo(np)).position();
}

Tree::ChildrenIterator
Tree::children(NodePosition const & np) const {
	return ChildrenIterator(*this, np);
}

Tree::ChildrenIterator
Tree::children(NodeInfo const & ni) const {
	return ChildrenIterator(*this, ni);
}

Tree::Node
Tree::node(NodePosition const & np) const {
	if (np.dataOffset()+Node::MinimumDataSize > nodeData().size()) {
		throw sserialize::OutOfBoundsException("Tree::node");
	}
	return Node( sserialize::UByteArrayAdapter(nodeData(), np.dataOffset()), np.parent(), *m_sg);
}

Tree::NodeInfo
Tree::nodeInfo(NodePosition const & np) const {
	return NodeInfo(node(np), np);
}

Tree::NodePosition
Tree::push(Node const & node) {
	auto nds = node.minSizeInBytes(*m_sg);
	auto ndp = nodeData().tellPutPtr();
	node.update(nodeData().fromPutPtr(), node, nds, *m_sg);
	nodeData().growStorage(nds);
	nodeData().incPutPtr(nds);
	m_md.setDataSize(nodeData().tellPutPtr());
	NodePosition rnp;
	if (node.isRoot()) {
		SSERIALIZE_CHEAP_ASSERT_EQUAL(0, ndp);
		rnp = NodePosition(0, ndp);
	}
	else {
		rnp = NodePosition(m_sg->parent(node.pixelId()), ndp);
	}
	
	SSERIALIZE_CHEAP_ASSERT_EQUAL(nds, nodeData().tellPutPtr() - rnp.dataOffset());
	SSERIALIZE_EXPENSIVE_ASSERT_EQUAL(this->node(rnp).getSizeInBytes(*m_sg), nodeData().tellPutPtr() - rnp.dataOffset());
	SSERIALIZE_EXPENSIVE_ASSERT_EQUAL(node, this->node(rnp));
	return rnp;
}

void Tree::pop(NodePosition pos) {
	SSERIALIZE_CHEAP_ASSERT_SMALLER(pos.dataOffset(), nodeData().tellPutPtr());
	SSERIALIZE_CHEAP_ASSERT_SMALLER(nodeData().tellPutPtr() - pos.dataOffset(), Node::MinimumDataSize);
	SSERIALIZE_CHEAP_ASSERT_SMALLER(nodeData().tellPutPtr() - pos.dataOffset(), Node::MaximumDataSize);
	SSERIALIZE_NORMAL_ASSERT_EQUAL(nodeData().tellPutPtr() - pos.dataOffset(), node(pos).getSizeInBytes(*m_sg));
	nodeData().setPutPtr(pos.dataOffset());
	m_md.setDataSize(nodeData().tellPutPtr());
}

void
Tree::update(NodePosition const & pos, Node const & node) {
	auto nd = sserialize::UByteArrayAdapter(nodeData(), pos.dataOffset());
	TreeNode oldNode(nd, pos.parent(), *m_sg);
	TreeNode::update(nd, node, oldNode.getSizeInBytes(*m_sg), *m_sg);
	SSERIALIZE_CHEAP_ASSERT_EQUAL(oldNode.pixelId(), node.pixelId());
	SSERIALIZE_EXPENSIVE_ASSERT_EQUAL(node, this->node(pos));
}

void
Tree::updateNextNode(NodePosition const & target, NodePosition const & nextNodePosition) {
	Node tn( node(target) );
	if (nextNodePosition.dataOffset() < target.dataOffset()) {
		SSERIALIZE_EXPENSIVE_ASSERT_EQUAL(node(nextNodePosition).pixelId(), m_sg->parent(tn.pixelId()));
		tn.setFlags(tn.flags() | Node::NEXT_NODE_IS_PARENT);
		tn.setNextNodeOffset(target.dataOffset() - nextNodePosition.dataOffset());
	}
	else {
		SSERIALIZE_EXPENSIVE_ASSERT_EQUAL(m_sg->parent(node(nextNodePosition).pixelId()), m_sg->parent(tn.pixelId()));
		tn.setNextNodeOffset(nextNodePosition.dataOffset() - target.dataOffset());
	}
	SSERIALIZE_CHEAP_ASSERT_LARGER(tn.nextNodeOffset(), 0);
	update(target, tn);
}

//END Tree


}//end namespace hic::Static::detail::HCQRSpatialGrid

namespace hic::Static::impl {

HCQRSpatialGrid::HCQRSpatialGrid(hic::impl::HCQRSpatialGrid const & other) :
Parent(other),
m_tree(Tree::create(sserialize::UByteArrayAdapter::createCache(), other)),
m_items(other.idxStore()),
m_fetchedItems(other.fetchedItems())
{
	SSERIALIZE_EXPENSIVE_ASSERT_EQUAL(this->depth(), other.depth());
	SSERIALIZE_EXPENSIVE_ASSERT_EQUAL(this->numberOfNodes(), other.numberOfNodes());
	SSERIALIZE_EXPENSIVE_ASSERT( detail::HCQRSpatialGrid::inlinetests::TreeFromHCQRSpatialGrid(m_tree).equalTopology(m_tree.rootNodePosition(), *other.root()));
	SSERIALIZE_EXPENSIVE_ASSERT_EQUAL(this->items(), other.items());
}

HCQRSpatialGrid::HCQRSpatialGrid(
	sserialize::UByteArrayAdapter const & data,
	sserialize::Static::ItemIndexStore const & idxStore,
	sserialize::RCPtrWrapper<hic::interface::SpatialGrid> sg,
	sserialize::RCPtrWrapper<hic::interface::SpatialGridInfo> sgi
) :
Parent(sg, sgi),
m_tree(data, sg),
m_items(idxStore)
{}

HCQRSpatialGrid::HCQRSpatialGrid(
	Tree && tree,
	sserialize::Static::ItemIndexStore const & idxStore,
	std::vector<sserialize::ItemIndex> fetchedItems,
	sserialize::RCPtrWrapper<hic::interface::SpatialGrid> sg,
	sserialize::RCPtrWrapper<hic::interface::SpatialGridInfo> sgi
) :
Parent(sg, sgi),
m_tree(std::move(tree)),
m_items(idxStore),
m_fetchedItems(fetchedItems)
{}

HCQRSpatialGrid::HCQRSpatialGrid(
	sserialize::Static::ItemIndexStore const & idxStore,
	sserialize::RCPtrWrapper<hic::interface::SpatialGrid> sg,
	sserialize::RCPtrWrapper<hic::interface::SpatialGridInfo> sgi
) :
Parent(sg, sgi),
m_tree(Tree::create(sserialize::UByteArrayAdapter::createCache(), sg)),
m_items(idxStore)
{}

HCQRSpatialGrid::~HCQRSpatialGrid() {}

HCQRSpatialGrid::SizeType
HCQRSpatialGrid::depth() const {
    struct Recurser {
		HCQRSpatialGrid const & that;
		Tree const & tree;
		Recurser(HCQRSpatialGrid const & that) : that(that), tree(that.tree()) {}
        SizeType operator()(Tree::NodePosition const & np) const {
			auto node = tree.node(np);
			if (node.isLeaf()) {
				return 0;
			}
			SizeType maxDepth = 0;
			for(Tree::ChildrenIterator cit(that.tree().children(np)); cit.valid(); cit.next()) {
				maxDepth = std::max(maxDepth, (*this)(cit.position()));
			}
			return maxDepth+1;
        }
    };
	if (m_tree.hasNodes()) {
		return Recurser(*this)(rootNodePosition());
	}
	else {
		return 0;
	}
}

HCQRSpatialGrid::SizeType
HCQRSpatialGrid::numberOfItems() const {
    struct Recurser {
        HCQRSpatialGrid const & that;
		Tree const & tree;
        SizeType numberOfItems{0};
        void operator()(Tree::NodePosition const & np) {
			Tree::Node node = tree.node(np);
            if (node.isInternal()) {
                for(auto cit(tree.children(np)); cit.valid(); cit.next()) {
                    (*this)(cit.position());
                }
            }
            else if (node.isFullMatch()) {
                numberOfItems += that.sgi().itemCount(node.pixelId());
            }
            else if (node.isFetched()) {
                numberOfItems += that.fetchedItems().at(node.itemIndexId()).size();
            }
            else { //partial-match
                numberOfItems += that.idxStore().idxSize(node.itemIndexId());
            }
        }
        Recurser(HCQRSpatialGrid const & that) : that(that), tree(that.tree()) {}
    };
	if (m_tree.hasNodes()) {
		Recurser r(*this);
		r(rootNodePosition());
		return r.numberOfItems;
	}
	else {
		return 0;
	}
}

HCQRSpatialGrid::SizeType
HCQRSpatialGrid::numberOfNodes() const {
    struct Recurser {
        HCQRSpatialGrid const & that;
		Tree const & tree;
        SizeType numberOfNodes{0};
        void operator()(Tree::NodePosition const & np) {
			numberOfNodes += 1;
			if (tree.node(np).isInternal()) {
				for(auto cit(tree.children(np)); cit.valid(); cit.next()) {
					(*this)(cit.position());
				}
			}
        }
        Recurser(HCQRSpatialGrid const & that) : that(that), tree(that.tree()) {}
    };
	if (tree().hasNodes()) {
		Recurser r(*this);
		r(rootNodePosition());
		return r.numberOfNodes;
	}
	else {
		return 0;
	}
}

HCQRSpatialGrid::ItemIndex
HCQRSpatialGrid::items() const {
	if (tree().hasNodes()) {
		return items(rootNodePosition());
	}
	else {
		return ItemIndex();
	}
}

struct HCQRSpatialGrid::OpHelper {
    HCQRSpatialGrid & dest;
    OpHelper(HCQRSpatialGrid & dest) : dest(dest) {}
    
    Tree::NodePosition endOfSubTree(HCQRSpatialGrid const & src, Tree::NodePosition const & np) {
		Tree::Node node = src.tree().node(np);
		if (node.hasSibling()) {
			return src.tree().nextNode(np);
		}
		else if (node.hasParent()) {
			return endOfSubTree(src, src.tree().parent(np));
		}
		else {
			SSERIALIZE_CHEAP_ASSERT(node.isRoot());
			return src.tree().end();
		}
	}
	
    /** Problem: fetched index pointers may need to be updated such that they don't fit in the allocated memory
      * This is why the simple approach decodes and re-encodes the nodes.
      * Possible improvements 
      * Try-fail:
      * Copy the whole data and try to fix the fetched index pointers. If this fails, use the decode/reencode variant
	  * 
	  * try-fail-patch:
	  * Copy the data in-order but one node at a time, decode the data, but don't reencode it. Try t ofit the fetched index pointers.
	  * If this fails, then push a re-encoded node. This will shift the next-pointers of all ancestors by the additionally used storage
	  * Accumulate this shift of all subtree nodes and only update it once.
	  * This can be combined with stuff below
      *
	  * in-order encoding of fetched item indexes:
	  * Don't encode fetched indexes, but rather store them in-order (as is the case already).
	  * This however has the downside, that a tree-traversal is only possible in-order (in case we are interested in fetched indexes)
      *
      * flag to indicate whether the subtree of a node has a fetched index:
      * this would speed-up the try-fail-copy path since we would not need to check anything if the subtree does not cona
      *
      */
	Tree::NodePosition deepCopy(HCQRSpatialGrid const & src, Tree::NodePosition const & np) {
		Tree::Node node = src.tree().node(np);
		SSERIALIZE_CHEAP_ASSERT(np.valid());
		if (node.isInternal()) {
			Tree::NodePosition rnp = dest.tree().push(node);
			
			Tree::ChildrenIterator cit = src.tree().children(np);
			Tree::NodePosition rprev = deepCopy(src, cit.position());
			for(cit.next(); cit.valid(); cit.next()) {
				Tree::NodePosition rnewPrev = deepCopy(src, cit.position());
				dest.tree().updateNextNode(rprev, rnewPrev);
				rprev = rnewPrev;
			}
			
			dest.tree().updateNextNode(rprev, rnp);
			return rnp;
		}
		else if (node.isFetched()) {
			dest.fetchedItems().emplace_back( src.items(np) );
			node.setItemIndexId(dest.fetchedItems().size()-1);
			return dest.tree().push(node);
		}
		else {
			return dest.tree().push(node);
		}
	}

    PixelId resultPixelId(HCQRSpatialGrid::Tree::Node const & first, HCQRSpatialGrid::Tree::Node const & second) const {
		SSERIALIZE_NORMAL_ASSERT(first.valid());
		SSERIALIZE_NORMAL_ASSERT(second.valid());
        if (first.pixelId() == second.pixelId()) {
            return first.pixelId();
        }
        else if (dest.sg().isAncestor(first.pixelId(), second.pixelId())) {
            return second.pixelId();
        }
        else if (dest.sg().isAncestor(second.pixelId(), first.pixelId())) {
            return first.pixelId();
        }
        else {
            throw sserialize::BugException("Trying to compute common node for non-related tree nodes");
        }
    }
};

HCQRSpatialGrid::HCQRPtr
HCQRSpatialGrid::operator/(HCQR const & other) const {
	if (!dynamic_cast<Self const *>(&other)) {
		throw sserialize::TypeMissMatchException("Incorrect input type");
	}
	
    struct Recurser: public OpHelper {
        HCQRSpatialGrid const & firstSg;
        HCQRSpatialGrid const & secondSg;
        Recurser(HCQRSpatialGrid const & firstSg, HCQRSpatialGrid const & secondSg, HCQRSpatialGrid & dest) :
        OpHelper(dest),
        firstSg(firstSg),
        secondSg(secondSg)
        {}
        Tree::NodePosition operator()(Tree::NodePosition const & fnp, Tree::NodePosition const & snp) {
			SSERIALIZE_NORMAL_ASSERT(fnp.valid());
			SSERIALIZE_NORMAL_ASSERT(snp.valid());
			Tree::Node firstNode = firstSg.tree().node(fnp);
			Tree::Node secondNode = secondSg.tree().node(snp);
			int rnIsRoot = firstNode.flags() & secondNode.flags() & Tree::Node::IS_ROOT_NODE;
			Tree::NodePosition rnp;
			if (firstNode.isFullMatch() && secondNode.isFullMatch()) {
				SSERIALIZE_CHEAP_ASSERT_EQUAL(firstSg.sg().level(firstNode.pixelId()), secondSg.sg().level(secondNode.pixelId()));
				rnp = dest.tree().push(resultPixelId(firstNode, secondNode), Tree::Node::IS_FULL_MATCH | rnIsRoot);
			}
            else if (firstNode.isFullMatch() && secondNode.isInternal()) {
				SSERIALIZE_CHEAP_ASSERT_EQUAL(firstSg.sg().level(firstNode.pixelId()), secondSg.sg().level(secondNode.pixelId()));
				rnp = deepCopy(secondSg, snp);
            }
            else if (secondNode.isFullMatch() && firstNode.isInternal()) {
				SSERIALIZE_CHEAP_ASSERT_EQUAL(firstSg.sg().level(firstNode.pixelId()), secondSg.sg().level(secondNode.pixelId()));
                rnp = deepCopy(firstSg, fnp);
            }
            else if (firstNode.isLeaf() && secondNode.isLeaf()) {
                auto result = firstSg.items(fnp) / secondSg.items(snp);
                if (!result.size()) {
                    rnp = Tree::NodePosition();
                }
                else {
					dest.m_fetchedItems.emplace_back(result);
					rnp = dest.tree().push(resultPixelId(firstNode, secondNode), Tree::Node::IS_FETCHED | rnIsRoot, dest.m_fetchedItems.size()-1);
				}
            }
            else {
                rnp = dest.tree().push(resultPixelId(firstNode, secondNode), Tree::Node::IS_INTERNAL | rnIsRoot);
				Tree::NodePosition lastCNP;
				
                if (firstNode.isInternal() && secondNode.isInternal()) {
                    auto fIt = firstSg.tree().children(fnp);
                    auto sIt = secondSg.tree().children(snp);
                    for(;fIt.valid() && sIt.valid();) {
                        if (fIt.node().pixelId() < sIt.node().pixelId()) {
                            fIt.next();
                        }
                        else if (fIt.node().pixelId() > sIt.node().pixelId()) {
                            sIt.next();
                        }
                        else {
                            Tree::NodePosition x = (*this)(fIt.position(), sIt.position());
                            if (x.valid()) {
								if (lastCNP.valid()) {
									dest.tree().updateNextNode(lastCNP, x);
								}
								lastCNP = x;
                            }
                            fIt.next();
							sIt.next();
                        }
                    }
                }
                else if (firstNode.isInternal()) {
                    auto fIt = firstSg.tree().children(fnp);
                    for( ;fIt.valid(); fIt.next()) {
                        Tree::NodePosition x = (*this)(fIt.position(), snp);
						if (x.valid()) {
							if (lastCNP.valid()) {
								dest.tree().updateNextNode(lastCNP, x);
							}
							lastCNP = x;
						}
                    }
                }
                else if (secondNode.isInternal()) {
                    auto sIt = secondSg.tree().children(snp);
                    for( ;sIt.valid(); sIt.next()) {
                        auto x = (*this)(snp, sIt.position());
						if (x.valid()) {
							if (lastCNP.valid()) {
								dest.tree().updateNextNode(lastCNP, x);
							}
							lastCNP = x;
						}
                    }
                }

                if (lastCNP.valid()) {
					dest.tree().updateNextNode(lastCNP, rnp);
				}
				else {
					dest.tree().pop(rnp);
                    rnp = Tree::NodePosition();
                }
            }
            SSERIALIZE_EXPENSIVE_ASSERT_EQUAL(dest.items(rnp), firstSg.items(fnp) / secondSg.items(snp));
			return rnp;
        }
    };
	
    sserialize::RCPtrWrapper<Self> dest( new Self(m_items, sgPtr(), sgiPtr()) );
	if (tree().hasNodes() && static_cast<Self const &>(other).tree().hasNodes()) {
		Recurser rec(*this, static_cast<Self const &>(other), *dest);
		rec(this->rootNodePosition(), static_cast<Self const &>(other).rootNodePosition());
	}
	SSERIALIZE_EXPENSIVE_ASSERT_EQUAL(items() / other.items(), dest->items());
    return dest;
}

HCQRSpatialGrid::HCQRPtr
HCQRSpatialGrid::operator+(HCQR const & other) const {
	if (!dynamic_cast<Self const *>(&other)) {
		throw sserialize::TypeMissMatchException("Incorrect input type");
	}
	
    struct Recurser: public OpHelper {
        HCQRSpatialGrid const & firstSg;
        HCQRSpatialGrid const & secondSg;
        Recurser(HCQRSpatialGrid const & firstSg, HCQRSpatialGrid const & secondSg, HCQRSpatialGrid & dest) :
        OpHelper(dest),
        firstSg(firstSg),
        secondSg(secondSg)
        {}
        Tree::NodePosition operator()(Tree::NodePosition const & fnp, Tree::NodePosition const & snp) {
			SSERIALIZE_NORMAL_ASSERT(fnp.valid());
			SSERIALIZE_NORMAL_ASSERT(snp.valid());
			Tree::Node firstNode = firstSg.tree().node(fnp);
			Tree::Node secondNode = secondSg.tree().node(snp);
			int rnIsRoot = firstNode.flags() & secondNode.flags() & Tree::Node::IS_ROOT_NODE;
			Tree::NodePosition rnp;
			if (firstNode.isFullMatch()) {
				rnp = dest.tree().push(firstNode.pixelId(), firstNode.flags());
			}
            else if (secondNode.isFullMatch()) {
				rnp = dest.tree().push(secondNode.pixelId(), secondNode.flags());
            }
            else if (firstNode.isLeaf() && secondNode.isLeaf()) {
				auto fnLvl = firstSg.level(firstNode);
				auto snLvl = secondSg.level(secondNode);
				sserialize::ItemIndex result;
				if (fnLvl == snLvl) {
					result = firstSg.items(fnp) + secondSg.items(snp);
				}
				else if (fnLvl < snLvl) { //firstNode is an ancestor of secondNode
					SSERIALIZE_NORMAL_ASSERT(firstSg.sg().isAncestor(firstNode.pixelId(), secondNode.pixelId()));
					result = (firstSg.items(fnp) / firstSg.sgi().items(secondNode.pixelId())) + secondSg.items(snp);
				}
				else if (snLvl < fnLvl) { //secondNode is an ancestor of firstNode
					SSERIALIZE_NORMAL_ASSERT(firstSg.sg().isAncestor(secondNode.pixelId(), firstNode.pixelId()));
					result = firstSg.items(fnp) + (secondSg.items(snp) / secondSg.sgi().items(firstNode.pixelId()));
				}
				SSERIALIZE_CHEAP_ASSERT(result.size());
                dest.m_fetchedItems.emplace_back(result);
				rnp = dest.tree().push(resultPixelId(firstNode, secondNode), Tree::Node::IS_FETCHED | rnIsRoot, dest.m_fetchedItems.size()-1);
            }
            else {
                rnp = dest.tree().push(resultPixelId(firstNode, secondNode), Tree::Node::IS_INTERNAL | rnIsRoot);
				Tree::NodePosition lastCNP;
				
                if (firstNode.isInternal() && secondNode.isInternal()) {
                    auto fIt = firstSg.tree().children(fnp);
                    auto sIt = secondSg.tree().children(snp);
                    for(;fIt.valid() && sIt.valid();) {
						Tree::NodePosition x;
                        if (fIt.node().pixelId() < sIt.node().pixelId()) {
							x = deepCopy(firstSg, fIt.position());
                            fIt.next();
                        }
                        else if (fIt.node().pixelId() > sIt.node().pixelId()) {
							x = deepCopy(secondSg, sIt.position());
                            sIt.next();
                        }
                        else {
                            x = (*this)(fIt.position(), sIt.position());
                            fIt.next();
							sIt.next();
						}
						SSERIALIZE_CHEAP_ASSERT(x.valid());
						if (lastCNP.valid()) {
							dest.tree().updateNextNode(lastCNP, x);
						}
						lastCNP = x;
                    }
                    for(;fIt.valid();) {
						Tree::NodePosition x = deepCopy(firstSg, fIt.position());
						SSERIALIZE_CHEAP_ASSERT(x.valid());
						if (lastCNP.valid()) {
							dest.tree().updateNextNode(lastCNP, x);
						}
						lastCNP = x;
					}
                    for(;sIt.valid();) {
						Tree::NodePosition x = deepCopy(secondSg, sIt.position());
						SSERIALIZE_CHEAP_ASSERT(x.valid());
						if (lastCNP.valid()) {
							dest.tree().updateNextNode(lastCNP, x);
						}
						lastCNP = x;
					}
                }
                else if (firstNode.isInternal()) {
					SSERIALIZE_CHEAP_ASSERT(!secondNode.isFullMatch() && secondNode.isLeaf());
					std::vector<PixelId> virtSecondPids = secondSg.pixelChildren(firstNode.pixelId());
					
					auto secondNodeItems = secondSg.items(snp);
                    auto fIt = firstSg.tree().children(fnp);
					auto sIt = virtSecondPids.begin();
					auto sEnd = virtSecondPids.end();
                    for( ;fIt.valid() && sIt != sEnd; ++sIt) {
						Tree::NodePosition x;
						if (*sIt < fIt.node().pixelId()) {
							auto result = secondNodeItems / secondSg.items(*sIt);
							if (result.size()) {
								dest.m_fetchedItems.push_back(result);
								x = dest.tree().push(*sIt, Tree::Node::IS_FETCHED, dest.m_fetchedItems.size()-1);
							}
						}
						else {
							x = (*this)(fIt.position(), snp);
							fIt.next();
						}
						if (x.valid()) {
							if (lastCNP.valid()) {
								dest.tree().updateNextNode(lastCNP, x);
							}
							lastCNP = x;
						}
                    }
                }
                else if (secondNode.isInternal()) {
					SSERIALIZE_CHEAP_ASSERT(!firstNode.isFullMatch() && firstNode.isLeaf());
					std::vector<PixelId> virtFirstPids = firstSg.pixelChildren(secondNode.pixelId());
					
					auto firstNodeItems = firstSg.items(fnp);
					auto fIt = virtFirstPids.begin();
					auto fEnd = virtFirstPids.end();
                    auto sIt = firstSg.tree().children(snp);
                    for( ;sIt.valid() && fIt != fEnd; ++fIt) {
						Tree::NodePosition x;
						if (*fIt < sIt.node().pixelId()) {
							auto result = firstNodeItems / firstSg.items(*fIt);
							if (result.size()) {
								dest.m_fetchedItems.push_back(result);
								x = dest.tree().push(*fIt, Tree::Node::IS_FETCHED, dest.m_fetchedItems.size()-1);
							}
							
						}
						else {
							x = (*this)(fnp, sIt.position());
							sIt.next();
						}
						if (x.valid()) {
							if (lastCNP.valid()) {
								dest.tree().updateNextNode(lastCNP, x);
							}
							lastCNP = x;
						}
                    }
                }

                if (lastCNP.valid()) {
					dest.tree().updateNextNode(lastCNP, rnp);
				}
				else {
					dest.tree().pop(rnp);
                    rnp = Tree::NodePosition();
                }
            }
            SSERIALIZE_EXPENSIVE_ASSERT_EQUAL(dest.items(rnp), firstSg.items(fnp) / secondSg.items(snp));
			return rnp;
        }
    };
	
    sserialize::RCPtrWrapper<Self> dest( new Self(m_items, sgPtr(), sgiPtr()) );
	if (tree().hasNodes() && static_cast<Self const &>(other).tree().hasNodes()) {
		Recurser rec(*this, static_cast<Self const &>(other), *dest);
		rec(this->rootNodePosition(), static_cast<Self const &>(other).rootNodePosition());
	}
	SSERIALIZE_EXPENSIVE_ASSERT_EQUAL(items() / other.items(), dest->items());
    return dest;
}

HCQRSpatialGrid::HCQRPtr
HCQRSpatialGrid::operator-(HCQR const & other) const {
	if (!dynamic_cast<Self const *>(&other)) {
		throw sserialize::TypeMissMatchException("Incorrect input type");
	}
	
    struct Recurser: public OpHelper {
        HCQRSpatialGrid const & firstSg;
        HCQRSpatialGrid const & secondSg;
        Recurser(HCQRSpatialGrid const & firstSg, HCQRSpatialGrid const & secondSg, HCQRSpatialGrid & dest) :
        OpHelper(dest),
        firstSg(firstSg),
        secondSg(secondSg)
        {}
        Tree::NodePosition operator()(Tree::NodePosition const & fnp, Tree::NodePosition const & snp) {
			SSERIALIZE_NORMAL_ASSERT(fnp.valid());
			SSERIALIZE_NORMAL_ASSERT(snp.valid());
			Tree::Node firstNode = firstSg.tree().node(fnp);
			Tree::Node secondNode = secondSg.tree().node(snp);
			int rnIsRoot = firstNode.flags() & secondNode.flags() & Tree::Node::IS_ROOT_NODE;
			Tree::NodePosition rnp;
			if (secondNode.isFullMatch() && firstSg.level(firstNode) >= secondSg.level(secondNode)) {
				rnp = Tree::NodePosition();
			}
            else if (firstNode.isLeaf() && secondNode.isLeaf()) {
				auto fnLvl = firstSg.level(firstNode);
				auto snLvl = secondSg.level(secondNode);
				sserialize::ItemIndex result;
				if (fnLvl == snLvl) {
					result = firstSg.items(fnp) - secondSg.items(snp);
				}
				else if (fnLvl < snLvl) { //firstNode is an ancestor of secondNode
					SSERIALIZE_NORMAL_ASSERT(firstSg.sg().isAncestor(firstNode.pixelId(), secondNode.pixelId()));
					result = (firstSg.items(fnp) / firstSg.sgi().items(secondNode.pixelId())) - secondSg.items(snp);
				}
				else if (snLvl < fnLvl) { //secondNode is an ancestor of firstNode
					SSERIALIZE_NORMAL_ASSERT(firstSg.sg().isAncestor(secondNode.pixelId(), firstNode.pixelId()));
					result = firstSg.items(fnp) - (secondSg.items(snp) / secondSg.sgi().items(firstNode.pixelId()));
				}
                if (!result.size()) {
                    rnp = Tree::NodePosition();
                }
                else {
					dest.m_fetchedItems.emplace_back(result);
					rnp = dest.tree().push(resultPixelId(firstNode, secondNode), Tree::Node::IS_FETCHED | rnIsRoot, dest.m_fetchedItems.size()-1);
				}
            }
            else {
                rnp = dest.tree().push(resultPixelId(firstNode, secondNode), Tree::Node::IS_INTERNAL | rnIsRoot);
				Tree::NodePosition lastCNP;
				
                if (firstNode.isInternal() && secondNode.isInternal()) {
                    auto fIt = firstSg.tree().children(fnp);
                    auto sIt = secondSg.tree().children(snp);
                    for(;fIt.valid() && sIt.valid();) {
						Tree::NodePosition x;
                        if (fIt.node().pixelId() < sIt.node().pixelId()) {
							x = deepCopy(firstSg, fIt.position());
                            fIt.next();
                        }
                        else if (fIt.node().pixelId() > sIt.node().pixelId()) {
                            sIt.next();
							continue; //x is not valid
                        }
                        else {
							x = (*this)(fIt.position(), sIt.position());
                            fIt.next();
							sIt.next();
						}
						if (x.valid()) {
							if (lastCNP.valid()) {
								dest.tree().updateNextNode(lastCNP, x);
							}
							lastCNP = x;
						}
                    }
                    
                    for(;fIt.valid();) {
						Tree::NodePosition x = deepCopy(firstSg, fIt.position());
						SSERIALIZE_CHEAP_ASSERT(x.valid());
						if (lastCNP.valid()) {
							dest.tree().updateNextNode(lastCNP, x);
						}
						lastCNP = x;
					}
                }
                else if (firstNode.isInternal()) {
                    auto fIt = firstSg.tree().children(fnp);
                    for( ;fIt.valid(); fIt.next()) {
                        Tree::NodePosition x = (*this)(fIt.position(), snp);
						if (x.valid()) {
							if (lastCNP.valid()) {
								dest.tree().updateNextNode(lastCNP, x);
							}
							lastCNP = x;
						}
                    }
                }
                else if (secondNode.isInternal()) {
					SSERIALIZE_CHEAP_ASSERT(!firstNode.isFullMatch() && firstNode.isLeaf());
					std::vector<PixelId> virtFirstPids = firstSg.pixelChildren(secondNode.pixelId());
					
					sserialize::ItemIndex firstNodeItems;
					if (!firstNode.isFullMatch()) {
						firstNodeItems = firstSg.items(fnp);
					}
					
					auto fIt = virtFirstPids.begin();
					auto fEnd = virtFirstPids.end();
                    auto sIt = firstSg.tree().children(snp);
                    for( ;sIt.valid() && fIt != fEnd; ++fIt) {
						Tree::NodePosition x;
						if (*fIt < sIt.node().pixelId()) {
							if (firstNode.isFullMatch()) {
								x = dest.tree().push(*fIt, Tree::Node::IS_FULL_MATCH);
							}
							else {
								auto result = firstNodeItems / firstSg.items(*fIt);
								if (result.size()) {
									dest.m_fetchedItems.push_back(result);
									x = dest.tree().push(*fIt, Tree::Node::IS_FETCHED, dest.m_fetchedItems.size()-1);
								}
							}
						}
						else {
							x = (*this)(fnp, sIt.position());
							sIt.next();
						}
						if (x.valid()) {
							if (lastCNP.valid()) {
								dest.tree().updateNextNode(lastCNP, x);
							}
							lastCNP = x;
						}
                    }
                }

                if (lastCNP.valid()) {
					dest.tree().updateNextNode(lastCNP, rnp);
				}
				else {
					dest.tree().pop(rnp);
                    rnp = Tree::NodePosition();
                }
            }
            SSERIALIZE_EXPENSIVE_ASSERT_EQUAL(dest.items(rnp), firstSg.items(fnp) / secondSg.items(snp));
			return rnp;
        }
    };
	
    sserialize::RCPtrWrapper<Self> dest( new Self(m_items, sgPtr(), sgiPtr()) );
	if (tree().hasNodes() && static_cast<Self const &>(other).tree().hasNodes()) {
		Recurser rec(*this, static_cast<Self const &>(other), *dest);
		rec(this->rootNodePosition(), static_cast<Self const &>(other).rootNodePosition());
	}
	SSERIALIZE_EXPENSIVE_ASSERT_EQUAL(items() / other.items(), dest->items());
    return dest;
}

HCQRSpatialGrid::HCQRPtr
HCQRSpatialGrid::compactified(SizeType maxPMLevel) const {
	throw sserialize::UnimplementedFunctionException("Missing function");
    return HCQRSpatialGrid::HCQRPtr();
}


HCQRSpatialGrid::HCQRPtr
HCQRSpatialGrid::expanded(SizeType level) const {
	throw sserialize::UnimplementedFunctionException("Missing function");
    return HCQRSpatialGrid::HCQRPtr();
}

HCQRSpatialGrid::HCQRPtr
HCQRSpatialGrid::allToFull() const {
    struct Recurser: public OpHelper {
		HCQRSpatialGrid const & sg;
        Recurser(HCQRSpatialGrid const & sg, HCQRSpatialGrid & dest) :
        OpHelper(dest),
        sg(sg)
        {}
        Tree::NodePosition operator()(Tree::NodePosition const & np) {
			auto const & node = sg.tree().node(np);
			int rnIsRoot = node.flags() & Tree::Node::IS_ROOT_NODE;
			
            if (node.isInternal()) {
				Tree::NodePosition rnp = dest.tree().push(node.pixelId(), Tree::Node::IS_INTERNAL | rnIsRoot);
				Tree::NodePosition lastCNP;
				auto cit = sg.tree().children(np);
				for( ;cit.valid(); cit.next()) {
					Tree::NodePosition x = (*this)(cit.position());
					if (x.valid()) {
						if (lastCNP.valid()) {
							dest.tree().updateNextNode(lastCNP, x);
						}
						lastCNP = x;
					}
				}
				if (lastCNP.valid()) {
					dest.tree().updateNextNode(lastCNP, rnp);
				}
				else {
					dest.tree().pop(rnp);
                    rnp = Tree::NodePosition();
                }
                return rnp;
            }
            else {
                return dest.tree().push(node.pixelId(), Tree::Node::IS_FULL_MATCH | rnIsRoot);
            }
        }
    };
	
    sserialize::RCPtrWrapper<Self> dest( new Self(m_items, sgPtr(), sgiPtr()) );
	if (tree().hasNodes()) {
		Recurser rec(*this, *dest);
		rec(this->rootNodePosition());
	}
    return dest;
}

HCQRSpatialGrid::Tree::Node
HCQRSpatialGrid::root() const {
	return tree().node( rootNodePosition() );
}

HCQRSpatialGrid::Tree::NodePosition
HCQRSpatialGrid::rootNodePosition() const {
	return tree().rootNodePosition();
}

sserialize::ItemIndex
HCQRSpatialGrid::items(Tree::NodePosition const & np) const {
	if (!np.valid()) {
		return sserialize::ItemIndex();
	}
	auto node = tree().node(np);
	if (node.isInternal()) {
		std::vector<ItemIndex> tmp;
		for(auto cit(tree().children(np)); cit.valid(); cit.next()) {
			tmp.emplace_back(items(cit.position()));
			SSERIALIZE_EXPENSIVE_ASSERT_EQUAL(sg().parent(tree().node(cit.position()).pixelId()), node.pixelId());
			SSERIALIZE_EXPENSIVE_ASSERT_LARGER_OR_EQUAL(sg().childrenCount(node.pixelId()), tmp.size());
		}
		return ItemIndex::unite(tmp);
	}
	else if (node.isFullMatch()) {
		return sgi().items(node.pixelId());
	}
	else if (node.isFetched()) {
		return fetchedItems().at(node.itemIndexId());
	}
	else {
		return idxStore().at(node.itemIndexId());
	}
}

HCQRSpatialGrid::PixelLevel
HCQRSpatialGrid::level(Tree::Node const & node) const {
	return sg().level(node.pixelId());
}


void
HCQRSpatialGrid::flushFetchedItems(sserialize::ItemIndexFactory & idxFactory) {
	if (!fetchedItems().size()) {
		return;
	}
    struct Recurser {
		HCQRSpatialGrid & that;
		Tree & tree;
		sserialize::ItemIndexFactory & idxFactory;
		Recurser(HCQRSpatialGrid & that, sserialize::ItemIndexFactory & idxFactory) :
		that(that), tree(that.tree()), idxFactory(idxFactory)
		{}
        void operator()(Tree::NodePosition const & np) const {
			auto node = tree.node(np);
			if (node.isLeaf() && node.isFetched()) {
				node.setItemIndexId(
					idxFactory.addIndex(
						that.fetchedItems().at(
							node.itemIndexId()
						)
					)
				);
				tree.update(np, node);
			}
			for(Tree::ChildrenIterator cit(that.tree().children(np)); cit.valid(); cit.next()) {
				(*this)(cit.position());
			}
        }
    };
	Recurser(*this, idxFactory)(tree().rootNodePosition());
	m_fetchedItems.clear();
}

}//end namespace hic::Static::impl
