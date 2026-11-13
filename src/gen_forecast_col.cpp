#include "arima.h"

#include <algorithm>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

int main()
{
    std::ifstream brent_file("data/processed/brent_daily.csv");
    if (!brent_file)
    {
        std::cerr << "Could not open data/processed/brent_daily.csv\n";
        return 1;
    }

    std::vector<std::pair<std::string, double>> brent_rows;
    std::string line;
    std::getline(brent_file, line);
    while (std::getline(brent_file, line))
    {
        std::istringstream row(line);
        std::string date;
        std::string price_text;
        if (!std::getline(row, date, ',') ||
            !std::getline(row, price_text, ','))
            continue;

        std::istringstream price_stream(price_text);
        double price;
        if (!(price_stream >> price) || price <= 0)
            continue;

        brent_rows.emplace_back(date, price);
    }
    if (brent_file.bad())
    {
        std::cerr << "Error reading data/processed/brent_daily.csv\n";
        return 1;
    }
    std::sort(brent_rows.begin(), brent_rows.end());

    std::ifstream levy_file("data/processed/levy_training.csv");
    if (!levy_file)
    {
        std::cerr << "Could not open data/processed/levy_training.csv\n";
        return 1;
    }

    std::vector<std::string> period_starts;
    std::getline(levy_file, line);
    while (std::getline(levy_file, line))
    {
        if (line.empty() || line == "\r")
            continue;

        std::istringstream row(line);
        std::string period_start;
        std::getline(row, period_start, ',');
        if (!period_start.empty() && period_start.back() == '\r')
            period_start.pop_back();
        period_starts.push_back(period_start);
    }
    if (levy_file.bad())
    {
        std::cerr << "Error reading data/processed/levy_training.csv\n";
        return 1;
    }

    std::ofstream output("data/processed/forecast_col.csv");
    if (!output)
    {
        std::cerr << "Could not open data/processed/forecast_col.csv\n";
        return 1;
    }
    output << "period_start,forecast_crude_price\n";
    output << std::fixed << std::setprecision(2);

    for (std::size_t i = 0; i < period_starts.size(); ++i)
    {
        const std::string& cutoff = period_starts[i];
        std::vector<double> slice;
        for (const auto& brent_row : brent_rows)
        {
            if (brent_row.first >= cutoff)
                break;
            slice.push_back(brent_row.second);
        }

        if (slice.size() < static_cast<size_t>(MIN_TRAIN))
        {
            std::cerr << "Row " << i << " (" << cutoff << "): only "
                      << slice.size() << " rows of history, skipping\n";
            output << cutoff << ",\n";
            continue;
        }

        const double result = forecastMean(slice, 10);
        const double baseline_rmse = ARIMA().baselineRMSE(slice);
        std::cerr << "Row " << i << " (" << cutoff << "): forecast="
                  << std::fixed << std::setprecision(2) << result
                  << ", baseline_rmse=" << baseline_rmse << '\n';
        output << cutoff << ',' << result << '\n';
    }

    output.close();
    if (!output)
    {
        std::cerr << "Error writing data/processed/forecast_col.csv\n";
        return 1;
    }
    return 0;
}
