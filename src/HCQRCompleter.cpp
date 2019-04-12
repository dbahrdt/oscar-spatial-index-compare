#include "HCQRCompleter.h"

#include "HcqrOpTree.h"

namespace hic {

HCQRCompleter::HCQRCompleter(HCQRIndexPtr const & index) :
m_d(index)
{}

HCQRCompleter::~HCQRCompleter() {}

sserialize::RCPtrWrapper<hic::interface::HCQR>
HCQRCompleter::complete(std::string const & str) {
	hic::HcqrOpTree opTree(m_d);
	opTree.parse(str);
	return opTree.calc();
}

}//end namespace hic
