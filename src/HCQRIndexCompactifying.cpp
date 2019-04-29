#include "HCQRIndexCompactifying.h"


namespace hic {

HCQRIndexCompactifying::HCQRIndexCompactifying(HCQRIndexPtr const & base) :
m_base(base),
m_cl(std::numeric_limits<uint32_t>::max())
{}

HCQRIndexCompactifying::~HCQRIndexCompactifying() {}

HCQRIndexCompactifying::HCQRIndexPtr
HCQRIndexCompactifying::make(HCQRIndexPtr const & base) {
	return HCQRIndexPtr( new HCQRIndexCompactifying(base) );
}

void
HCQRIndexCompactifying::setCompactLevel(uint32_t size) {
	m_cl = size;
}

sserialize::StringCompleter::SupportedQuerries
HCQRIndexCompactifying::getSupportedQueries() const {
    return m_base->getSupportedQueries();
}

HCQRIndexCompactifying::HCQRPtr
HCQRIndexCompactifying::complete(const std::string & qstr, const sserialize::StringCompleter::QuerryType qt) const {
    auto tmp = m_base->complete(qstr, qt);
	auto result = tmp->compactified(m_cl);
	SSERIALIZE_EXPENSIVE_ASSERT_EQUAL(tmp->items(), result->items());
	return result;
}

HCQRIndexCompactifying::HCQRPtr
HCQRIndexCompactifying::items(const std::string & qstr, const sserialize::StringCompleter::QuerryType qt) const {
    auto tmp = m_base->items(qstr, qt);
	auto result = tmp->compactified(m_cl);
	SSERIALIZE_EXPENSIVE_ASSERT_EQUAL(tmp->items(), result->items());
	return result;
}

HCQRIndexCompactifying::HCQRPtr
HCQRIndexCompactifying::regions(const std::string & qstr, const sserialize::StringCompleter::QuerryType qt) const {
    auto tmp = m_base->regions(qstr, qt);
	auto result = tmp->compactified(m_cl);
	SSERIALIZE_EXPENSIVE_ASSERT_EQUAL(tmp->items(), result->items());
	return result;
}

HCQRIndexCompactifying::HCQRPtr
HCQRIndexCompactifying::cell(uint32_t cellId) const {
    auto tmp = m_base->cell(cellId);
	auto result = tmp->compactified(m_cl);
	SSERIALIZE_EXPENSIVE_ASSERT_EQUAL(tmp->items(), result->items());
	return result;
}

HCQRIndexCompactifying::HCQRPtr
HCQRIndexCompactifying::region(uint32_t regionId) const {
    auto tmp = m_base->region(regionId);
	auto result = tmp->compactified(m_cl);
	SSERIALIZE_EXPENSIVE_ASSERT_EQUAL(tmp->items(), result->items());
	return result;
	
}

HCQRIndexCompactifying::SpatialGridInfo const &
HCQRIndexCompactifying::sgi() const {
    return m_base->sgi();
}

HCQRIndexCompactifying::SpatialGrid const &
HCQRIndexCompactifying::sg() const {
    return m_base->sg();
}

}//end namespace hic
