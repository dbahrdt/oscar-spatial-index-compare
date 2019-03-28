#include "StaticHCQRSpatialGrid.h"
#include <sserialize/storage/pack_unpack_functions.h>


namespace hic::Static::detail::HCQRSpatialGrid {

//BEGIN TreeNode
	

TreeNode::TreeNode(PixelId pid, int flags, uint32_t itemIndexId, uint32_t nextNodeOffset, uint8_t padding) :
m_pid(pid),
m_itemIndexId(itemIndexId),
m_nextNodeOffset(nextNodeOffset),
m_f(flags),
m_padding(padding)
{}

TreeNode::TreeNode(sserialize::UByteArrayAdapter const & data, PixelId parent, SpatialGrid const & sg) :
m_pid(std::numeric_limits<PixelId>::max()),
m_itemIndexId(std::numeric_limits<uint32_t>::max()),
m_nextNodeOffset(0),
m_f(0),
m_padding(0)
{
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
		m_pid = sg.index(parent, data.getVlPackedUint32(0, &len));
		SSERIALIZE_CHEAP_ASSERT(len > 0);
		p += len;
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
	auto parent = sg.parent(m_pid);
	auto childPos = sg.childPosition(parent, pixelId());
	sserialize::UByteArrayAdapter::OffsetType ds = 0;
	ds += sserialize::SerializationInfo<uint8_t>::length;
	ds += sserialize::SerializationInfo<uint32_t>::length;
	if (!(flags() & IS_ROOT_NODE)) {
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
	return m_f & (IS_PARTIAL_MATCH | IS_FULL_MATCH | IS_PARTIAL_MATCH);
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
TreeNode::hasSibling() const {
	return !(flags() & NEXT_NODE_IS_PARENT);
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
	dest.putVlPackedUint32(sg.childPosition(sg.parent(node.pixelId()), node.pixelId()));
	
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

Tree::MetaData::MetaData(sserialize::UByteArrayAdapter const & d) {}

Tree::MetaData::MetaData::~MetaData() {}

sserialize::UByteArrayAdapter::OffsetType
Tree::MetaData::dataSize() const {
	return m_d.getUint32( sserialize::UByteArrayAdapter::OffsetType(Positions::DATA_SIZE) );
}

sserialize::UByteArrayAdapter
Tree::MetaData::treeData() const {
	sserialize::UByteArrayAdapter tmp(m_d);
	tmp.growStorage(dataSize());
	return tmp;
}

void
Tree::MetaData::setDataSize(uint32_t v) {
	m_d.putUint32(sserialize::UByteArrayAdapter::OffsetType(Positions::DATA_SIZE), v);
}

//END Tree::MetaData

//BEGIN Tree


Tree::Tree(sserialize::UByteArrayAdapter const & data, sserialize::RCPtrWrapper<SpatialGrid> const & sg) :
m_md(data),
m_nd(data+ uint32_t(MetaData::StorageSize)),
m_sg(sg)
{
	m_nd.setPutPtr(m_md.dataSize());
}

Tree::~Tree() {}

Tree
Tree::create(sserialize::UByteArrayAdapter dest, sserialize::RCPtrWrapper<SpatialGrid> const & sg) {
	dest.shrinkToPutPtr();
	dest.putUint32(0);
	return Tree(dest, sg);
}

///Create at dest.putPtr()
Tree
Tree::create(sserialize::UByteArrayAdapter dest, hic::impl::HCQRSpatialGrid const & src) {
	Tree tree( create(dest, src.sgPtr()) );
	
	struct Recurser {
		using Source = hic::impl::HCQRSpatialGrid;
		Source const & src;
		Tree & tree;
		
		Node convert(Source::TreeNode const & node) const {
			return Node(node.pixelId(), node.flags(), (node.flags() & Node::HAS_INDEX ? node.itemIndexId() : 0));
		}
		
		void updateNextNode(NodePosition const & target, NodePosition const & np) {
			auto targetNode = tree.get(target);
			targetNode.setNextNodeOffset(np.dataOffset());
			tree.update(target, targetNode);
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
			updateNextNode(last, np);
		}
		
		NodePosition processChildren(Source::TreeNode const & node) {
			SSERIALIZE_CHEAP_ASSERT(node.isInternal());
			
			NodePosition prev = rec(*node.children().at(0));
			for(uint32_t i(1), s(node.children().size()); i < s; ++i) {
				NodePosition newPrev = rec(*node.children().at(i));
				
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
			updateNextNode(last, np);
			return np;
		}
	};
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
Tree::parent(Node const & node, NodePosition const & np) const {
	if (np.isRootNode()) {
		throw sserialize::OutOfBoundsException("Node does not have a parent");
	}
	if (node.hasParent()) {
		return NodePosition(m_sg->parent(np.parent()), node.nextNodeOffset());
	}
	else {
		NodePosition tnp( np.parent(), node.nextNodeOffset());
		Node tn = get(tnp);
		while(!tn.hasParent()) {
			tnp = NodePosition( tnp.parent(), tn.nextNodeOffset());
			tn = get(tnp);
		}
		SSERIALIZE_CHEAP_ASSERT(tn.hasParent());
		return NodePosition( m_sg->parent(np.parent()), tn.nextNodeOffset() );
	}
}

Tree::ChildrenIterator
Tree::children(NodePosition const & np) const {
	return ChildrenIterator(*this, np);
}

Tree::Node
Tree::get(NodePosition const & np) const {
	return Node( sserialize::UByteArrayAdapter(nodeData(), np.dataOffset()), np.parent(), *m_sg);
}

Tree::NodePosition
Tree::push(Node const & node) {
	auto nds = node.minSizeInBytes(*m_sg);
	auto ndp = nodeData().tellPutPtr();
	node.update(data().fromPutPtr(), node, nds, *m_sg);
	data().growStorage(nds);
	data().incPutPtr(nds);
	m_md.setDataSize(data().tellPutPtr());
	return NodePosition(m_sg->parent(node.pixelId()), ndp);
}

void Tree::pop(NodePosition pos) {
	data().setPutPtr(pos.dataOffset());
	m_md.setDataSize(data().tellPutPtr());
}

void
Tree::update(NodePosition pos, Node const & node) {
	auto nd = sserialize::UByteArrayAdapter(data(), pos.dataOffset());
	TreeNode oldNode(nd, pos.parent(), *m_sg);
	node.update(nd, node, oldNode.getSizeInBytes(*m_sg), *m_sg);
}

//END Tree


}//end namespace hic::Static::detail::HCQRSpatialGrid

namespace hic::Static::impl {
	
	
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

HCQRSpatialGrid::~HCQRSpatialGrid() {}

HCQRSpatialGrid::SizeType
HCQRSpatialGrid::depth() const {
    struct Recurser {
		HCQRSpatialGrid const & that;
		Tree const & tree;
		Recurser(HCQRSpatialGrid const & that) : that(that), tree(that.tree()) {}
        SizeType operator()(TreeNodePosition const & np) const {
			auto node = tree.get(np);
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
        void operator()(TreeNodePosition const & np) {
			TreeNode node = tree.get(np);
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
        void operator()(TreeNodePosition const & np) {
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
    std::unique_ptr<HCQRSpatialGrid::TreeNode> deepCopy(HCQRSpatialGrid const & src, HCQRSpatialGrid::TreeNode const & node) {
		SSERIALIZE_NORMAL_ASSERT(node.valid());
		if (node.isInternal()) {
			auto result = node.shallowCopy();
			for(auto const & x : node.children()) {
				result->children().emplace_back( this->deepCopy(src, *x) );
			}
			return result;
		}
        else if (node.isFetched()) {
            dest.m_fetchedItems.emplace_back( src.items(node) );
            return node.shallowCopy(dest.m_fetchedItems.size()-1);
        }
        else {
            return node.shallowCopy();
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

    void sortChildren(TreeNode & node) {
		SSERIALIZE_NORMAL_ASSERT(node.valid());
        sort(node.children().begin(), node.children().end(), [](std::unique_ptr<TreeNode> const & a, std::unique_ptr<TreeNode> const & b) {
            return a->pixelId() < b->pixelId();
        });
    }
};

HCQRSpatialGrid::HCQRPtr
HCQRSpatialGrid::operator/(Parent::Self const & other) const {
    struct Recurser: public HCQRSpatialGridOpHelper {
        HCQRSpatialGrid const & firstSg;
        HCQRSpatialGrid const & secondSg;
        Recurser(HCQRSpatialGrid const & firstSg, HCQRSpatialGrid const & secondSg, HCQRSpatialGrid & dest) :
        HCQRSpatialGridOpHelper(dest),
        firstSg(firstSg),
        secondSg(secondSg)
        {}
        std::unique_ptr<TreeNode> operator()(TreeNode const & firstNode, TreeNode const & secondNode) {
			SSERIALIZE_NORMAL_ASSERT(firstNode.valid());
			SSERIALIZE_NORMAL_ASSERT(secondNode.valid());
			if (firstNode.isFullMatch() && secondNode.isFullMatch()) {
				SSERIALIZE_CHEAP_ASSERT_EQUAL(firstSg.sg().level(firstNode.pixelId()), secondSg.sg().level(secondNode.pixelId()));
				return TreeNode::make_unique(resultPixelId(firstNode, secondNode), TreeNode::IS_FULL_MATCH);
			}
            else if (firstNode.isFullMatch() && secondNode.isInternal()) {
				SSERIALIZE_CHEAP_ASSERT_EQUAL(firstSg.sg().level(firstNode.pixelId()), secondSg.sg().level(secondNode.pixelId()));
				return deepCopy(secondSg, secondNode);
            }
            else if (secondNode.isFullMatch() && firstNode.isInternal()) {
				SSERIALIZE_CHEAP_ASSERT_EQUAL(firstSg.sg().level(firstNode.pixelId()), secondSg.sg().level(secondNode.pixelId()));
                return deepCopy(firstSg, firstNode);
            }
            else if (firstNode.isLeaf() && secondNode.isLeaf()) {
                auto result = firstSg.items(firstNode) / secondSg.items(secondNode);
                if (!result.size()) {
                    return std::unique_ptr<TreeNode>();
                }
                dest.m_fetchedItems.emplace_back(result);
                return TreeNode::make_unique(resultPixelId(firstNode, secondNode), TreeNode::IS_FETCHED, dest.m_fetchedItems.size()-1);
            }
            else {
                std::unique_ptr<TreeNode> resNode = TreeNode::make_unique(resultPixelId(firstNode, secondNode), TreeNode::IS_INTERNAL);

                if (firstNode.isInternal() && secondNode.isInternal()) {
                    auto fIt = firstNode.children().begin();
                    auto fEnd = firstNode.children().end();
                    auto sIt = secondNode.children().begin();
                    auto sEnd = secondNode.children().end();
                    for(;fIt != fEnd && sIt != sEnd;) {
                        if ((*fIt)->pixelId() < (*sIt)->pixelId()) {
                            ++fIt;
                        }
                        else if ((*fIt)->pixelId() > (*sIt)->pixelId()) {
                            ++sIt;
                        }
                        else {
                            auto x = (*this)(**fIt, **sIt);
                            if (x) {
                                resNode->children().emplace_back(std::move(x));
                            }
                            ++fIt;
							++sIt;
                        }
                    }
                }
                else if (firstNode.isInternal()) {
                    auto fIt = firstNode.children().begin();
                    auto fEnd = firstNode.children().end();
                    for( ;fIt != fEnd; ++fIt) {
                        auto x = (*this)(**fIt, secondNode);
                        if (x) {
                            resNode->children().emplace_back(std::move(x));
                        }
                    }
                }
                else if (secondNode.isInternal()) {
                    auto sIt = secondNode.children().begin();
                    auto sEnd = secondNode.children().end();
                    for( ;sIt != sEnd; ++sIt) {
                        auto x = (*this)(firstNode, **sIt);
                        if (x) {
                            resNode->children().emplace_back(std::move(x));
                        }
                    }
                }

                if (resNode->children().size()) {
                    return resNode;
                }
                else {
                    return std::unique_ptr<TreeNode>();
                }
            }
        }
    };
    sserialize::RCPtrWrapper<Self> dest( new Self(m_items, m_sg, m_sgi) );
	if (m_root && static_cast<Self const &>(other).m_root) {
		Recurser rec(*this, static_cast<Self const &>(other), *dest);
		dest->m_root = rec(*(this->m_root), *(static_cast<Self const &>(other).m_root));
	}
	SSERIALIZE_EXPENSIVE_ASSERT_EQUAL(items() / other.items(), dest->items());
    return dest;
}

HCQRSpatialGrid::HCQRPtr
HCQRSpatialGrid::operator+(Parent::Self const & other) const {
    struct Recurser: public HCQRSpatialGridOpHelper {
        HCQRSpatialGrid const & firstSg;
        HCQRSpatialGrid const & secondSg;
        Recurser(HCQRSpatialGrid const & firstSg, HCQRSpatialGrid const & secondSg, HCQRSpatialGrid & dest) :
        HCQRSpatialGridOpHelper(dest),
        firstSg(firstSg),
        secondSg(secondSg)
        {}
        std::unique_ptr<TreeNode> operator()(TreeNode const & firstNode, TreeNode const & secondNode) {
			SSERIALIZE_NORMAL_ASSERT(firstNode.valid());
			SSERIALIZE_NORMAL_ASSERT(secondNode.valid());
			if (firstNode.isFullMatch()) {
				return TreeNode::make_unique(firstNode.pixelId(), TreeNode::IS_FULL_MATCH);
			}
            else if (secondNode.isFullMatch()) {
				return TreeNode::make_unique(secondNode.pixelId(), TreeNode::IS_FULL_MATCH);
            }
            else if (firstNode.isLeaf() && secondNode.isLeaf()) {
				auto fnLvl = firstSg.level(firstNode);
				auto snLvl = secondSg.level(secondNode);
				sserialize::ItemIndex result;
				if (fnLvl == snLvl) {
					result = firstSg.items(firstNode) / secondSg.items(secondNode);
				}
				else if (fnLvl < snLvl) {
					result = (firstSg.items(firstNode) / firstSg.sgi().items(firstNode.pixelId())) + secondSg.items(secondNode);
				}
				else if (snLvl < fnLvl) {
					result = firstSg.items(firstNode) + (secondSg.items(secondNode) / secondSg.sgi().items(secondNode.pixelId()));
				}
				SSERIALIZE_CHEAP_ASSERT(result.size());
                dest.m_fetchedItems.emplace_back(result);
                return TreeNode::make_unique(resultPixelId(firstNode, secondNode), TreeNode::IS_FETCHED, dest.m_fetchedItems.size()-1);
            }
            else {
                std::unique_ptr<TreeNode> resNode = TreeNode::make_unique(resultPixelId(firstNode, secondNode), TreeNode::IS_INTERNAL);

                if (firstNode.isInternal() && secondNode.isInternal()) {
                    auto fIt = firstNode.children().begin();
                    auto fEnd = firstNode.children().end();
                    auto sIt = secondNode.children().begin();
                    auto sEnd = secondNode.children().end();
                    for(;fIt != fEnd && sIt != sEnd;) {
                        if ((*fIt)->pixelId() < (*sIt)->pixelId()) {
                            ++fIt;
                        }
                        else if ((*fIt)->pixelId() > (*sIt)->pixelId()) {
                            ++sIt;
                        }
                        else {
                            auto x = (*this)(**fIt, **sIt);
                            if (x) {
                                resNode->children().emplace_back(std::move(x));
                            }
                            ++fIt;
							++sIt;
                        }
                    }
                    for(; fIt != fEnd; ++fIt) {
						resNode->children().emplace_back( deepCopy(firstSg, **fIt) );
					}
					for(; sIt != sEnd; ++sIt) {
						resNode->children().emplace_back( deepCopy(secondSg, **sIt) );
					}
                }
                else if (firstNode.isInternal()) {
                    auto fIt = firstNode.children().begin();
                    auto fEnd = firstNode.children().end();
                    for( ;fIt != fEnd; ++fIt) {
                        auto x = (*this)(**fIt, secondNode);
                        if (x) {
                            resNode->children().emplace_back(std::move(x));
                        }
                    }
                }
                else if (secondNode.isInternal()) {
                    auto sIt = secondNode.children().begin();
                    auto sEnd = secondNode.children().end();
                    for( ;sIt != sEnd; ++sIt) {
                        auto x = (*this)(firstNode, **sIt);
                        if (x) {
                            resNode->children().emplace_back(std::move(x));
                        }
                    }
                }

                if (resNode->children().size()) {
                    return resNode;
                }
                else {
                    return std::unique_ptr<TreeNode>();
                }
            }
        }
    };
    sserialize::RCPtrWrapper<Self> dest( new Self(m_items, m_sg, m_sgi) );
	if (m_root && static_cast<Self const &>(other).m_root) {
		Recurser rec(*this, static_cast<Self const &>(other), *dest);
		dest->m_root = rec(*(this->m_root), *(static_cast<Self const &>(other).m_root));
	}
	SSERIALIZE_EXPENSIVE_ASSERT_EQUAL(items() + other.items(), dest->items());
    return dest;
}

HCQRSpatialGrid::HCQRPtr
HCQRSpatialGrid::operator-(Parent::Self const & other) const {
    struct Recurser: public HCQRSpatialGridOpHelper {
        HCQRSpatialGrid const & firstSg;
        HCQRSpatialGrid const & secondSg;
        Recurser(HCQRSpatialGrid const & firstSg, HCQRSpatialGrid const & secondSg, HCQRSpatialGrid & dest) :
        HCQRSpatialGridOpHelper(dest),
        firstSg(firstSg),
        secondSg(secondSg)
        {}
        std::unique_ptr<TreeNode> operator()(TreeNode const & firstNode, TreeNode const & secondNode) {
			SSERIALIZE_NORMAL_ASSERT(firstNode.valid());
			SSERIALIZE_NORMAL_ASSERT(secondNode.valid());
			if (firstNode.isFullMatch() && secondNode.isFullMatch()) {
				SSERIALIZE_CHEAP_ASSERT_EQUAL(firstSg.sg().level(firstNode.pixelId()), secondSg.sg().level(secondNode.pixelId()));
				return std::unique_ptr<TreeNode>();
			}
            else if (firstNode.isLeaf() && secondNode.isLeaf()) {
				auto fnLvl = firstSg.level(firstNode);
				auto snLvl = secondSg.level(secondNode);
				sserialize::ItemIndex result;
				if (fnLvl == snLvl) {
					result = firstSg.items(firstNode) - secondSg.items(secondNode);
				}
				else if (fnLvl < snLvl) {
					result = (firstSg.items(firstNode) / firstSg.sgi().items(firstNode.pixelId())) - secondSg.items(secondNode);
				}
				else if (snLvl < fnLvl) {
					result = firstSg.items(firstNode) - (secondSg.items(secondNode) / secondSg.sgi().items(secondNode.pixelId()));
				}
                if (!result.size()) {
                    return std::unique_ptr<TreeNode>();
                }
                dest.m_fetchedItems.emplace_back(result);
                return TreeNode::make_unique(resultPixelId(firstNode, secondNode), TreeNode::IS_FETCHED, dest.m_fetchedItems.size()-1);
            }
            else {
                std::unique_ptr<TreeNode> resNode = TreeNode::make_unique(resultPixelId(firstNode, secondNode), TreeNode::IS_INTERNAL);

                if (firstNode.isInternal() && secondNode.isInternal()) {
                    auto fIt = firstNode.children().begin();
                    auto fEnd = firstNode.children().end();
                    auto sIt = secondNode.children().begin();
                    auto sEnd = secondNode.children().end();
                    for(;fIt != fEnd && sIt != sEnd;) {
                        if ((*fIt)->pixelId() < (*sIt)->pixelId()) {
                            ++fIt;
                        }
                        else if ((*fIt)->pixelId() > (*sIt)->pixelId()) {
                            ++sIt;
                        }
                        else {
                            auto x = (*this)(**fIt, **sIt);
                            if (x) {
                                resNode->children().emplace_back(std::move(x));
                            }
                            ++fIt;
							++sIt;
                        }
                    }
                }
                else if (firstNode.isInternal()) {
                    auto fIt = firstNode.children().begin();
                    auto fEnd = firstNode.children().end();
                    for( ;fIt != fEnd; ++fIt) {
                        auto x = (*this)(**fIt, secondNode);
                        if (x) {
                            resNode->children().emplace_back(std::move(x));
                        }
                    }
                }
                else if (secondNode.isInternal()) {
                    auto sIt = secondNode.children().begin();
                    auto sEnd = secondNode.children().end();
                    for( ;sIt != sEnd; ++sIt) {
                        auto x = (*this)(firstNode, **sIt);
                        if (x) {
                            resNode->children().emplace_back(std::move(x));
                        }
                    }
                }

                if (resNode->children().size()) {
                    return resNode;
                }
                else {
                    return std::unique_ptr<TreeNode>();
                }
            }
        }
    };
    sserialize::RCPtrWrapper<Self> dest( new Self(m_items, m_sg, m_sgi) );
	if (m_root && static_cast<Self const &>(other).m_root) {
		Recurser rec(*this, static_cast<Self const &>(other), *dest);
		dest->m_root = rec(*(this->m_root), *(static_cast<Self const &>(other).m_root));
	}
	SSERIALIZE_EXPENSIVE_ASSERT_EQUAL(items() - other.items(), dest->items());
    return dest;
}

HCQRSpatialGrid::HCQRPtr
HCQRSpatialGrid::compactified(SizeType maxPMLevel) const {
    struct Recurser {
        HCQRSpatialGrid const & that;
        HCQRSpatialGrid & dest;
        SizeType maxPMLevel;
        Recurser(HCQRSpatialGrid const & that, HCQRSpatialGrid & dest, SizeType maxPMLevel) :
        that(that),
        dest(dest),
        maxPMLevel(maxPMLevel)
        {}
        std::unique_ptr<TreeNode> operator()(TreeNode const & node) const {
			SSERIALIZE_NORMAL_ASSERT(node.valid());
            if (node.isInternal()) {
                TreeNode::Children children;
				int flags = 0;
                for(auto const & x : node.children()) {
                    children.emplace_back((*this)(*x));
					flags |= children.back()->flags();
                }
                //check if we can compactify even further by merging partial-match indexes to parent nodes
                if ((flags & (TreeNode::IS_FULL_MATCH | TreeNode::IS_INTERNAL)) == 0 && that.sg().level(node.pixelId()) > maxPMLevel) {
                    sserialize::SizeType dataSize = 0;
                    std::vector<sserialize::ItemIndex> indexes;
                    for(auto & x : children) {
                        indexes.emplace_back( that.items(*x) );
                        dataSize += indexes.back().getSizeInBytes();
                    }
                    sserialize::ItemIndex merged = sserialize::ItemIndex::unite(indexes);
                    if (merged.getSizeInBytes() < dataSize) {
                        //we first have to get rid of those extra fetched indexes
                        for(auto rit(children.rbegin()), rend(children.rend()); rit != rend; ++rit) {
                            if ((*rit)->isFetched()) {
                                SSERIALIZE_CHEAP_ASSERT((*rit)->itemIndexId() == dest.m_fetchedItems.size()-1);
                                dest.m_fetchedItems.pop_back();
                            }
                        }
                        dest.m_fetchedItems.emplace_back(merged);
                        auto result = TreeNode::make_unique(node.pixelId(), TreeNode::IS_FETCHED, dest.m_fetchedItems.size()-1);
						SSERIALIZE_EXPENSIVE_ASSERT_EQUAL(that.items(node), dest.items(*result));
						return result;
                    } 
                }
                //merging was not possible
                if ((flags & (~TreeNode::IS_FULL_MATCH)) || that.sg().childrenCount(node.pixelId()) != children.size()) {
                    auto result = node.shallowCopy(std::move(children));
					SSERIALIZE_EXPENSIVE_ASSERT_EQUAL(that.items(node), dest.items(*result));
					return result;
                }
                else {
					auto result = TreeNode::make_unique(node.pixelId(), TreeNode::IS_FULL_MATCH);
					SSERIALIZE_EXPENSIVE_ASSERT_EQUAL(that.items(node), dest.items(*result));
					return result;
                }
            }
            else if (node.isFetched()) {
                dest.m_fetchedItems.emplace_back( that.items(node) );
                auto result = node.shallowCopy(dest.m_fetchedItems.size()-1);
				SSERIALIZE_EXPENSIVE_ASSERT_EQUAL(that.items(node), dest.items(*result));
				return result;
            }
            else {
                auto result = node.shallowCopy();
				SSERIALIZE_EXPENSIVE_ASSERT_EQUAL(that.items(node), dest.items(*result));
				return result;
            }
        };
    };

    sserialize::RCPtrWrapper<Self> dest( new Self(m_items, m_sg, m_sgi) );
	if (m_root) {
		Recurser rec(*this, *dest, maxPMLevel);
		dest->m_root = rec(*m_root);
	}
    return dest;
}


HCQRSpatialGrid::HCQRPtr
HCQRSpatialGrid::expanded(SizeType level) const {
    struct Recurser: HCQRSpatialGridOpHelper {
        HCQRSpatialGrid const & that;
        SizeType level;
        Recurser(HCQRSpatialGrid & dest, HCQRSpatialGrid const & that, SizeType level) :
        HCQRSpatialGridOpHelper(dest),
        that(that),
        level(level)
        {}
        std::unique_ptr<TreeNode> operator()(TreeNode const & node) {
			SSERIALIZE_NORMAL_ASSERT(node.valid());
            return rec(node, 0);
        }
        std::unique_ptr<TreeNode> rec(TreeNode const & node, SizeType myLevel) {
			SSERIALIZE_NORMAL_ASSERT(node.valid());
            if (myLevel >= level) {
                return deepCopy(that, node);
            }
            if (node.isInternal()) {
                TreeNode::Children children;
                for(auto const & x : node.children()) {
                    children.emplace_back( rec(*x, myLevel+1) );
                }
                return node.shallowCopy(std::move(children));
            }
            else if (node.isFullMatch()) {
                auto result = node.shallowCopy();
                expandFullMatchNode(*result, myLevel);
                return result;
            }
            else {
                auto result = TreeNode::make_unique(node.pixelId(), TreeNode::IS_INTERNAL);
                expandPartialMatchNode(*result, myLevel, that.items(node));
                return result;
            }
        }
        void expandFullMatchNode(TreeNode & node, SizeType myLevel) {
			SSERIALIZE_NORMAL_ASSERT(node.valid());
            if (myLevel >= level || !that.sg().childrenCount(node.pixelId())) {
                return;
            }
            node.setFlags(TreeNode::IS_INTERNAL);
            auto childrenCount = that.sg().childrenCount(node.pixelId());
            for(decltype(childrenCount) i(0); i < childrenCount; ++i) {
                node.children().emplace_back(TreeNode::make_unique(that.sg().index(node.pixelId(), i), TreeNode::IS_FULL_MATCH));
            }
            sortChildren(node);
            for(auto & x : node.children()) {
                expandFullMatchNode(*x, myLevel+1);
            }
        }
        void expandPartialMatchNode(TreeNode & node, SizeType myLevel, sserialize::ItemIndex const & items) {
			SSERIALIZE_NORMAL_ASSERT(node.valid());
            if (myLevel >= level || !that.sg().childrenCount(node.pixelId())) {
                dest.m_fetchedItems.emplace_back(items);
                node.setItemIndexId(dest.m_fetchedItems.size()-1);
                node.setFlags(TreeNode::IS_FETCHED);
                return;
            }
            auto childrenCount = that.sg().childrenCount(node.pixelId());
            for(decltype(childrenCount) i(0); i < childrenCount; ++i) {
                auto childPixelId = that.sg().index(node.pixelId(), i);
                sserialize::ItemIndex childFmIdx = that.sgi().items(childPixelId);
                sserialize::ItemIndex childPmIdx = childFmIdx / items;
                node.children().emplace_back(TreeNode::make_unique(childPixelId, TreeNode::IS_INTERNAL));
                expandPartialMatchNode(*node.children().back(), myLevel+1, childPmIdx);
            }
        }
    };
    sserialize::RCPtrWrapper<Self> dest( new Self(m_items, m_sg, m_sgi) );
	if (m_root) {
		Recurser rec(*dest, *this, level);
		dest->m_root = rec(*m_root);
	}
    return dest;
}

HCQRSpatialGrid::HCQRPtr
HCQRSpatialGrid::allToFull() const {
    struct Recurser {
        std::unique_ptr<TreeNode> operator()(TreeNode const & node) {
			SSERIALIZE_NORMAL_ASSERT(node.valid());
            if (node.isInternal()) {
                TreeNode::Children children;
                for(auto const & x : node.children()) {
                    children.emplace_back((*this)(*x));
                }
                return node.shallowCopy(std::move(children));
            }
            else {
                return TreeNode::make_unique(node.pixelId(), TreeNode::IS_FULL_MATCH);
            }
        }
    };
    sserialize::RCPtrWrapper<Self> dest( new Self(m_items, m_sg, m_sgi) );
	if (m_root) {
		Recurser rec;
		dest->m_root = rec(*m_root);
	}
    return dest;
}

HCQRSpatialGrid::TreeNode
HCQRSpatialGrid::root() const {
	return tree().get( tree().rootNodePosition() );
}

HCQRSpatialGrid::TreeNodePosition
HCQRSpatialGrid::rootNodePosition() const {
	return tree().rootNodePosition();
}

sserialize::ItemIndex
HCQRSpatialGrid::items(TreeNodePosition const & np) const {
	SSERIALIZE_NORMAL_ASSERT(node.valid());
	auto node = tree().get(np);
	if (node.isInternal()) {
		std::vector<ItemIndex> tmp;
		for(auto cit(tree().children(np); cit.valid(); cit.next()) {
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
