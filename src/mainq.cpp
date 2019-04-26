#include <ostream>
#include <chrono>

#include <liboscar/StaticOsmCompleter.h>
#include <sserialize/stats/TimeMeasuerer.h>
#include <sserialize/stats/statfuncs.h>
#include "static-htm-index.h"
#include "HCQRCompleter.h"
#include "HCQRIndexMakeStatic.h"
#include "GeoHierarchyHCQRCompleter.h"
#include "HCQRIndexCompactifying.h"

struct Config {
    std::string oscarFiles;
    std::string htmFiles;
	bool staticHCQR{false};
	bool compactifiedHCQR{false};
	uint32_t cachedHCQR{0};
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


struct WorkDataBenchmark: public WorkData {
	WorkDataBenchmark(std::string const & queryFileName, std::string const & rawStatsPrefix, bool treedCQR, bool hcqr, uint32_t threadCount) :
	queryFileName(queryFileName),
	rawStatsPrefix(rawStatsPrefix),
	treedCQR(treedCQR),
	hcqr(hcqr),
	threadCount(threadCount)
	{}
	virtual ~WorkDataBenchmark() {}
	std::string queryFileName;
	std::string rawStatsPrefix;
	bool treedCQR;
	bool hcqr;
	uint32_t threadCount;
};

using WorkDataString = WorkDataSingleValue<std::string>;
using WorkDataU32 = WorkDataSingleValue<uint32_t>;

struct WorkItem {
    enum Type {
        WI_QUERY_STRING,
		WI_BENCHMARK,
		WI_NUM_THREADS,
		WI_NUM_ITEMS,
        WI_SG_CQR,
        WI_SG_TCQR,
		WI_SG_HCQR,
        WI_OSCAR_CQR,
        WI_OSCAR_TCQR,
		WI_OSCAR_HCQR,
        WI_PRELOAD,
		WI_STATS
    };

    WorkItem(Type t, WorkData * d) : data(d), type(t) {}

    std::unique_ptr<WorkData> data;
    Type type;
};

struct State {
    std::vector<WorkItem> queue;
    std::string str;
	int numItems{0};
	uint32_t numThreads{1};
};

struct HtmState {
    sserialize::UByteArrayAdapter indexData;
    sserialize::UByteArrayAdapter searchData;
    sserialize::Static::ItemIndexStore idxStore;
};

struct Completers {
    std::shared_ptr<liboscar::Static::OsmCompleter> cmp;
	std::shared_ptr<hic::Static::OscarSearchSgCompleter> sgcmp;
	std::shared_ptr<hic::HCQRCompleter> hsgcmp;
	std::shared_ptr<hic::HCQRCompleter> hocmp;
};

struct QueryStats {
	sserialize::CellQueryResult cqr;
	sserialize::ItemIndex items;
	sserialize::TimeMeasurer cqrTime;
	sserialize::TimeMeasurer flatenTime;
};

struct HQueryStats {
	sserialize::RCPtrWrapper<hic::interface::HCQR> hcqr;
	sserialize::ItemIndex items;
	sserialize::TimeMeasurer cqrTime;
	sserialize::TimeMeasurer flatenTime;
};

template<typename T_OUTPUT_ITERATOR>
void readCompletionStringsFromFile(const std::string & fileName, T_OUTPUT_ITERATOR out) {
	std::string tmp;
	std::ifstream inFile;
	inFile.open(fileName);
	if (!inFile.is_open()) {
		throw std::runtime_error("Failed to read completion strings from " + fileName);
	}
	while (!inFile.eof()) {
		std::getline(inFile, tmp);
		*out = tmp;
		++out;
	}
	inFile.close();
}

std::ostream & operator<<(std::ostream & out, QueryStats const & ts) {
	out << "# cells: " << ts.cqr.cellCount() << '\n';
	out << "Set op time: " << ts.cqrTime << '\n';
	out << "# items: " << ts.items.size() << '\n';
	out << "Flaten time: " << ts.flatenTime << '\n';
	return out;
}

std::ostream & operator<<(std::ostream & out, HQueryStats const & qs) {
	out << "depth: " << qs.hcqr->depth() << '\n';
	out << "# nodes: " << qs.hcqr->numberOfNodes() << '\n';
	out << "Set op time: " << qs.cqrTime << '\n';
	out << "Flaten time: " << qs.flatenTime << '\n';
	out << "# items: " << qs.items.size() << '\n';
	return out;
}

std::string const meas_res_unit{"us"};

struct Stats {
	using meas_res = std::chrono::microseconds;
	std::vector<double> cqr;
	std::vector<double> flaten;
	std::vector<uint32_t> cellCount;
	std::vector<uint32_t> itemCount;
	void reserve(std::size_t size) {
		cqr.reserve(size);
		flaten.reserve(size);
		cellCount.reserve(size);
		itemCount.reserve(size);
	}
};

void benchmark(Completers & completers, WorkDataBenchmark const & cfg) {
	using namespace sserialize::statistics;
	
	std::cout << "Benchmarking\n";
	std::cout << "threads: " << cfg.threadCount << '\n';
	std::cout << "treedCQR: " << (cfg.treedCQR ? "yes" : "no") << std::endl;
	std::cout << "hcqr: " << (cfg.hcqr ? "yes" : "no") << std::endl;
	
	std::string header;
	if (cfg.hcqr) {
		header = "Query id; cqr time [us];flaten time[us];node count; item count";
	}
	else {
		header = "Query id; cqr time [us];flaten time[us];cell count; item count";
	}
	
	sserialize::ProgressInfo pinfo;
	
	std::vector<std::string> queries;
	readCompletionStringsFromFile(cfg.queryFileName, std::back_inserter(queries));
	
	if (completers.sgcmp) {
		Stats sg_stats;
		auto sg_rsf = std::ofstream(cfg.rawStatsPrefix + "." + completers.sgcmp->index().sg().name() + ".stats.raw");

		sg_rsf << header << std::endl;
		sg_stats.reserve(queries.size());
		

		pinfo.begin(queries.size(), "Computing sg queries");
		if (cfg.hcqr) {
			for(std::size_t i(0), s(queries.size()); i < s; ++i) {
				{ //no static hcqr support yet. Instead this will use the cached files. 
					auto dummy = completers.hsgcmp->complete(queries[i]);
				}
				
				auto start = std::chrono::high_resolution_clock::now();
				auto sg_hcqr = completers.hsgcmp->complete(queries[i]);
				auto stop = std::chrono::high_resolution_clock::now();
				sg_stats.cqr.emplace_back(std::chrono::duration_cast<Stats::meas_res>(stop-start).count());
				
				start = std::chrono::high_resolution_clock::now();
				auto sg_items = sg_hcqr->items();
				stop = std::chrono::high_resolution_clock::now();
				sg_stats.flaten.emplace_back(std::chrono::duration_cast<Stats::meas_res>(stop-start).count());
				sg_stats.cellCount.emplace_back(sg_hcqr->numberOfNodes());
				sg_stats.itemCount.emplace_back(sg_items.size());
				
				SSERIALIZE_EXPENSIVE_ASSERT(sg_items == completers.cmp->cqrComplete(queries[i]).flaten());
			}
		}
		else {
			for(std::size_t i(0), s(queries.size()); i < s; ++i) {
				auto start = std::chrono::high_resolution_clock::now();
				auto sg_cqr = completers.sgcmp->complete(queries[i], cfg.treedCQR, cfg.threadCount);
				auto stop = std::chrono::high_resolution_clock::now();
				sg_stats.cqr.emplace_back(std::chrono::duration_cast<Stats::meas_res>(stop-start).count());
				
				start = std::chrono::high_resolution_clock::now();
				auto sg_items = sg_cqr.flaten(cfg.threadCount);
				stop = std::chrono::high_resolution_clock::now();
				sg_stats.flaten.emplace_back(std::chrono::duration_cast<Stats::meas_res>(stop-start).count());
				sg_stats.cellCount.emplace_back(sg_cqr.cellCount());
				sg_stats.itemCount.emplace_back(sg_items.size());
				
				SSERIALIZE_EXPENSIVE_ASSERT(sg_items == completers.cmp->cqrComplete(queries[i]).flaten());
			}
		}
		pinfo.end();
		
		for(std::size_t i(0), s(queries.size()); i < s; ++i) {
			sg_rsf << i << ';'
					<< sg_stats.cqr[i] << ';'
					<< sg_stats.flaten[i] << ';'
					<< sg_stats.cellCount[i] << ';'
					<< sg_stats.itemCount[i] << '\n';
		}
		sg_rsf << std::flush;
		sg_rsf.close();
		
		
		std::cout << "SpatialIndex::cqr:" << std::endl;
		StatPrinting::print(std::cout, sg_stats.cqr.begin(), sg_stats.cqr.end());
		std::cout << "SpatialIndex::flaten:" << std::endl;
		StatPrinting::print(std::cout, sg_stats.flaten.begin(), sg_stats.flaten.end());
		std::cout << "SpatialIndex::cellCount:" << std::endl;
		StatPrinting::print(std::cout, sg_stats.cellCount.begin(), sg_stats.cellCount.end());
			
	}
	
	if (completers.cmp) {
		Stats o_stats;
		auto o_rsf = std::ofstream(cfg.rawStatsPrefix + ".oscar.stats.raw");
		
		o_rsf << header << std::endl;
		o_stats.reserve(queries.size());

		pinfo.begin(queries.size(), "Computing oscar queries");
		if (cfg.hcqr) {
			for(std::size_t i(0), s(queries.size()); i < s; ++i) {
				{
					auto dummy = completers.hocmp->complete(queries[i]);
				}
				auto start = std::chrono::high_resolution_clock::now();
				auto o_hcqr = completers.hocmp->complete(queries[i]);
				auto stop = std::chrono::high_resolution_clock::now();
				o_stats.cqr.emplace_back(std::chrono::duration_cast<Stats::meas_res>(stop-start).count());
				
				start = std::chrono::high_resolution_clock::now();
				auto o_items = o_hcqr->items();
				stop = std::chrono::high_resolution_clock::now();
				o_stats.flaten.emplace_back(std::chrono::duration_cast<Stats::meas_res>(stop-start).count());
				o_stats.cellCount.emplace_back(o_hcqr->numberOfNodes());
				o_stats.itemCount.emplace_back(o_items.size());
			}
		}
		else {
			for(std::size_t i(0), s(queries.size()); i < s; ++i) {
				auto start = std::chrono::high_resolution_clock::now();
				auto o_cqr = completers.cmp->cqrComplete(queries[i], cfg.treedCQR, cfg.threadCount);
				auto stop = std::chrono::high_resolution_clock::now();
				o_stats.cqr.emplace_back(std::chrono::duration_cast<Stats::meas_res>(stop-start).count());
				
				start = std::chrono::high_resolution_clock::now();
				auto o_items = o_cqr.flaten(cfg.threadCount);
				stop = std::chrono::high_resolution_clock::now();
				o_stats.flaten.emplace_back(std::chrono::duration_cast<Stats::meas_res>(stop-start).count());
				o_stats.cellCount.emplace_back(o_cqr.cellCount());
				o_stats.itemCount.emplace_back(o_items.size());
			}
		}
		pinfo.end();
	
		for(std::size_t i(0), s(queries.size()); i < s; ++i) {
			o_rsf << i << ';'
					<< o_stats.cqr[i] << ';'
					<< o_stats.flaten[i] << ';'
					<< o_stats.cellCount[i] << ';'
					<< o_stats.itemCount[i] << '\n';
		}
		o_rsf << std::flush;
		o_rsf.close();
	
		std::cout << "Oscar::cqr:" << std::endl;
		StatPrinting::print(std::cout, o_stats.cqr.begin(), o_stats.cqr.end());
		std::cout << "Oscar::flaten:" << std::endl;
		StatPrinting::print(std::cout, o_stats.flaten.begin(), o_stats.flaten.end());
		std::cout << "Oscar::cellCount:" << std::endl;
		StatPrinting::print(std::cout, o_stats.cellCount.begin(), o_stats.cellCount.end());
	}
}

void printStats(Completers & completers) {
	if (!completers.sgcmp) {
		std::cerr << "printStats: No spatial grid info" << std::endl;
		return;
	}
	auto const & sgInfo = completers.sgcmp->index().sgInfo();
	auto const & sg = completers.sgcmp->index().sg();
	std::cout << "levels: " << sgInfo.levels() << std::endl;
	std::cout << "cell count: " << sgInfo.cPixelCount() << std::endl;
	sserialize::MinMaxMean<double> area;
	
	for(uint32_t cellId(0), s(sgInfo.cPixelCount()); cellId < s; ++cellId) {
		auto sgPixelId = sgInfo.sgIndex(cellId);
		area.update( sg.area(sgPixelId) );
	}
	
	std::cout << "min area: " << area.min() << std::endl;
	std::cout << "mean area: " << area.mean() << std::endl;
	std::cout << "max area: " << area.max() << std::endl;
}

void help() {
	std::cerr << "prg -o <oscar files> -f <spatial grid files> --hcqr-cache <number> --static-hcqr --compact-hcqr  -m <query string> -t <number of threads> -sq -tsq -hsq -oq -toq -hoq --preload --benchmark <query file> <raw stats prefix> <treedCQR=true|false> <hcqr=true|false> <threadCount> --stats" << std::endl;
}

sserialize::RCPtrWrapper<hic::interface::HCQRIndex> applyCfg(sserialize::RCPtrWrapper<hic::interface::HCQRIndex> index, Config const & cfg) {
	if (cfg.compactifiedHCQR) {
		index = hic::HCQRIndexCompactifying::make(index);
	}
	if (cfg.staticHCQR) {
		index = hic::HCQRIndexMakeStatic::make(index);
	}
	if (cfg.cachedHCQR) {
		index = hic::HCQRIndexWithCache::make(index, cfg.cachedHCQR);
	}
	return index;
}

int main(int argc, char const * argv[]) {
    Config cfg;
    State state;
    HtmState htmState;
	Completers completers;

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
        else if (token == "--hcqr-cache" && i+1 < argc) {
			cfg.cachedHCQR = std::atoi(argv[i+1]);
			++i;
		}
		else if (token == "--compact-hcqr") {
			cfg.compactifiedHCQR = true;
		}
		else if (token == "--static-hcqr") {
			cfg.staticHCQR = true;
		}
		else if (token == "-m" && i+1 < argc) {
			state.queue.emplace_back(WorkItem::WI_QUERY_STRING, new WorkDataString(std::string(argv[i+1])));
			++i;
		}
		else if (token == "-t" && i+1 < argc) {
			state.queue.emplace_back(WorkItem::WI_NUM_THREADS, new WorkDataU32(std::atoi(argv[i+1])));
			++i;
		}
		else if (token == "-v" && i+1 < argc) {
			state.queue.emplace_back(WorkItem::WI_NUM_ITEMS, new WorkDataU32(std::atoi(argv[i+1])));
			++i;
		}
        else if (token == "-sq") {
            state.queue.emplace_back(WorkItem::WI_SG_CQR, std::nullptr_t());
        }
        else if (token == "-tsq") {
            state.queue.emplace_back(WorkItem::WI_SG_TCQR, std::nullptr_t());
        }
        else if (token == "-hsq") {
            state.queue.emplace_back(WorkItem::WI_SG_HCQR, std::nullptr_t());
        }
        else if (token == "-oq") {
            state.queue.emplace_back(WorkItem::WI_OSCAR_CQR, std::nullptr_t());
        }
        else if (token == "-toq") {
            state.queue.emplace_back(WorkItem::WI_OSCAR_TCQR, std::nullptr_t());
        }
        else if (token == "-hoq") {
            state.queue.emplace_back(WorkItem::WI_OSCAR_HCQR, std::nullptr_t());
        }
		else if (token == "--tempdir" && i+1 < argc) {
			token = std::string(argv[i+1]);
			sserialize::UByteArrayAdapter::setFastTempFilePrefix(token);
			sserialize::UByteArrayAdapter::setTempFilePrefix(token);
			++i;
		}
		else if (token == "--preload") {
            state.queue.emplace_back(WorkItem::WI_PRELOAD, std::nullptr_t());
		}
		else if (token == "--benchmark" && i+4 < argc) {
			state.queue.emplace_back(
				WorkItem::WI_BENCHMARK,
				new WorkDataBenchmark(
					std::string(argv[i+1]),
					std::string(argv[i+2]),
					sserialize::toBool(std::string(argv[i+3])),
					sserialize::toBool(std::string(argv[i+4])),
					std::atoi(argv[i+5])
				)
			);
			i += 5;
		}
		else if (token == "--stats") {
			state.queue.emplace_back(WorkItem::WI_STATS, std::nullptr_t());
		}
        else {
            std::cerr << "Unkown parameter: " << token << std::endl;
			help();
            return -1;
        }
    }

	
	if (cfg.oscarFiles.size()) {
		completers.cmp = std::make_shared<liboscar::Static::OsmCompleter>();
		completers.cmp->setAllFilesFromPrefix(cfg.oscarFiles);
		try {
			completers.cmp->energize();
		}
		catch (std::exception const & e) {
			std::cerr << "Error occured while loading oscar files: " << e.what() << std::endl;
			help();
			return -1;
		}
		bool needsHCQR = false;
		for(auto const & x : state.queue) {
			if ((x.type == WorkItem::WI_OSCAR_HCQR) || (x.type == WorkItem::WI_BENCHMARK && x.data->as<WorkDataBenchmark>()->hcqr)) {
				needsHCQR = true;
				break;
			}
		}
		if (needsHCQR) {
			try {
				auto base = hic::makeGeoHierarchyHCQRIndex(*completers.cmp);
				completers.hocmp = std::make_shared<hic::HCQRCompleter>( applyCfg(base, cfg) );
			}
			catch (std::exception const & e) {
				std::cerr << "Error while computing HCQR Index for OSCAR search files: " << e.what() << std::endl;
				return -1;
			}
		}
	}
	if (cfg.htmFiles.size()) {
		completers.sgcmp = std::make_shared<hic::Static::OscarSearchSgCompleter>();
		try {
			completers.sgcmp->energize(cfg.htmFiles);
		}
		catch (std::exception const & e) {
			std::cerr << "Error occured while loading spatial grid files: " << e.what() << std::endl;
			help();
			return -1;
		}
		bool needsHCQR = false;
		for(auto const & x : state.queue) {
			if ((x.type == WorkItem::WI_SG_HCQR) || (x.type == WorkItem::WI_BENCHMARK && x.data->as<WorkDataBenchmark>()->hcqr)) {
				needsHCQR = true;
				break;
			}
		}
		if (needsHCQR) {
			try {
				auto base = hic::Static::makeOscarSearchSgHCQRIndex(completers.sgcmp->indexPtr());
				completers.hsgcmp = std::make_shared<hic::HCQRCompleter>( applyCfg(base, cfg) );
			}
			catch (std::exception const & e) {
				std::cerr << "Failed to initialize hierachical spatial grid completer: " << e.what() << std::endl;
				return -1;
			}
		}
	}

	QueryStats oqs, hqs;
	HQueryStats hsg_qs, ho_qs;
	for(uint32_t i(0), s(state.queue.size()); i < s; ++i) {
		WorkItem & wi = state.queue[i];
		
		switch (wi.type) {
			case WorkItem::WI_QUERY_STRING:
				state.str = wi.data->as<WorkDataString>()->value;
				break;
			case WorkItem::WI_NUM_THREADS:
				state.numThreads = wi.data->as<WorkDataU32>()->value;
				break;
			case WorkItem::WI_NUM_ITEMS:
				state.numItems = wi.data->as<WorkDataU32>()->value;
				break;
			case WorkItem::WI_SG_CQR:
			case WorkItem::WI_SG_TCQR:
			{
				if (cfg.htmFiles.empty()) {
					std::cerr << "No spatial grid available" << std::endl;
					return -1;
				}
				hqs.cqrTime.begin();
				hqs.cqr = completers.sgcmp->complete(state.str, wi.type == WorkItem::WI_SG_TCQR, state.numThreads);
				hqs.cqrTime.end();
				if (state.numItems) {
					hqs.flatenTime.begin();
					hqs.items = hqs.cqr.flaten(state.numThreads);
					hqs.flatenTime.end();
				}
				std::cout << "Spatial Grid Index query: " << state.str << std::endl;
				std::cout << hqs << std::endl;
			}
				break;
			case WorkItem::WI_SG_HCQR:
			{
				if (cfg.htmFiles.empty()) {
					std::cerr << "No spatial grid available" << std::endl;
					return -1;
				}
				hsg_qs.cqrTime.begin();
				hsg_qs.hcqr = completers.hsgcmp->complete(state.str);
				hsg_qs.cqrTime.end();
				if (state.numItems) {
					hsg_qs.flatenTime.begin();
					hsg_qs.items = hsg_qs.hcqr->items();
					hsg_qs.flatenTime.end();
				}
				std::cout << "Hierarchical Spatial Grid Index query: " << state.str << std::endl;
				std::cout << hsg_qs << std::endl;
				
			}
				break;
			case WorkItem::WI_OSCAR_CQR:
			case WorkItem::WI_OSCAR_TCQR:
			{
				if (cfg.oscarFiles.empty()) {
					std::cerr << "No oscar completion available" << std::endl;
					return -1;
				}
				oqs.cqrTime.begin();
				oqs.cqr = completers.cmp->cqrComplete(state.str, wi.type == WorkItem::WI_OSCAR_TCQR, state.numThreads);
				oqs.cqrTime.end();
				if (state.numItems) {
					oqs.flatenTime.begin();
					oqs.items = oqs.cqr.flaten(state.numThreads);
					oqs.flatenTime.end();
				}
				std::cout << "Oscar query: " << state.str << std::endl;
				std::cout << oqs << std::endl;
			}
				break;
			case WorkItem::WI_OSCAR_HCQR:
			{
				if (cfg.oscarFiles.empty()) {
					std::cerr << "No spatial grid available" << std::endl;
					return -1;
				}
				ho_qs.cqrTime.begin();
				ho_qs.hcqr = completers.hocmp->complete(state.str);
				ho_qs.cqrTime.end();
				if (state.numItems) {
					ho_qs.flatenTime.begin();
					ho_qs.items = ho_qs.hcqr->items();
					ho_qs.flatenTime.end();
				}
				std::cout << "Hierarchical Oscar query: " << state.str << std::endl;
				std::cout << ho_qs << std::endl;
			}
				break;
			case WorkItem::WI_BENCHMARK:
				benchmark(completers, *(wi.data->as<WorkDataBenchmark>()));
				break;
			case WorkItem::WI_PRELOAD:
			{
				std::cout << "Preloading all data..." << std::flush;
				if (cfg.oscarFiles.size()) {
					for(int fc(liboscar::FC_BEGIN), fce(liboscar::FC_END); fc < fce; ++fc) {
						auto data = completers.cmp->data(liboscar::FileConfig(fc));
						data.advice(sserialize::UByteArrayAdapter::AT_LOAD, data.size());
					}
				}
				if (cfg.htmFiles.size()) {
					htmState.indexData.advice(sserialize::UByteArrayAdapter::AT_LOAD, htmState.indexData.size());
					htmState.searchData.advice(sserialize::UByteArrayAdapter::AT_LOAD, htmState.searchData.size());
				}
				std::cout << "done" << std::endl;
			}
				break;
			case WorkItem::WI_STATS:
			{
				printStats(completers);
			}
				break;
			default:
				break;
		}
	}

    return 0;
}
