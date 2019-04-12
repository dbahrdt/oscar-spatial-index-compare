#pragma once

#include "HCQRIndex.h"

namespace hic {
	
class HCQRCompleter {
public:
	using HCQRIndex = hic::interface::HCQRIndex;
	using HCQRIndexPtr = sserialize::RCPtrWrapper<HCQRIndex>;
public:
	HCQRCompleter(HCQRIndexPtr const & index);
	~HCQRCompleter();
public:
	sserialize::RCPtrWrapper<hic::interface::HCQR> complete(std::string const & str);
private:
	HCQRIndexPtr m_d;
};
	
}//end namespace hic
