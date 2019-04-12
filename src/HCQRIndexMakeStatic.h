#pragma once

#include "HCQRIndex.h"

namespace hic {

class HCQRIndexMakeStatic: public hic::interface::HCQRIndex {
public:
    using HCQRIndexPtr = sserialize::RCPtrWrapper<hic::interface::HCQRIndex>;
public:
    HCQRIndexMakeStatic(HCQRIndexPtr const & base);
    ~HCQRIndexMakeStatic() override;
	static HCQRIndexPtr make(HCQRIndexPtr const & base);
public:
    void setCacheSize(uint32_t size);
public:
    sserialize::StringCompleter::SupportedQuerries getSupportedQueries() const override;
public:
	HCQRPtr complete(const std::string & qstr, const sserialize::StringCompleter::QuerryType qt) const override;
	HCQRPtr items(const std::string & qstr, const sserialize::StringCompleter::QuerryType qt) const override;
	HCQRPtr regions(const std::string & qstr, const sserialize::StringCompleter::QuerryType qt) const override;
public:
	SpatialGridInfo const & sgi() const override;
	SpatialGrid const & sg() const override;
private:
    HCQRIndexPtr m_base;
};

}//end namespace hic
