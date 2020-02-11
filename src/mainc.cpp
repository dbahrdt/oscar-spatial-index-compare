#include <liboscar/StaticOsmCompleter.h>
#include <sserialize/containers/ItemIndexFactory.h>

#include <sserialize/spatial/dgg/SimpleGridSpatialGrid.h>

#include "OscarSearchWithSg.h"
#include "H3SpatialGrid.h"
#include "HtmSpatialGrid.h"
#include "S2GeomSpatialGrid.h"
#include "static-htm-index.h"
#include "StaticHCQRTextIndex.h"


enum IndexType {
	IT_HTM,
	IT_H3,
	IT_S2GEOM,
	IT_SIMPLEGRID
};

enum CreationType {
	CT_SPATIAL_GRID,
	CT_HCQR
};

struct Config {
	CreationType ct;
    int levels{8};
	uint32_t threadCount{0};
	uint32_t serializeThreadCount{0};
	uint32_t compactLevel{std::numeric_limits<uint32_t>::max()};
	bool onlyLeafs{false};
    std::string filename;
    std::string outdir;
	IndexType it{IT_HTM};
};

struct State {
    sserialize::UByteArrayAdapter indexFile;
    sserialize::UByteArrayAdapter searchFile;
};

void help() {
	std::cerr <<
		"prg sg|hcqr\n"
		"all modes\n"
		"\t-o <outdir>\n"
		"\t--tempdir <dir>\n"
		"\t-t <threadCount>\n"
		"\t-st <serialization thread count>\n"
		"sg-mode:\n"
		"\t-f <oscar files>\n"
		"\t--index-type (htm|h3|simplegrid|s2geom)\n"
		"\t-l <levels>\n"
		"hcqr-mode:\n"
		"\t-f <sg files>\n"
		"\t--compactify <max level>"
		"\t--only-leafs"
	<< std::flush;
}

int createSpatialGrid(Config & cfg) {
	
    State state;
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

    if (!sserialize::MmappedFile::isDirectory(cfg.outdir) && !sserialize::MmappedFile::createDirectory(cfg.outdir)) {
        std::cerr << "Could not create output directory" << std::endl;
        help();
        return -1;
    }

    state.indexFile = sserialize::UByteArrayAdapter::createFile(0, cfg.outdir + "/index");
    state.searchFile = sserialize::UByteArrayAdapter::createFile(0, cfg.outdir + "/search");
	
	sserialize::RCPtrWrapper<sserialize::spatial::dgg::interface::SpatialGrid> sg;
	switch(cfg.it) {
		case IT_HTM:
			sg = hic::HtmSpatialGrid::make(cfg.levels);
			break;
		case IT_H3:
			sg = hic::H3SpatialGrid::make(cfg.levels);
			break;
		case IT_S2GEOM:
			sg = hic::S2GeomSpatialGrid::make(cfg.levels);
			break;
		case IT_SIMPLEGRID:
			sg = sserialize::spatial::dgg::SimpleGridSpatialGrid::make(cfg.levels);
			break;
		default:
			std::cerr << "Invalid spatial index type" << std::endl;
			return -1;
	}

    auto ohi = std::make_shared<hic::OscarSgIndex>(cmp->store(), cmp->indexStore(), sg);
	
	std::cout << "Creating htm index..." << std::endl;
	ohi->create(cfg.threadCount);
	
	ohi->stats();
	
    auto oshi = std::make_shared<hic::OscarSearchSgIndex>(cmp, ohi);
		
    oshi->idxFactory().setType(cmp->indexStore().indexTypes());
    oshi->idxFactory().setDeduplication(true);
    oshi->idxFactory().setIndexFile(state.indexFile);
    
    std::cout << "Serializing search structures..." << std::endl;
    oshi->create(state.searchFile, cfg.serializeThreadCount);
    oshi->idxFactory().flush();
    std::cout << "done" << std::endl;

    state.indexFile.sync();
    state.searchFile.sync();
	return 0;
}


int createHCQR(Config & cfg) {
	if (!cfg.filename.size()) {
		std::cerr << "No input files given: " << cfg.filename << std::endl;
		return -1;
	}
	if (!sserialize::MmappedFile::isDirectory(cfg.outdir)) {
		sserialize::MmappedFile::createDirectory(cfg.outdir);
	}
	
	hic::Static::OscarSearchHCQRTextIndexCreator cc;
	cc.threads = cfg.serializeThreadCount;
	cc.compactTree = cfg.onlyLeafs;
	if (cfg.compactLevel != std::numeric_limits<uint32_t>::max()) {
		cc.compactify = true;
		cc.compactLevel = cfg.compactLevel;
	}
	cc.dest = sserialize::UByteArrayAdapter::createFile(0, cfg.outdir + "/search.hcqr");
	cc.src = sserialize::UByteArrayAdapter::openRo(cfg.filename + "/search", false);
	cc.idxStore = sserialize::Static::ItemIndexStore(sserialize::UByteArrayAdapter::openRo(cfg.filename + "/index", false));
	cc.idxFactory.setIndexFile(sserialize::UByteArrayAdapter::createFile(0, cfg.outdir + "/index"));
	cc.idxFactory.setType(cc.idxStore.indexTypes());

	sserialize::TimeMeasurer tm;
	std::cout << "Computing hcqr index" << std::endl;
	tm.begin();
	cc.run();
	tm.end();
	std::cout << "Computing hcqr index took " << tm << std::endl;
	
	cc.idxFactory.flush();
	
	return 0;
}

int main(int argc, char const * argv[] ) {
    Config cfg;
	
	if (argc < 2) {
		help();
		return -1;
	}
	
	if (std::string(argv[1]) == "sg") {
		cfg.ct = CT_SPATIAL_GRID;
	}
	else if (std::string(argv[1]) == "hcqr") {
		cfg.ct = CT_HCQR;
	}
	else {
		help();
		std::cerr << "Unkown creation mode " << argv[1] << std::endl;
		return -1;
	}

    for(int i(2); i < argc; ++i) {
        std::string token(argv[i]);
        if (token == "-l" && i+1 < argc ) {
            cfg.levels = std::atoi(argv[i+1]);
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
			else if (token == "s2geom") {
				cfg.it = IT_S2GEOM;
			}
			else if (token == "simplegrid") {
				cfg.it = IT_SIMPLEGRID;
			}
			else {
				std::cerr << "Invalid index type" << std::endl;
				return -1;
			}
			++i;
		}
        else if (token == "-f" && i+1 < argc) {
            cfg.filename = std::string(argv[i+1]);
            ++i;
        }
		else if (token == "-o" && i+1 < argc) {
			cfg.outdir = std::string(argv[i+1]);
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
            cfg.serializeThreadCount = std::atoi(argv[i+1]);
            ++i;
        }
        else if (token == "--compactify" && i+1 < argc) {
			cfg.compactLevel = std::atoi(argv[i+1]);
			++i;
		}
		else if (token == "--only-leafs") {
			cfg.onlyLeafs = true;
		}
        else {
            std::cerr << "Unkown parameter: " << token << std::endl;
			help();
            return -1;
        }
    }

	if (cfg.ct == CT_SPATIAL_GRID) {
		return createSpatialGrid(cfg);
	}
	else if (cfg.ct == CT_HCQR) {
		return createHCQR(cfg);
	}
}
