#pragma once
#include <sserialize/spatial/dgg/HCQR.h>
#include <sserialize/spatial/dgg/HCQRIndex.h>
#include <liboscar/AdvancedOpTree.h>

namespace hic {

class HcqrOpTree: public liboscar::AdvancedOpTree {
public:
    using SearchIndex = sserialize::RCPtrWrapper<sserialize::spatial::dgg::interface::HCQRIndex>;
    using HCQRPtr = sserialize::spatial::dgg::interface::HCQR::HCQRPtr;
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
