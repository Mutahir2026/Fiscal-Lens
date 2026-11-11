#include <emscripten/bind.h>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include <functional>
#include "arima.h"
#include "LevyAdvisor.h"

// These are the original main() functions, renamed by -Dmain=... at compile time
int tree_main(int argc, char* argv[]);
int gen_forecast_main();

struct RunResult {
    int code;
    std::string output;
};

// Runs fn while capturing everything written to cout and cerr
static RunResult capture(const std::function<int()>& fn) {
    std::ostringstream buf;
    std::streambuf* oldOut = std::cout.rdbuf(buf.rdbuf());
    std::streambuf* oldErr = std::cerr.rdbuf(buf.rdbuf());
    int code = fn();
    std::cout.rdbuf(oldOut);
    std::cerr.rdbuf(oldErr);
    return RunResult{code, buf.str()};
}

RunResult wasmLive(double crude) {
    return capture([&] { return runLiveMode(crude); });
}

RunResult wasmBacktest(const std::string& date) {
    return capture([&] { return runBacktestMode(date); });
}

// Full demo report (what ./tree_test prints with no arguments)
RunResult wasmReport() {
    return capture([] {
        char name[] = "tree_test";
        char* argv[] = {name, nullptr};
        return tree_main(1, argv);
    });
}

// Writes /data/processed/forecast_col.csv inside the in-memory filesystem
RunResult wasmGenForecastCol() {
    return capture([] { return gen_forecast_main(); });
}

// ARIMA forecast: JS passes an array of prices
double wasmForecastMean(const std::vector<double>& prices, int nDays) {
    return forecastMean(prices, nDays);
}

EMSCRIPTEN_BINDINGS(levy_module) {
    emscripten::value_object<RunResult>("RunResult")
        .field("code", &RunResult::code)
        .field("output", &RunResult::output);

    emscripten::register_vector<double>("VectorDouble");

    emscripten::function("runLive", &wasmLive);
    emscripten::function("runBacktest", &wasmBacktest);
    emscripten::function("runReport", &wasmReport);
    emscripten::function("generateForecastCol", &wasmGenForecastCol);
    emscripten::function("forecastMean", &wasmForecastMean);
}