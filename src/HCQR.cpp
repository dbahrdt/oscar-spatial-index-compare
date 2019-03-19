#include "HCQR.h"
#include <sserialize/utility/exceptions.h>
#include <memory>

namespace hic::interface {
	

HCQR::HCQR() {}
HCQR::~HCQR() {}
	
}//end namespace hic::interface

namespace hic::impl::detail::HCQRSpatialGrid {

TreeNode::TreeNode(PixelId pixelId, int flags, uint32_t itemIndexId) :
m_pid(pixelId),
m_f(flags),
m_itemIndexId(itemIndexId)
{}

std::unique_ptr<TreeNode>
TreeNode::make_unique(PixelId pixelId, int flags, uint32_t itemIndexId) {
	return std::unique_ptr<TreeNode>( new TreeNode(pixelId, flags, itemIndexId) );
}

std::unique_ptr<TreeNode>
TreeNode::shallowCopy() const {
    SSERIALIZE_CHEAP_ASSERT(!isFetched());
    return TreeNode::make_unique(pixelId(), m_f, m_itemIndexId);
}

std::unique_ptr<TreeNode>
TreeNode::shallowCopy(uint32_t fetchedItemIndexId) const {
    SSERIALIZE_CHEAP_ASSERT(isFetched());
    return TreeNode::make_unique(pixelId(), m_f, fetchedItemIndexId);
}

std::unique_ptr<TreeNode>
TreeNode::shallowCopy(Children && newChildren) const {
    SSERIALIZE_CHEAP_ASSERT(isInternal())
    auto result = TreeNode::make_unique(pixelId(), m_f);
    result->children() = std::move(newChildren);
    return result;
}

}//end namespace hic::impl::detail::HCQRSpatialGrid

namespace hic::impl {

HCQRSpatialGrid::HCQRSpatialGrid(
	sserialize::Static::ItemIndexStore idxStore,
	sserialize::RCPtrWrapper<hic::interface::SpatialGrid> sg,
	sserialize::RCPtrWrapper<hic::interface::SpatialGridInfo> sgi
) :
m_items(idxStore),
m_sg(sg),
m_sgi(sgi)
{}

HCQRSpatialGrid::SizeType
HCQRSpatialGrid::depth() const {
    struct Recurser {
        SizeType operator()(TreeNode & node) const {
            if (node.children().size()) {
                SizeType tmp = 0;
                for(auto const & x : node.children()) {
                    tmp = std::max(tmp, (*this)(*x));
                }
                return tmp+1;
            }
            else {
                return 0;
            }
        }
    };
    return Recurser()(*m_root);
}

HCQRSpatialGrid::SizeType
HCQRSpatialGrid::numberOfItems() const {
    struct Recurser {
        HCQRSpatialGrid const & that;
        SizeType numberOfItems{0};
        void operator()(TreeNode const & node) {
            if (node.isInternal()) {
                for(auto const & x : node.children()) {
                    (*this)(*x);
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
        Recurser(HCQRSpatialGrid const & that) : that(that) {}
    };
    Recurser r(*this);
    r(*m_root);
    return r.numberOfItems;
}

HCQRSpatialGrid::ItemIndex
HCQRSpatialGrid::items() const {
    struct Recurser {
        HCQRSpatialGrid const & that;
        ItemIndex operator()(std::unique_ptr<TreeNode> const & node) {
            return (*this)(*node);
        }
        ItemIndex operator()(TreeNode const & node) {
            if (node.isInternal()) {
                std::vector<ItemIndex> tmp;
                for(auto const & x : node.children()) {
                    tmp.emplace_back((*this)(*x));
                }
                return ItemIndex::unite(tmp);
            }
            else if (node.isFullMatch()) {
                return that.sgi().items(node.pixelId());
            }
            else if (node.isFetched()) {
                return that.fetchedItems().at(node.itemIndexId());
            }
            else {
                return that.idxStore().at(node.itemIndexId());
            }
        }
        Recurser(HCQRSpatialGrid const & that) : that(that) {}
    };
    Recurser r(*this);
    return r(*m_root);;
}

struct HCQRSpatialGrid::HCQRSpatialGridOpHelper {
    HCQRSpatialGrid & dest;
    HCQRSpatialGridOpHelper(HCQRSpatialGrid & dest) : dest(dest) {}
    std::unique_ptr<HCQRSpatialGrid::TreeNode> deepCopy(HCQRSpatialGrid const & src, HCQRSpatialGrid::TreeNode const & node) {
        std::unique_ptr<TreeNode> result;
        if (node.isFetched()) {
            result = node.shallowCopy(dest.m_fetchedItems.size());
            dest.m_fetchedItems.emplace_back( src.items(node) );
        }
        else {
            result = node.shallowCopy();
        }

        for(auto const & x : node.children()) {
            result->children().emplace_back( this->deepCopy(src, *x) );
        }
        return std::move(result);
    }

    PixelId resultPixelId(HCQRSpatialGrid::TreeNode const & first, HCQRSpatialGrid::TreeNode const & second) const {
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
            if (firstNode.isFullMatch()) {
                return deepCopy(secondSg, secondNode);
            }
            else if (secondNode.isFullMatch()) {
                return deepCopy(firstSg, firstNode);
            }
            else if (firstNode.isLeaf() && secondNode.isLeaf()) {
                auto result = firstSg.items(firstNode) / secondSg.items(secondNode);
                if (result.size()) {
                    return std::unique_ptr<TreeNode>();
                }
                dest.m_fetchedItems.emplace_back(result);
                return TreeNode::make_unique(resultPixelId(firstNode, secondNode), TreeNode::IS_FETCHED, dest.m_fetchedItems.size()-1);
            }
            else {
                std::unique_ptr<TreeNode> resNode = TreeNode::make_unique(resultPixelId(firstNode, secondNode));

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
    Recurser rec(*this, static_cast<Self const &>(other), *dest);
    dest->m_root = rec(*(this->m_root), *(static_cast<Self const &>(other).m_root));
    return dest;
}

HCQRSpatialGrid::HCQRPtr
HCQRSpatialGrid::operator+(Parent::Self const & other) const {
    throw sserialize::UnimplementedFunctionException("HCQRSpatialGrid::operator+");
    return HCQRPtr();
}

HCQRSpatialGrid::HCQRPtr
HCQRSpatialGrid::operator-(Parent::Self const & other) const {
    throw sserialize::UnimplementedFunctionException("HCQRSpatialGrid::operator+");
    return HCQRPtr();
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
            if (node.isInternal()) {
                TreeNode::Children children;
                bool hasFmChild = false;
                bool hasPmChild = false;
                bool childHasChildren = false;
                for(auto const & x : node.children()) {
                    children.emplace_back((*this)(*x));
                    hasFmChild = hasFmChild || children.back()->isFullMatch();
                    hasPmChild = hasPmChild || !children.back()->isFullMatch();
                    childHasChildren = childHasChildren || children.back()->children().size();
                }
                //check if we can compactify even further by merging partial-match indexes to parent nodes
                if (!hasFmChild && !childHasChildren && that.sg().level(node.pixelId()) > maxPMLevel) {
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
                        return TreeNode::make_unique(node.pixelId(), TreeNode::IS_FETCHED, dest.m_fetchedItems.size()-1);
                    } 
                }
                //merging was not possible
                if (hasPmChild || that.sg().childrenCount(node.pixelId()) != children.size()) {
                    return node.shallowCopy(std::move(children));
                }
                else {
                    return TreeNode::make_unique(node.pixelId(), TreeNode::IS_FULL_MATCH);
                }
            }
            else if (node.isFetched()) {
                dest.m_fetchedItems.emplace_back( that.items(node) );
                return node.shallowCopy(dest.m_fetchedItems.size()-1);
            }
            else {
                return node.shallowCopy();
            }
        };
    };

    sserialize::RCPtrWrapper<Self> dest( new Self(m_items, m_sg, m_sgi) );
    Recurser rec(*this, *dest, maxPMLevel);
    dest->m_root = rec(*m_root);
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
            return rec(node, 0);
        }
        std::unique_ptr<TreeNode> rec(TreeNode const & node, SizeType myLevel) {
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
            if (myLevel >= level) {
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
            if (myLevel >= level) {
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
    Recurser rec(*dest, *this, level);
    dest->m_root = rec(*m_root);
    return dest;
}

HCQRSpatialGrid::HCQRPtr
HCQRSpatialGrid::allToFull() const {
    struct Recurser {
        std::unique_ptr<TreeNode> operator()(TreeNode const & node) {
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
    Recurser rec;
    dest->m_root = rec(*m_root);
    return dest;
}

sserialize::ItemIndex
HCQRSpatialGrid::items(TreeNode const & node) const {
	if (node.isFullMatch()) {
		return m_sgi->items(node.pixelId());
	}
	else if (node.isFetched()) {
		return m_fetchedItems.at(node.itemIndexId());
	}
	else {
		return m_items.at(node.itemIndexId());
	}
	
}

} //end namespace hic::impl
