#include <liboscar/StaticOsmCompleter.h>
#include <sserialize/spatial/dgg/SimpleGridSpatialGrid.h>
#include "OscarSearchWithSg.h"
#include "H3SpatialGrid.h"
#include "HtmSpatialGrid.h"
#include "S2GeomSpatialGrid.h"

enum SearchType {
	ST_NONE,
	ST_NOOP,
	ST_MEM,
	ST_SS
};

enum IndexType {
	IT_HTM,
	IT_H3,
	IT_S2GEOM,
	IT_SIMPLE_GRID
};

struct Config {
    int levels{8};
	uint32_t threadCount{0};
	uint32_t searchCreationThreadCount{0};
    std::string filename;
	std::vector<std::string> queries;
	SearchType st{ST_NONE};
	IndexType it{IT_HTM};
};

void help() {
	std::cerr << "prg -f <oscar files> --index-type (htm|h3|simplegrid|s2geom) -l <htm levels> --search-type (noop|mem|sserialize) -q <query> --tempdir <dir> -t <threadCount> -st <search creation thread count>" << std::endl;
}

int main(int argc, char const * argv[] ) {
    Config cfg;

    for(int i(1); i < argc; ++i) {
        std::string token(argv[i]);
        if (token == "-l" && i+1 < argc ) {
            cfg.levels = std::atoi(argv[i+1]);
            ++i;
        }
        else if (token == "-f" && i+1 < argc) {
            cfg.filename = std::string(argv[i+1]);
            ++i;
        }
        else if (token == "--search-type" && i+1 < argc) {
			token = std::string(argv[i+1]);
			if (token == "noop") {
				cfg.st = ST_NOOP;
			}
			else if (token == "mem") {
				cfg.st = ST_MEM;
			}
			else if (token == "sserialize") {
				cfg.st = ST_SS;
			}
			else {
				std::cerr << "Search type" << std::endl;
				return -1;
			}
			++i;
		}
		else if (token == "--index-type" && i+1 < argc) {
			token = std::string(argv[i+1]);
			if (token == "htm") {
				cfg.it = IT_HTM;
			}
			else if (token == "h3") {
				cfg.it = IT_H3;
			}
			else if (token == "simplegrid") {
				cfg.it = IT_SIMPLE_GRID;
			}
			else if (token == "s2geom") {
				cfg.it = IT_S2GEOM;
			}
			else {
				std::cerr << "Invalid index type" << std::endl;
				return -1;
			}
			++i;
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
        else if (token == "-st" && i+1 < argc ) {
            cfg.searchCreationThreadCount = std::atoi(argv[i+1]);
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
    
    sserialize::RCPtrWrapper<sserialize::spatial::dgg::interface::SpatialGrid> sg;
	switch(cfg.it) {
		case IT_HTM:
			sg = hic::HtmSpatialGrid::make(cfg.levels);
			break;
		case IT_H3:
			sg = hic::H3SpatialGrid::make(cfg.levels);
			break;
		case IT_SIMPLE_GRID:
			sg = sserialize::spatial::dgg::SimpleGridSpatialGrid::make(cfg.levels);
			break;
		case IT_S2GEOM:
			sg = hic::S2GeomSpatialGrid::make(cfg.levels);
			break;
		default:
			std::cerr << "Invalid spatial index type" << std::endl;
			return -1;
	}

    auto ohi = std::make_shared<hic::OscarSgIndex>(cmp->store(), cmp->indexStore(), sg);
	
	std::cout << "Creating htm index..." << std::endl;
	ohi->create(cfg.threadCount);
	
	ohi->stats();
	
	if (cfg.st != ST_NONE) {
		
		auto oshi = std::make_shared<hic::OscarSearchSgIndex>(cmp, ohi);
		
		oshi->idxFactory().setType(sserialize::ItemIndex::T_RLE_DE);
		oshi->idxFactory().setDeduplication(true);
		oshi->idxFactory().setIndexFile(sserialize::UByteArrayAdapter::createCache(1024, sserialize::MM_SLOW_FILEBASED));
		
		std::cout << "Creating search structures..." << std::endl;
		switch (cfg.st) {
			case ST_MEM:
				oshi->create(cfg.searchCreationThreadCount);
				break;
			case ST_NOOP:
				oshi->create(cfg.searchCreationThreadCount, hic::OscarSearchSgIndex::FT_NO_OP);
				break;
			case ST_SS:
			{
				auto searchData = sserialize::UByteArrayAdapter::createCache(1024, sserialize::MM_SLOW_FILEBASED);
				oshi->create(searchData, cfg.searchCreationThreadCount);
			}
				break;
			default:
				break;
		};
		
		auto oswh = std::make_shared<hic::OscarSearchWithSg>(oshi);
		
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
