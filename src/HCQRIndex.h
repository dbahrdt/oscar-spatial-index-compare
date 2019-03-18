#pragma once

#include <sserialize/search/StringCompleter.h>

#include "HCQR.h"

namespace hic::interface {

class HCQRIndex: public sserialize::RefCountObject {
public:
    using Self = HCQRIndex;
    using HCQRPtr = hic::interface::HCQR::HCQRPtr;
    using SpatialGrid = hic::interface::SpatialGrid;
    using SpatialGridInfo = hic::interface::SpatialGridInfo;
public:
    HCQRIndex() {}
    virtual ~HCQRIndex() {}
public:
    virtual sserialize::StringCompleter::SupportedQuerries getSupportedQueries() const = 0;

	virtual HCQRPtr complete(const std::string & qstr, const sserialize::StringCompleter::QuerryType qt) const = 0;
	virtual HCQRPtr items(const std::string & qstr, const sserialize::StringCompleter::QuerryType qt) const = 0;
	virtual HCQRPtr regions(const std::string & qstr, const sserialize::StringCompleter::QuerryType qt) const = 0;
public:
	virtual SpatialGridInfo const & sgi() const = 0;
	virtual SpatialGrid const & sg() const = 0;
};


}//end namespace hic::interface