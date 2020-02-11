#pragma once

#include <sserialize/spatial/dgg/HCQRIndex.h>

namespace hic {
	
class HCQRCompleter {
public:
	using HCQRIndex = sserialize::spatial::dgg::interface::HCQRIndex;
	using HCQRIndexPtr = sserialize::RCPtrWrapper<HCQRIndex>;
public:
	HCQRCompleter(HCQRIndexPtr const & index);
	~HCQRCompleter();
public:
	sserialize::RCPtrWrapper<sserialize::spatial::dgg::interface::HCQR> complete(std::string const & str);
private:
	HCQRIndexPtr m_d;
};
	
}//end namespace hic
