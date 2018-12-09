#include "static-htm-index.h"
#include <sserialize/strings/unicode_case_functions.h>

namespace hic::Static {

OscarSearchHtmIndex::OscarSearchHtmIndex(const sserialize::UByteArrayAdapter & d, const sserialize::Static::ItemIndexStore & idxStore) :
m_sq(d.at(1)),
m_htmLevels(d.at(2)),
m_trie( Trie::PrivPtrType(new FlatTrieType(d+3)) ),
m_idxStore(idxStore),
m_hp(m_htmLevels)
{
	SSERIALIZE_VERSION_MISSMATCH_CHECK(MetaData::version, d.at(0), "hic::Static::OscarSearchHtmIndex");
}


sserialize::RCPtrWrapper<OscarSearchHtmIndex>
OscarSearchHtmIndex::make(const sserialize::UByteArrayAdapter & d, const sserialize::Static::ItemIndexStore & idxStore) {
    return sserialize::RCPtrWrapper<OscarSearchHtmIndex>( new OscarSearchHtmIndex(d, idxStore) );
}

OscarSearchHtmIndex::~OscarSearchHtmIndex() {}

sserialize::UByteArrayAdapter::SizeType
OscarSearchHtmIndex::getSizeInBytes() const {
    return 0;
}

sserialize::Static::ItemIndexStore const &
OscarSearchHtmIndex::idxStore() const {
    return m_idxStore;
}

int
OscarSearchHtmIndex::flags() const {
    return m_flags;
}

std::ostream &
OscarSearchHtmIndex::printStats(std::ostream & out) const {
	out << "OscarSearchHtmIndex::BEGIN_STATS" << std::endl;
	m_trie.printStats(out);
	out << "OscarSearchHtmIndex::END_STATS" << std::endl;
	return out;
}

sserialize::StringCompleter::SupportedQuerries
OscarSearchHtmIndex::getSupportedQueries() const {
    return sserialize::StringCompleter::SupportedQuerries(m_sq);
}

OscarSearchHtmIndex::Payload::Type
OscarSearchHtmIndex::typeFromCompletion(const std::string& qs, const sserialize::StringCompleter::QuerryType qt) const {
	std::string qstr;
	if (m_sq & sserialize::StringCompleter::SQ_CASE_INSENSITIVE) {
		qstr = sserialize::unicode_to_lower(qs);
	}
	else {
		qstr = qs;
	}
	Payload p( m_trie.at(qstr, (qt & sserialize::StringCompleter::QT_SUBSTRING || qt & sserialize::StringCompleter::QT_PREFIX)) );
	Payload::Type t;
	if (p.types() & qt) {
		t = p.type(qt);
	}
	else if (qt & sserialize::StringCompleter::QT_SUBSTRING) {
		if (p.types() & sserialize::StringCompleter::QT_PREFIX) { //exact suffix matches are either available or not
			t = p.type(sserialize::StringCompleter::QT_PREFIX);
		}
		else if (p.types() & sserialize::StringCompleter::QT_SUFFIX) {
			t = p.type(sserialize::StringCompleter::QT_SUFFIX);
		}
		else if (p.types() & sserialize::StringCompleter::QT_EXACT) {
			t = p.type(sserialize::StringCompleter::QT_EXACT);
		}
		else {
			throw sserialize::OutOfBoundsException("OscarSearchHtmIndex::typeFromCompletion");
		}
	}
	else if (p.types() & sserialize::StringCompleter::QT_EXACT) { //qt is either prefix, suffix, exact
		t = p.type(sserialize::StringCompleter::QT_EXACT);
	}
	else {
		throw sserialize::OutOfBoundsException("OscarSearchHtmIndex::typeFromCompletion");
	}
	return t;
}

}//end namespace hic::Static