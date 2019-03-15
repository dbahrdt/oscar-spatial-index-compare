#include "HCQR.h"
#include <sserialize/utility/exceptions.h>

namespace hic::impl {

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
            if (node.fullMatch()) {
                numberOfItems += that.m_items.idxSize( that.sgi().itemIndexId(node.pixelId()) );
            }
            else {
                if (node.isFetched()) {
                    numberOfItems += that.fetchedItems().at(node.itemIndexId()).size();
                }
                else {
                    numberOfItems += that.items().idxSize(node.itemIndexId());
                }
            }
        }
        Recurser(HCQRSpatialGrid const &  that) : that(that) {}
    };
    Recurser r(*this);
    r(*m_root);
    return r.numberOfItems;
}

struct HCQRSpatialGrid::HCQRSpatialGridOpHelper {
    HCQRSpatialGrid & dest;
    HCQRSpatialGridOpHelper(HCQRSpatialGrid & dest) : dest(dest) {}
    std::unique_ptr<HCQRSpatialGrid::TreeNode> deepCopy(HCQRSpatialGrid const & src, HCQRSpatialGrid::TreeNode const & node) {
        std::unique_ptr<TreeNode> result;
        if (node.isFetched()) {
            uint32_t nodeIdxId = dest.m_fetchedItems.size();
            dest.m_fetchedItems.emplace_back( src.items(node) );
            result = TreeNode::make_unique(node.pixelId(), TreeNode::IS_FETCHED, nodeIdxId);
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
};

HCQRSpatialGrid::OperatorReturnValue
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
            if (firstNode.fullMatch()) {
                return deepCopy(secondSg, secondNode);
            }
            else if (secondNode.fullMatch()) {
                return deepCopy(firstSg, firstNode);
            }
            else {
                std::unique_ptr<TreeNode> resNode;
                sserialize::ItemIndex resNodeItems = firstSg.items(firstNode) / secondSg.items(secondNode);
                
                if (resNodeItems.size()) {
                    uint32_t idxId = dest.m_fetchedItems.size();
                    dest.m_fetchedItems[idxId] = resNodeItems; 
                    resNode = TreeNode::make_unique(resultPixelId(firstNode, secondNode), TreeNode::IS_FETCHED, idxId);
                }
                else {
                    resNode = TreeNode::make_unique(resultPixelId(firstNode, secondNode));
                }

                if (firstNode.children().size() && secondNode.children().size()) {
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
                else if (firstNode.children().size()) {
                    auto fIt = firstNode.children().begin();
                    auto fEnd = firstNode.children().end();
                    for( ;fIt != fEnd; ++fIt) {
                        auto x = (*this)(**fIt, secondNode);
                        if (x) {
                            resNode->children().emplace_back(std::move(x));
                        }
                    }
                }
                else if (secondNode.children().size()) {
                    auto sIt = secondNode.children().begin();
                    auto sEnd = secondNode.children().end();
                    for( ;sIt != sEnd; ++sIt) {
                        auto x = (*this)(firstNode, **sIt);
                        if (x) {
                            resNode->children().emplace_back(std::move(x));
                        }
                    }
                }

                if (resNode->isFetched() || resNode->children().size()) {
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

HCQRSpatialGrid::OperatorReturnValue
HCQRSpatialGrid::operator+(Parent::Self const & other) const {
    throw sserialize::UnimplementedFunctionException("HCQRSpatialGrid::operator+");
    return OperatorReturnValue();
}

HCQRSpatialGrid::OperatorReturnValue
HCQRSpatialGrid::operator-(Parent::Self const & other) const {
    throw sserialize::UnimplementedFunctionException("HCQRSpatialGrid::operator+");
    return OperatorReturnValue();
}

HCQRSpatialGrid::OperatorReturnValue
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
            if (node.children().size()) {
                std::vector<std::unique_ptr<TreeNode>> children;
                bool hasFmChild = false;
                bool hasPmChild = false;
                bool childHasChildren = false;
                for(auto const & x : node.children()) {
                    children.emplace_back((*this)(*x));
                    hasFmChild = hasFmChild || children.back()->fullMatch();
                    hasPmChild = hasPmChild || !children.back()->fullMatch();
                    childHasChildren = childHasChildren || children.back()->children().size();
                }
                //check if we can compactify even further by merging partial-match indexes to parent nodes
                if (!hasFmChild && !childHasChildren && that.sg().level(node.pixelId()) > maxPMLevel) {
                    sserialize::SizeType dataSize = 0;
                    std::vector<sserialize::ItemIndex> indexes;
                    if (node.hasIndex()) {
                        indexes.emplace_back( that.items(node) );
                        dataSize += indexes.back().getSizeInBytes();
                    }
                    for(auto & x : children) {
                        indexes.emplace_back( that.items(*x) );
                        dataSize += indexes.back().getSizeInBytes();
                    }
                    sserialize::ItemIndex merged = sserialize::ItemIndex::unite(indexes);
                    if (merged.getSizeInBytes() < dataSize) {
                        uint32_t idxId = dest.m_fetchedItems.size();
                        dest.m_fetchedItems.emplace_back(merged);
                        return TreeNode::make_unique(node.pixelId(), TreeNode::IS_FETCHED, idxId);
                    } 
                }
                //merging was not possible
                if (node.isFetched() || hasPmChild || that.sg().childrenCount(node.pixelId()) != children.size()) {
                    auto cpy = node.shallowCopy();
                    cpy->children() = std::move(children);
                    return cpy;
                }
                else {
                    return TreeNode::make_unique(node.pixelId(), TreeNode::IS_FULL_MATCH);
                }
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

HCQRSpatialGrid::OperatorReturnValue
HCQRSpatialGrid::allToFull() const {
    throw sserialize::UnimplementedFunctionException("HCQRSpatialGrid::operator+");
    return OperatorReturnValue();
}

} //end namespace hic::impl