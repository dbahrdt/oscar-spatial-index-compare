#include <liboscar/StaticOsmCompleter.h>
#include "htm-index.h"

struct Config {
    int htmLevel{8};
	uint32_t threadCount{0};
    std::string filename;
	bool createSearch{false};
	std::vector<std::string> queries;
};

void help() {
	std::cerr << "prg -f <oscar files> -l <htm levels> --create-search -q <query> --tempdir <dir> -t <threadCount> --serialize <outdir>" << std::endl;
}

int main(int argc, char const * argv[] ) {
    Config cfg;

    for(int i(1); i < argc; ++i) {
        std::string token(argv[i]);
        if (token == "-l" && i+1 < argc ) {
            cfg.htmLevel = std::atoi(argv[i+1]);
            ++i;
        }
        else if (token == "-f" && i+1 < argc) {
            cfg.filename = std::string(argv[i+1]);
            ++i;
        }
        else if (token == "--create-search") {
			cfg.createSearch = true;
		}
		else if (token == "-q" && i+1 < argc) {
			cfg.queries.emplace_back(argv[i+1]);
			++i;
		}
		else if (token == "--tempdir" && i+1 < argc) {
			token = std::string(argv[i+1]);
			sserialize::UByteArrayAdapter::setFastTempFilePrefix(token);
			sserialize::UByteArrayAdapter::setTempFilePrefix(token);
			++i;
		}
        else if (token == "-t" && i+1 < argc ) {
            cfg.threadCount = std::atoi(argv[i+1]);
            ++i;
        }
        else {
            std::cerr << "Unkown parameter: " << token << std::endl;
			help();
            return -1;
        }
    }

    auto cmp = std::make_shared<liboscar::Static::OsmCompleter>();
    cmp->setAllFilesFromPrefix(cfg.filename);

    try {
        cmp->energize();
    }
    catch (std::exception const & e) {
        std::cerr << "Error occured: " << e.what() << std::endl;
		help();
		return -1;
    }

    auto ohi = std::make_shared<hic::OscarHtmIndex>(cmp->store(), cmp->indexStore(), cfg.htmLevel);
	
	std::cout << "Creating htm index..." << std::endl;
	ohi->create(cfg.threadCount);
	
	ohi->stats();
	
	if (cfg.createSearch || cfg.queries.size()) {
		
		auto oshi = std::make_shared<hic::OscarSearchHtmIndex>(cmp, ohi);
		
		oshi->idxFactory().setType(sserialize::ItemIndex::T_RLE_DE);
		oshi->idxFactory().setDeduplication(true);
		oshi->idxFactory().setIndexFile(sserialize::UByteArrayAdapter::createCache(1024, sserialize::MM_SLOW_FILEBASED));
		
		std::cout << "Creating search structures..." << std::endl;
		oshi->create(cfg.threadCount);
		
		auto oswh = std::make_shared<hic::OscarSearchWithHtm>(oshi);
		
		liboscar::AdvancedOpTree opTree;
		for(std::string const & q : cfg.queries) {
			opTree.parse(q);
			
			sserialize::CellQueryResult htmCqr;
			
			try {
				htmCqr = oswh->complete(opTree);
			}
			catch (const std::exception & e) {
				std::cerr << "Error occured while processing query: " << q << ":" << std::endl;
				std::cerr << e.what() << std::endl;
				continue;
			}
			
			
			auto oscarCqr = cmp->cqrComplete(q);
			
			sserialize::ItemIndex htmItems = htmCqr.flaten();
			sserialize::ItemIndex oscarItems = oscarCqr.flaten();
			
			if (htmItems != oscarItems) {
				std::cerr << "Oscar and htm index return differing results:" << std::endl;
				std::cerr << "Oscar::items.size: " << oscarItems.size() << std::endl;
				std::cerr << "Htm::items.size: " << htmItems.size() << std::endl;
				std::cerr << "(Oscar / Htm).size: " << (oscarItems / htmItems).size() << std::endl;
				std::cerr << "(Oscar - Htm).size: " << (oscarItems - htmItems).size() << std::endl;
				std::cerr << "(Htm - Oscar).size: " << (htmItems - oscarItems).size() << std::endl;
			}
			
			std::cout << "Query " << q << ": " << std::endl;
			std::cout << "#oscar cells: " << oscarCqr.cellCount() << std::endl;
			std::cout << "#htm cells: " << htmCqr.cellCount() << std::endl;
			std::cout << "#Items: " << oscarItems.size() << std::endl;
			
		}
		
	}
	
    return 0;
}
