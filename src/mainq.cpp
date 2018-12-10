#include <liboscar/StaticOsmCompleter.h>
#include "static-htm-index.h"

struct Config {
    std::string oscarFiles;
    std::string htmFiles;
};

struct WorkData {
    WorkData() {}
    virtual ~WorkData() {}
    template<typename T>
    T const * as() const { return dynamic_cast<T const *>(this); }

    template<typename T>
    T * as() { return dynamic_cast<T&>(*this); }
};

struct WorkDataString: public WorkData {
    WorkDataString(std::string const & str) : str(str) {}
    virtual ~WorkDataString() override {}
    std::string str;
};

struct WorkItem {
    enum Type {
        WI_STRING,
        WI_HTM_QUERY,
        WI_OSCAR_QUERY
    };

    WorkItem(Type t, WorkData * d) : type(t), data(d) {}

    std::unique_ptr<WorkData> data;
    Type type;
};

struct State {
    std::vector<WorkItem> queue;
    std::string str;
};

struct HtmState {
    sserialize::UByteArrayAdapter indexData;
    sserialize::UByteArrayAdapter searchData;
    sserialize::Static::ItemIndexStore idxStore;
};

void help() {
	std::cerr << "prg -o <oscar files> -f <htm files> -m <query string> -hq -oq" << std::endl;
}

int main(int argc, char const * argv[] ) {
    Config cfg;
    State state;
    HtmState htmState;

    for(int i(1); i < argc; ++i) {
        std::string token(argv[i]);
        if (token == "-o" && i+1 < argc ) {
            cfg.oscarFiles = std::atoi(argv[i+1]);
            ++i;
        }
        else if (token == "-f" && i+1 < argc) {
            cfg.htmFiles = std::string(argv[i+1]);
            ++i;
        }
		else if (token == "-m" && i+1 < argc) {
			state.queue.emplace_back(WorkItem::WI_STRING, new WorkDataString(std::string(argv[i+1])));
			++i;
		}
        else if (token == "-hq") {
            state.queue.emplace_back(WorkItem::WI_HTM_QUERY, std::nullptr_t());
        }
        else if (token == "-oq") {
            state.queue.emplace_back(WorkItem::WI_OSCAR_QUERY, std::nullptr_t());
        }
		else if (token == "--tempdir" && i+1 < argc) {
			token = std::string(argv[i+1]);
			sserialize::UByteArrayAdapter::setFastTempFilePrefix(token);
			sserialize::UByteArrayAdapter::setTempFilePrefix(token);
			++i;
		}
        else {
            std::cerr << "Unkown parameter: " << token << std::endl;
			help();
            return -1;
        }
    }

    auto cmp = std::make_shared<liboscar::Static::OsmCompleter>();
    cmp->setAllFilesFromPrefix(cfg.oscarFiles);

    try {
        cmp->energize();
    }
    catch (std::exception const & e) {
        std::cerr << "Error occured: " << e.what() << std::endl;
		help();
		return -1;
    }

    try {
        htmState.indexData = sserialize::UByteArrayAdapter::openRo(cfg.htmFiles + "/index", false);
        htmState.searchData = sserialize::UByteArrayAdapter::openRo(cfg.htmFiles + "/search", false);
        htmState.idxStore = sserialize::Static::ItemIndexStore(htmState.indexData);
    }
    catch (std::exception const & e) {
        std::cerr << "Could not initialize htm index data" << std::endl;
        return -1;
    } 

    auto htmCmp = hic::Static::OscarSearchHtmIndex::make(htmState.searchData, htmState.idxStore);

    

    return 0;
}
