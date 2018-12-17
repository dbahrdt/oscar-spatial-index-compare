#include <liboscar/StaticOsmCompleter.h>
#include "htm-index.h"

struct Config {
    int htmLevel{8};
	uint32_t threadCount{0};
    std::string filename;
    std::string outdir;
};

struct State {
    sserialize::UByteArrayAdapter indexFile;
    sserialize::UByteArrayAdapter searchFile;
};

void help() {
	std::cerr << "prg -f <oscar files> -l <htm levels>  -o <outdir> --tempdir <dir> -t <threadCount>" << std::endl;
}

int main(int argc, char const * argv[] ) {
    Config cfg;
    State state;

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

    if (!sserialize::MmappedFile::isDirectory(cfg.outdir) && !sserialize::MmappedFile::createDirectory(cfg.outdir)) {
        std::cerr << "Could not create output directory" << std::endl;
        help();
        return -1;
    }

    state.indexFile = sserialize::UByteArrayAdapter::createFile(0, cfg.outdir + "/index");
    state.searchFile = sserialize::UByteArrayAdapter::createFile(0, cfg.outdir + "/search");

    auto ohi = std::make_shared<hic::OscarHtmIndex>(cmp->store(), cmp->indexStore(), cfg.htmLevel);
	
	std::cout << "Creating htm index..." << std::endl;
	ohi->create(cfg.threadCount);
	
	ohi->stats();
	
    auto oshi = std::make_shared<hic::OscarSearchHtmIndex>(cmp, ohi);
		
    oshi->idxFactory().setType(cmp->indexStore().indexTypes());
    oshi->idxFactory().setDeduplication(true);
    oshi->idxFactory().setIndexFile(state.indexFile);
    
    std::cout << "Serializing search structures..." << std::endl;
    oshi->create(state.searchFile, cfg.threadCount);
    oshi->idxFactory().flush();
    std::cout << "done" << std::endl;

    state.indexFile.sync();
    state.searchFile.sync();

    return 0;
}
