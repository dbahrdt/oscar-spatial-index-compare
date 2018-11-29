#include <liboscar/StaticOsmCompleter.h>
#include "htm-index.h"

struct Config {
    int htmLevel{8};
    std::string filename;
};

int main(int argc, char const * argv[] ) {
    Config cfg;

    for(int i(1); i < argc; ++i) {
        std::string token(argv[i]);
        if (token == "--htm-level" && i+1 < argc ) {
            cfg.htmLevel = std::atoi(argv[i+1]);
            ++i;
        }
        else if (token == "-f" && i+1 < argc) {
            cfg.filename = std::string(argv[i+1]);
            ++i;
        }
        else {
            std::cerr << "Unkown parameter: " << token << std::endl;
            return -1;
        }
    }

    liboscar::Static::OsmCompleter cmp;
    cmp.setAllFilesFromPrefix(cfg.filename);

    try {
        cmp.energize();
    }
    catch (std::exception const & e) {
        std::cerr << "Error occured: " << e.what() << std::endl;
    }

    

    return 0;
}
