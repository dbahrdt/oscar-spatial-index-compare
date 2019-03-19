#pragma once
#include "HCQR.h"
#include "HCQRIndex.h"
#include <liboscar/AdvancedOpTree.h>

namespace hic {

class HcqrOpTree: public liboscar::AdvancedOpTree {
public:
    using SearchIndex = sserialize::RCPtrWrapper<hic::interface::HCQRIndex>;
    using HCQRPtr = hic::interface::HCQR::HCQRPtr;
public:
    HcqrOpTree(SearchIndex const & si);
    virtual ~HcqrOpTree();
public:
    HCQRPtr calc();
private:
    class Calc final {
    public:
        Calc(SearchIndex const & d) : m_d(d) {}
        ~Calc() {}
        HCQRPtr calc(const Node * node);
    private:
        SearchIndex m_d;
    };
private:
    SearchIndex m_d;
};

} //end namespace hic