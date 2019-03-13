#include "HCQR.h"
#include <sserialize/utility/exceptions.h>

namespace hic::impl {

HCQRSpatialGrid::SizeType
HCQRSpatialGrid::depth() const {

}

HCQRSpatialGrid::SizeType
HCQRSpatialGrid::numberOfItems() const {
    struct Recurser {
        const HCQRSpatialGrid * that;
        std::size_t numberOfItems{0};
        void operator()(TreeNode const & node) {
            if (node.fullMatch()) {
                numberOfItems += that->sgi().size(node.pixelId());
            }
            else {
                if (node.isFetched()) {
                    numberOfItems += that->fetchedItems().at(node.itemIndexId()).size();
                }
                else {
                    numberOfItems += that->items().idxSize(node.itemIndexId());
                }
            }
        }
        Recurser(const HCQRSpatialGrid * that) : that(that) {}
    };
    Recurser r(this);
    r(*m_root);
    return r.numberOfItems;
}

struct HCQRSpatialGrid::HCQRSpatialGridOpHelper {
    HCQRSpatialGrid & dest;
    HCQRSpatialGridOpHelper(HCQRSpatialGrid & dest) : dest(dest) {}
    std::unique_ptr<HCQRSpatialGrid::TreeNode> deepCopy(HCQRSpatialGrid const & src, HCQRSpatialGrid::TreeNode const & node);
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
                    resNode = TreeNode::make_unique(resultPixelId(firstNode, secondNode), false, true, idxId);
                }
                else {
                    resNode = TreeNode::make_unique(resultPixelId(firstNode, secondNode), false, false, 0);
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
HCQRSpatialGrid::compactified() const {
    throw sserialize::UnimplementedFunctionException("HCQRSpatialGrid::operator+");
    return OperatorReturnValue();
}

HCQRSpatialGrid::OperatorReturnValue
HCQRSpatialGrid::allToFull() const {
    throw sserialize::UnimplementedFunctionException("HCQRSpatialGrid::operator+");
    return OperatorReturnValue();
}

} //end namespace hic::impl