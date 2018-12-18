#include <liboscar/StaticOsmCompleter.h>
#include <sserialize/stats/TimeMeasuerer.h>
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
    T * as() { return dynamic_cast<T*>(this); }
};

template<typename T>
struct WorkDataSingleValue: public WorkData {
    WorkDataSingleValue(T const & value) : value(value) {}
    virtual ~WorkDataSingleValue() override {}
    T value;
};



using WorkDataString = WorkDataSingleValue<std::string>;
using WorkDataU32 = WorkDataSingleValue<uint32_t>;

struct WorkItem {
    enum Type {
        WI_QUERY_STRING,
		WI_NUM_THREADS,
        WI_HTM_QUERY,
        WI_OSCAR_QUERY
    };

    WorkItem(Type t, WorkData * d) : data(d), type(t) {}

    std::unique_ptr<WorkData> data;
    Type type;
};

struct State {
    std::vector<WorkItem> queue;
    std::string str;
	uint32_t numThreads{1};
};

struct HtmState {
    sserialize::UByteArrayAdapter indexData;
    sserialize::UByteArrayAdapter searchData;
    sserialize::Static::ItemIndexStore idxStore;
};

struct QueryStats {
	sserialize::CellQueryResult cqr;
	sserialize::ItemIndex items;
	sserialize::TimeMeasurer cqrTime;
	sserialize::TimeMeasurer flatenTime;
};

std::ostream & operator<<(std::ostream & out, QueryStats const & ts) {
	out << "# cells: " << ts.cqr.cellCount() << '\n';
	out << "# items: " << ts.items.size() << '\n';
	out << "Cell time: " << ts.cqrTime << '\n';
	out << "Flaten time: " << ts.flatenTime << '\n';
	return out;
}

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
            cfg.oscarFiles = std::string(argv[i+1]);
            ++i;
        }
        else if (token == "-f" && i+1 < argc) {
            cfg.htmFiles = std::string(argv[i+1]);
            ++i;
        }
		else if (token == "-m" && i+1 < argc) {
			state.queue.emplace_back(WorkItem::WI_QUERY_STRING, new WorkDataString(std::string(argv[i+1])));
			++i;
		}
		else if (token == "-t" && i+1 < argc) {
			state.queue.emplace_back(WorkItem::WI_NUM_THREADS, new WorkDataU32(std::atoi(argv[i+1])));
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
	auto hcmp = std::make_shared<hic::Static::OscarSearchHtmCompleter>();
	
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
		hcmp->energize(cfg.htmFiles);
	}
    catch (std::exception const & e) {
        std::cerr << "Error occured: " << e.what() << std::endl;
		help();
		return -1;
    }

	QueryStats oqs, hqs;
	for(uint32_t i(0), s(state.queue.size()); i < s; ++i) {
		WorkItem & wi = state.queue[i];
		
		switch (wi.type) {
			case WorkItem::WI_QUERY_STRING:
				state.str = wi.data->as<WorkDataString>()->value;
				break;
			case WorkItem::WI_NUM_THREADS:
				state.numThreads = wi.data->as<WorkDataU32>()->value;
				break;
			case WorkItem::WI_HTM_QUERY:
			{
				hqs.cqrTime.begin();
				hqs.cqr = hcmp->complete(state.str);
				hqs.cqrTime.end();
				hqs.flatenTime.begin();
				hqs.items = hqs.cqr.flaten(state.numThreads);
				hqs.flatenTime.end();
				std::cout << "HtmIndex query: " << state.str << std::endl;
				std::cout << hqs << std::endl;
			}
				break;
			case WorkItem::WI_OSCAR_QUERY:
			{
				oqs.cqrTime.begin();
				oqs.cqr = cmp->cqrComplete(state.str);
				oqs.cqrTime.end();
				oqs.flatenTime.begin();
				oqs.items = oqs.cqr.flaten(state.numThreads);
				oqs.flatenTime.end();
				std::cout << "Oscar query: " << state.str << std::endl;
				std::cout << oqs << std::endl;
			}
				break;
			default:
				break;
		}
	}

    return 0;
}
