#include "StaticHCQRSpatialGrid.h"
#include <sserialize/storage/pack_unpack_functions.h>


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
		auto childPos = data.getVlPackedUint32(0, &len)
		SSERIALIZE_CHEAP_ASSERT(len > 0);
		p += len;
		m_pid = sg.index(parent, childPos);
	}
	
	if (m_f & (HAS_INDEX)) {
		m_itemIndexId = data.getVlPackedUint32(p, &len);
		SSERIALIZE_CHEAP_ASSERT(len > 0);
		p += len;
	}
	
	if (m_f & HAS_PADDING) {
		m_padding = data.getUint8(p);
		p += 1+m_padding;
	}
}

TreeNode::~TreeNode() {}

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
	return !hasSibling();
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
	SSERIALIZE_CHEAP_ASSERT()
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

///Create at dest.putPtr()
Tree
Tree::create(sserialize::UByteArrayAdapter dest, hic::impl::HCQRSpatialGrid const & src) {
	struct Recurser {
		using Source = hic::impl::HCQRSpatialGrid;
		Source const & src;
		Tree & tree;
		
		Recurser(Source const & src, Tree & tree) : src(src), tree(tree) {}
		
		Node convert(Source::TreeNode const & node) const {
			return Node(node.pixelId(), node.flags(), (node.flags() & Node::HAS_INDEX ? node.itemIndexId() : 0));
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
			return np;
		}
	};
	
	Tree tree( create(dest, src.sgPtr()) );
	Recurser r(src, tree);
	
	r(*src.root());
	
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
		return nodeInfo(pnp);
	}
	else { //next node is a sibling
		NodePosition nnp(ni.position().parent(), ni.position().dataOffset()+ni.node().nextNodeOffset());
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
	node.update(data().fromPutPtr(), node, nds, *m_sg);
	data().growStorage(nds);
	data().incPutPtr(nds);
	m_md.setDataSize(data().tellPutPtr());
	if (node.isRoot()) {
		SSERIALIZE_CHEAP_ASSERT_EQUAL(0, ndp);
		return NodePosition(0, ndp);
	}
	else {
		return NodePosition(m_sg->parent(node.pixelId()), ndp);
	}
}

void Tree::pop(NodePosition pos) {
	data().setPutPtr(pos.dataOffset());
	m_md.setDataSize(data().tellPutPtr());
}

void
Tree::update(NodePosition const & pos, Node const & node) {
	auto nd = sserialize::UByteArrayAdapter(data(), pos.dataOffset());
	TreeNode oldNode(nd, pos.parent(), *m_sg);
	TreeNode::update(nd, node, oldNode.getSizeInBytes(*m_sg), *m_sg);
}

void
Tree::updateNextNode(NodePosition const & target, NodePosition const & nextNodePosition) {
	Node tn( node(target) );
	if (tn.hasParent()) {
		SSERIALIZE_CHEAP_ASSERT_SMALLER(nextNodePosition.dataOffset(), target.dataOffset());
		tn.setNextNodeOffset(target.dataOffset() - nextNodePosition.dataOffset());
	}
	else {
		SSERIALIZE_CHEAP_ASSERT_SMALLER(target.dataOffset(), nextNodePosition.dataOffset());
		tn.setNextNodeOffset(nextNodePosition.dataOffset() - target.dataOffset());
	}
	update(target, tn);
}

//END Tree


}//end namespace hic::Static::detail::HCQRSpatialGrid

namespace hic::Static::impl {

HCQRSpatialGrid::HCQRSpatialGrid(hic::impl::HCQRSpatialGrid const & other) :
m_tree(Tree::create(sserialize::UByteArrayAdapter::createCache(), other)),
m_items(other.idxStore()),
m_fetchedItems(other.fetchedItems()),
m_sg(other.sgPtr()),
m_sgi(other.sgiPtr())
{}

HCQRSpatialGrid::HCQRSpatialGrid(
	sserialize::UByteArrayAdapter const & data,
	sserialize::Static::ItemIndexStore idxStore,
	sserialize::RCPtrWrapper<hic::interface::SpatialGrid> sg,
	sserialize::RCPtrWrapper<hic::interface::SpatialGridInfo> sgi
) :
m_tree(data, sg),
m_items(idxStore),
m_sg(sg),
m_sgi(sgi)
{}

HCQRSpatialGrid::HCQRSpatialGrid(
	Tree && tree,
	sserialize::Static::ItemIndexStore idxStore,
	std::vector<sserialize::ItemIndex> fetchedItems,
	sserialize::RCPtrWrapper<hic::interface::SpatialGrid> sg,
	sserialize::RCPtrWrapper<hic::interface::SpatialGridInfo> sgi
) :
m_tree(std::move(tree)),
m_items(idxStore),
m_fetchedItems(fetchedItems),
m_sg(sg),
m_sgi(sgi)
{}

HCQRSpatialGrid::HCQRSpatialGrid(
	sserialize::Static::ItemIndexStore idxStore,
	sserialize::RCPtrWrapper<hic::interface::SpatialGrid> sg,
	sserialize::RCPtrWrapper<hic::interface::SpatialGridInfo> sgi
) :
m_tree(Tree::create(sserialize::UByteArrayAdapter::createCache(), sg)),
m_items(idxStore),
m_sg(sg),
m_sgi(sgi)
{}

HCQRSpatialGrid::~HCQRSpatialGrid() {}

HCQRSpatialGrid::SizeType
HCQRSpatialGrid::depth() const {
    struct Recurser {
		HCQRSpatialGrid const & that;
		Tree const & tree;
		Recurser(HCQRSpatialGrid const & that) : that(that), tree(that.tree()) {}
        SizeType operator()(NodePosition const & np) const {
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
        void operator()(NodePosition const & np) {
			TreeNode node = tree.node(np);
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
        void operator()(NodePosition const & np) {
			numberOfNodes += 1;
			for(auto cit(tree.children(np)); cit.valid(); cit.next()) {
				(*this)(cit.position());
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
    
    NodePosition endOfSubTree(HCQRSpatialGrid const & src, NodePosition const & np) {
		TreeNode node = src.tree().node(np);
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
	NodePosition deepCopy(HCQRSpatialGrid const & src, NodePosition const & np) {
		Node node = src.tree().node(np);
		SSERIALIZE_CHEAP_ASSERT(np.valid());
		if (node.isInternal()) {
			NodePosition rnp = dest.tree().push(node);
			
			Tree::ChildrenIterator cit = src.tree().children(np);
			NodePosition rprev = deepCopy(src, cit.position());
			for(cit.next(); cit.valid(); cit.next()) {
				NodePosition rnewPrev = deepCopy(src, cit.position());
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

    PixelId resultPixelId(HCQRSpatialGrid::TreeNode const & first, HCQRSpatialGrid::TreeNode const & second) const {
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
HCQRSpatialGrid::operator/(Parent::Self const & other) const {
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
        NodePosition operator()(NodePosition const & fnp, NodePosition const & snp) {
			SSERIALIZE_NORMAL_ASSERT(fnp.valid());
			SSERIALIZE_NORMAL_ASSERT(snp.valid());
			TreeNode firstNode = firstSg.tree().node(fnp);
			TreeNode secondNode = secondSg.tree().node(snp);
			if (firstNode.isFullMatch() && secondNode.isFullMatch()) {
				SSERIALIZE_CHEAP_ASSERT_EQUAL(firstSg.sg().level(firstNode.pixelId()), secondSg.sg().level(secondNode.pixelId()));
				return dest.tree().push(resultPixelId(firstNode, secondNode), TreeNode::IS_FULL_MATCH);
			}
            else if (firstNode.isFullMatch() && secondNode.isInternal()) {
				SSERIALIZE_CHEAP_ASSERT_EQUAL(firstSg.sg().level(firstNode.pixelId()), secondSg.sg().level(secondNode.pixelId()));
				return deepCopy(secondSg, snp);
            }
            else if (secondNode.isFullMatch() && firstNode.isInternal()) {
				SSERIALIZE_CHEAP_ASSERT_EQUAL(firstSg.sg().level(firstNode.pixelId()), secondSg.sg().level(secondNode.pixelId()));
                return deepCopy(firstSg, fnp);
            }
            else if (firstNode.isLeaf() && secondNode.isLeaf()) {
                auto result = firstSg.items(fnp) / secondSg.items(snp);
                if (!result.size()) {
                    return NodePosition();
                }
                dest.m_fetchedItems.emplace_back(result);
                return dest.tree().push(resultPixelId(firstNode, secondNode), TreeNode::IS_FETCHED, dest.m_fetchedItems.size()-1);
            }
            else {
                NodePosition rnp = dest.tree().push(resultPixelId(firstNode, secondNode), TreeNode::IS_INTERNAL);
				uint32_t numberOfChildren = 0;
				
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
                            NodePosition x = (*this)(fIt.position(), sIt.position());
                            if (x.valid()) {
                                numberOfChildren += 1;
                            }
                            fIt.next();
							sIt.next();
                        }
                    }
                }
                else if (firstNode.isInternal()) {
                    auto fIt = firstSg.tree().children(fnp);
                    for( ;fIt.valid(); fIt.next()) {
                        NodePosition x = (*this)(fIt.position(), snp);
                        if (x.valid()) {
                            numberOfChildren += 1;
                        }
                    }
                }
                else if (secondNode.isInternal()) {
                    auto sIt = secondSg.tree().children(snp);
                    for( ;sIt.valid(); sIt.next()) {
                        auto x = (*this)(snp, sIt.position());
                        if (x.valid()) {
                            numberOfChildren += 1;
                        }
                    }
                }

                if (numberOfChildren) {
                    return rnp;
                }
                else {
					dest.tree().pop(rnp);
                    return NodePosition();
                }
            }
        }
    };
	
    sserialize::RCPtrWrapper<Self> dest( new Self(m_items, m_sg, m_sgi) );
	if (tree().hasNodes() && static_cast<Self const &>(other).tree().hasNodes()) {
		Recurser rec(*this, static_cast<Self const &>(other), *dest);
		rec(this->rootNodePosition(), static_cast<Self const &>(other).rootNodePosition());
	}
	SSERIALIZE_EXPENSIVE_ASSERT_EQUAL(items() / other.items(), dest->items());
    return dest;
}

HCQRSpatialGrid::HCQRPtr
HCQRSpatialGrid::operator+(Parent::Self const & other) const {
	if (!dynamic_cast<Self const *>(&other)) {
		throw sserialize::TypeMissMatchException("Incorrect input type");
	}
	throw sserialize::UnimplementedFunctionException("Missing function");
    return HCQRSpatialGrid::HCQRPtr();
}

HCQRSpatialGrid::HCQRPtr
HCQRSpatialGrid::operator-(Parent::Self const & other) const {
	if (!dynamic_cast<Self const *>(&other)) {
		throw sserialize::TypeMissMatchException("Incorrect input type");
	}
	throw sserialize::UnimplementedFunctionException("Missing function");
    return HCQRSpatialGrid::HCQRPtr();
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
	throw sserialize::UnimplementedFunctionException("Missing function");
    return HCQRSpatialGrid::HCQRPtr();
}

HCQRSpatialGrid::TreeNode
HCQRSpatialGrid::root() const {
	return tree().node( rootNodePosition() );
}

HCQRSpatialGrid::NodePosition
HCQRSpatialGrid::rootNodePosition() const {
	return tree().rootNodePosition();
}

sserialize::ItemIndex
HCQRSpatialGrid::items(NodePosition const & np) const {
	SSERIALIZE_NORMAL_ASSERT(np.valid());
	auto node = tree().node(np);
	if (node.isInternal()) {
		std::vector<ItemIndex> tmp;
		for(auto cit(tree().children(np)); cit.valid(); cit.next()) {
			tmp.emplace_back(items(cit.position()));
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
HCQRSpatialGrid::level(TreeNode const & node) const {
	return sg().level(node.pixelId());
}

}//end namespace hic::Static::impl
