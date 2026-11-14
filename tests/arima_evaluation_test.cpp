#include "arima.h"
#include <cassert>
#include <cmath>
#include <iostream>
#include <limits>

void close(double actual, double expected) {
    assert(std::abs(actual - expected) < 1e-8);
}
int main() {
    for (const auto& filename : {"data/processed/brent_daily.csv", "data/raw/usd_pkr_daily.csv"}) {
        std::vector<std::string> dates;
        std::vector<double> prices;
        assert(loadCSV(filename, dates, prices));
        for (int horizon : {12, 24}) {
            const auto result = forecastWithEvaluation(prices, horizon);
            assert(result.ok);
            assert(result.testCount == horizon);
            assert(result.trainCount == static_cast<int>(prices.size()) - horizon);
            close(result.mean, forecastMean(prices, horizon));
            // Independently fit on the prefix and calculate errors in original price units.
            std::vector<double> prefix(prices.begin(), prices.end() - horizon);
            ARIMA evaluation(10, horizon, 7, MIN_TRAIN);
            evaluation.trainModel(prefix);
            const auto predictions = evaluation.forecast(horizon);
            double squared = 0, absolute = 0, percentage = 0;
            for (int i = 0; i < horizon; ++i) {
                const double actual = prices[prefix.size() + i];
                const double error = predictions[i] - actual;
                squared += error * error;
                absolute += std::abs(error);
                percentage += std::abs(error) / actual;
            }
            close(result.rmse, std::sqrt(squared / horizon));
            close(result.mae, absolute / horizon);
            close(result.mape, percentage * 100 / horizon);
            assert(result.evaluationP == evaluation.getBestP());
            // Changing only the holdout must not change the selected evaluation order.
            auto changed = prices;
            for (size_t i = prefix.size(); i < changed.size(); ++i) changed[i] *= 2;
            assert(forecastWithEvaluation(changed, horizon).evaluationP == result.evaluationP);
            std::cout << filename << " horizon=" << horizon << " MAPE=" << result.mape
                      << " RMSE=" << result.rmse << " MAE=" << result.mae << '\n';
        }
    }
    std::vector<double> constant(320, 100);
    auto result = forecastWithEvaluation(constant, 12);
    assert(result.ok);
    close(result.mean, 100); close(result.next, 100);
    close(result.rmse, 0); close(result.mae, 0); close(result.mape, 0);
    assert(!forecastWithEvaluation(std::vector<double>(273, 100), 12).ok);
    assert(forecastWithEvaluation(std::vector<double>(274, 100), 12).ok);
    assert(!forecastWithEvaluation(constant, 0).ok);
    assert(!forecastWithEvaluation(constant, 366).ok);
    for (double invalid : {0.0, -1.0, std::numeric_limits<double>::infinity(), std::numeric_limits<double>::quiet_NaN()}) {
        constant[0] = invalid;
        assert(!forecastWithEvaluation(constant, 12).ok);
    }
    std::cout << "ARIMA evaluation checks passed\n";
}
