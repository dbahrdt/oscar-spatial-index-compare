#include "HCQRIndexMakeStatic.h"

#include "StaticHCQRSpatialGrid.h"

namespace hic {

HCQRIndexMakeStatic::HCQRIndexMakeStatic(HCQRIndexPtr const & base) :
m_base(base)
{}

HCQRIndexMakeStatic::~HCQRIndexMakeStatic() {}

HCQRIndexMakeStatic::HCQRIndexPtr
HCQRIndexMakeStatic::make(HCQRIndexPtr const & base) {
	return HCQRIndexPtr( new HCQRIndexMakeStatic(base) );
}

sserialize::StringCompleter::SupportedQuerries
HCQRIndexMakeStatic::getSupportedQueries() const {
    return m_base->getSupportedQueries();
}

HCQRIndexMakeStatic::HCQRPtr
HCQRIndexMakeStatic::complete(const std::string & qstr, const sserialize::StringCompleter::QuerryType qt) const {
	auto tmp = m_base->complete(qstr, qt);
	
	if (!dynamic_cast<hic::impl::HCQRSpatialGrid const *>(tmp.get())) {
		throw sserialize::TypeMissMatchException("HCQRMakeStatic: can only convert from hic::impl::HCQRSpatialGrid to hic::Static::impl::HCQRSpatialGrid");
	}
	
	return HCQRPtr(new hic::Static::impl::HCQRSpatialGrid( static_cast<hic::impl::HCQRSpatialGrid const &>(*tmp) ) );
}

HCQRIndexMakeStatic::HCQRPtr
HCQRIndexMakeStatic::items(const std::string & qstr, const sserialize::StringCompleter::QuerryType qt) const {
	auto tmp = m_base->items(qstr, qt);
	
	if (!dynamic_cast<hic::impl::HCQRSpatialGrid const *>(tmp.get())) {
		throw sserialize::TypeMissMatchException("HCQRMakeStatic: can only convert from hic::impl::HCQRSpatialGrid to hic::Static::impl::HCQRSpatialGrid");
	}
	
	return HCQRPtr(new hic::Static::impl::HCQRSpatialGrid( static_cast<hic::impl::HCQRSpatialGrid const &>(*tmp) ) );
}

HCQRIndexMakeStatic::HCQRPtr
HCQRIndexMakeStatic::regions(const std::string & qstr, const sserialize::StringCompleter::QuerryType qt) const {
	auto tmp = m_base->regions(qstr, qt);

	if (!dynamic_cast<hic::impl::HCQRSpatialGrid const *>(tmp.get())) {
		throw sserialize::TypeMissMatchException("HCQRMakeStatic: can only convert from hic::impl::HCQRSpatialGrid to hic::Static::impl::HCQRSpatialGrid");
	}
	
	return HCQRPtr(new hic::Static::impl::HCQRSpatialGrid( static_cast<hic::impl::HCQRSpatialGrid const &>(*tmp) ) );
}

HCQRIndexMakeStatic::HCQRPtr
HCQRIndexMakeStatic::cell(uint32_t cellId) const {
	auto tmp = m_base->cell(cellId);

	if (!dynamic_cast<hic::impl::HCQRSpatialGrid const *>(tmp.get())) {
		throw sserialize::TypeMissMatchException("HCQRMakeStatic: can only convert from hic::impl::HCQRSpatialGrid to hic::Static::impl::HCQRSpatialGrid");
	}
	
	return HCQRPtr(new hic::Static::impl::HCQRSpatialGrid( static_cast<hic::impl::HCQRSpatialGrid const &>(*tmp) ) );
}

HCQRIndexMakeStatic::HCQRPtr
HCQRIndexMakeStatic::region(uint32_t regionId) const {
	auto tmp = m_base->region(regionId);

	if (!dynamic_cast<hic::impl::HCQRSpatialGrid const *>(tmp.get())) {
		throw sserialize::TypeMissMatchException("HCQRMakeStatic: can only convert from hic::impl::HCQRSpatialGrid to hic::Static::impl::HCQRSpatialGrid");
	}
	
	return HCQRPtr(new hic::Static::impl::HCQRSpatialGrid( static_cast<hic::impl::HCQRSpatialGrid const &>(*tmp) ) );
	
}

HCQRIndexMakeStatic::SpatialGridInfo const &
HCQRIndexMakeStatic::sgi() const {
    return m_base->sgi();
}

HCQRIndexMakeStatic::SpatialGrid const &
HCQRIndexMakeStatic::sg() const {
    return m_base->sg();
}

}//end namespace hic
