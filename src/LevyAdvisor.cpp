#include <cstdio>
#include <cctype>
#include "LevyAdvisor.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <iomanip>
#include <cmath>

namespace {

bool isLeapYear(int y) {
    if (y % 400 == 0) return true;
    if (y % 100 == 0) return false;
    return (y % 4 == 0);
}

int daysInMonth(int y, int m) {
    switch (m) {
        case 1: case 3: case 5: case 7: case 8: case 10: case 12: return 31;
        case 4: case 6: case 9: case 11: return 30;
        case 2: return isLeapYear(y) ? 29 : 28;
        default: return 30;
    }
}

} // anonymous namespace

bool isValidDateFormat(const std::string& dateStr) {
    if (dateStr.size() != 10 || dateStr[4] != '-' || dateStr[7] != '-') return false;
    for (int i = 0; i < 10; i++) {
        if (i == 4 || i == 7) continue;
        if (!isdigit(static_cast<unsigned char>(dateStr[i]))) return false;
    }
    int y = std::stoi(dateStr.substr(0, 4));
    int m = std::stoi(dateStr.substr(5, 2));
    int d = std::stoi(dateStr.substr(8, 2));
    if (y < 1900 || y > 2100 || m < 1 || m > 12) return false;
    if (d < 1 || d > daysInMonth(y, m)) return false;
    return true;
}

std::string nextDay(const std::string& dateStr) {
    if (!isValidDateFormat(dateStr)) return "";
    int y = std::stoi(dateStr.substr(0, 4));
    int m = std::stoi(dateStr.substr(5, 2));
    int d = std::stoi(dateStr.substr(8, 2));
    d++;
    if (d > daysInMonth(y, m)) {
        d = 1;
        m++;
        if (m > 12) {
            m = 1;
            y++;
        }
    }
    char buf[16];
    snprintf(buf, sizeof(buf), "%04d-%02d-%02d", y, m, d);
    return std::string(buf);
}

std::string nextFortnightStart(const std::string& lastStart) {
    if (!isValidDateFormat(lastStart)) return "";
    int y = std::stoi(lastStart.substr(0, 4));
    int m = std::stoi(lastStart.substr(5, 2));
    int d = std::stoi(lastStart.substr(8, 2));
    if (d == 1) d = 16;
    else if (d == 16) { d = 1; if (++m > 12) { m = 1; y++; } }
    else return "";
    char buf[16];
    snprintf(buf, sizeof(buf), "%04d-%02d-%02d", y, m, d);
    return std::string(buf);
}

bool LevyAdvisor::buildRow(const std::string& periodStart, double crudeForecast, const Dataset& ds,
                           bool isLive, std::vector<double>& featureRow,
                           AssembledInputs& inputs) const {
    int carryRow = -1;
    if (isLive) {
        if (!ds.period_start.empty()) carryRow = static_cast<int>(ds.period_start.size()) - 1;
    } else {
        for (int i = static_cast<int>(ds.period_start.size()) - 1; i >= 0; i--)
            if (ds.period_start[i] < periodStart) { carryRow = i; break; }
    }
    if (carryRow < 0) {
        std::cout << "ERROR: no earlier row to carry from for " << periodStart << "\n";
        return false;
    }

    auto col = [&](const std::string& name) {
        for (size_t j = 0; j < ds.featureNames.size(); j++)
            if (ds.featureNames[j] == name) return static_cast<int>(j);
        return -1;
    };
    int iCrude = col("forecast_crude_price"), iTarget = col("levy_revenue_target_pkr_bn"),
        iVolume = col("levied_volume_bn_litres"), iRetail = col("retail_petrol_price"),
        iLevy = col("current_levy"), iAfford = col("affordability_index"),
        iGap = col("fiscal_gap_pkr_bn"), iReq = col("required_levy");
    if (iCrude < 0 || iTarget < 0 || iVolume < 0) {
        std::cout << "ERROR: dataset missing crude/target/volume feature columns\n";
        return false;
    }

    const std::vector<double>& src = ds.X[carryRow];
    auto get = [&](int i) { return i >= 0 ? src[i] : 0.0; };

    inputs.periodStart = periodStart;
    inputs.carryPeriod = ds.period_start[carryRow];
    inputs.forecast_crude_price = crudeForecast;
    inputs.crude_detail = isLive ? "user argument --crude" : "stored in levy_training.csv";
    inputs.retail_petrol_price = get(iRetail);
    inputs.current_levy = get(iLevy);
    inputs.affordability_index = get(iAfford);
    inputs.levy_revenue_target_pkr_bn = src[iTarget];
    inputs.levied_volume_bn_litres = src[iVolume];
    inputs.fiscal_gap_pkr_bn = get(iGap);
    if (inputs.levied_volume_bn_litres <= 0) {
        std::cout << "ERROR: non-positive volume for required_levy\n";
        return false;
    }
    inputs.required_levy = inputs.levy_revenue_target_pkr_bn / inputs.levied_volume_bn_litres;

    featureRow = src;
    featureRow[iCrude] = crudeForecast;
    if (iReq >= 0) featureRow[iReq] = inputs.required_levy;
    return true;
}

static void printInputs(const AssembledInputs& in) {
    auto line = [](const char* name, double v, const std::string& note) {
        std::cout << "  " << std::left << std::setw(28) << name << std::right
                  << std::setw(9) << v << "  " << note << "\n";
    };
    std::cout << "INPUTS (carried from period " << in.carryPeriod << " unless noted):\n";
    line("forecast_crude_price", in.forecast_crude_price, "[INPUT] " + in.crude_detail);
    line("retail_petrol_price", in.retail_petrol_price, "[CARRIED]");
    line("current_levy", in.current_levy, "[CARRIED]");
    line("affordability_index", in.affordability_index, "[CARRIED]");
    line("levy_revenue_target_pkr_bn", in.levy_revenue_target_pkr_bn, "[CARRIED]");
    line("levied_volume_bn_litres", in.levied_volume_bn_litres, "[CARRIED]");
    line("fiscal_gap_pkr_bn", in.fiscal_gap_pkr_bn, "[CARRIED]");
    line("required_levy", in.required_levy, "[CALCULATED] target / volume");
}

int runLiveMode(double crudePrice) {
    Dataset ds;
    if (!loadDataset(CSV_PATH, ds)) return 1;
    if (ds.y.size() < 10) {
        std::cout << "ERROR: dataset too small.\n";
        return 1;
    }

    LevyAdvisor advisor;

    std::string periodStart = nextFortnightStart(ds.period_start.back());
    if (periodStart.empty()) {
        std::cout << "ERROR: cannot determine live period start.\n";
        return 1;
    }

    std::vector<double> featureRow;
    AssembledInputs inputs;
    if (!advisor.buildRow(periodStart, crudePrice, ds, true, featureRow, inputs)) {
        return 1;
    }

    RegressionTree tree(3, 6, 3);
    tree.fit(ds.X, ds.y);

    double multiplier = tree.predict(featureRow);
    double recommendedLevy = inputs.required_levy * multiplier;
    int band = bandOf(recommendedLevy);

    std::cout << std::fixed << std::setprecision(3);
    std::cout << "========================================\n";
    std::cout << "   LEVY ADVISOR - LIVE RECOMMENDATION\n";
    std::cout << "========================================\n";
    std::cout << "Period Start:     " << periodStart << "\n";
    std::cout << "Training samples: " << ds.y.size() << " (all historical rows)\n\n";

    printInputs(inputs);

    std::cout << "\nRECOMMENDATION:\n";
    std::cout << "  Predicted multiplier:        " << multiplier << "\n";
    std::cout << "  Required levy:               " << inputs.required_levy << " PKR/litre\n";
    std::cout << "  Recommended levy:            " << recommendedLevy << " PKR/litre\n";
    std::cout << "  Recommended band:            " << bandName(band) << "\n";
    std::cout << "========================================\n";

    return 0;
}

int runBacktestMode(const std::string& targetDate) {
    if (!isValidDateFormat(targetDate)) {
        std::cout << "ERROR: invalid backtest date format '" << targetDate << "'. Expected YYYY-MM-DD.\n";
        return 1;
    }

    Dataset ds;
    if (!loadDataset(CSV_PATH, ds)) return 1;

    int targetIndex = -1;
    for (size_t i = 0; i < ds.period_start.size(); i++) {
        if (ds.period_start[i] == targetDate) {
            targetIndex = static_cast<int>(i);
            break;
        }
    }

    if (targetIndex < 0) {
        std::cout << "ERROR: target date " << targetDate << " not found in " << CSV_PATH << ".\n";
        return 1;
    }

    int earlierRows = targetIndex;
    if (earlierRows == 0) {
        std::cout << "ERROR: no earlier row to carry from for date " << targetDate << ".\n";
        return 1;
    }
    if (earlierRows < 10) {
        std::cout << "ERROR: backtest requires at least 10 earlier training rows (found "
                  << earlierRows << " for date " << targetDate << ").\n";
        return 1;
    }

    int idxCrude = -1;
    for (size_t j = 0; j < ds.featureNames.size(); j++) {
        if (ds.featureNames[j] == "forecast_crude_price") {
            idxCrude = static_cast<int>(j);
            break;
        }
    }
    if (idxCrude < 0) {
        std::cout << "ERROR: forecast_crude_price feature not found in dataset.\n";
        return 1;
    }

    double storedCrude = ds.X[targetIndex][idxCrude];
    double storedLevy = ds.levy[targetIndex];
    double storedCurrentLevy = ds.currentLevy[targetIndex];
    double storedScale = ds.scale[targetIndex];

    LevyAdvisor advisor;

    std::vector<double> featureRow;
    AssembledInputs inputs;
    if (!advisor.buildRow(targetDate, storedCrude, ds, false, featureRow, inputs)) {
        return 1;
    }

    std::vector<std::vector<double>> Xtrain(ds.X.begin(), ds.X.begin() + targetIndex);
    std::vector<double> ytrain(ds.y.begin(), ds.y.begin() + targetIndex);

    RegressionTree tree(3, 6, 3);
    tree.fit(Xtrain, ytrain);

    double multiplier = tree.predict(featureRow);
    double recommendedLevy = inputs.required_levy * multiplier;
    int band = bandOf(recommendedLevy);
    double absError = std::fabs(recommendedLevy - storedLevy);

    std::cout << std::fixed << std::setprecision(3);
    std::cout << "========================================\n";
    std::cout << " LEVY ADVISOR - BACKTEST RECOMMENDATION\n";
    std::cout << "========================================\n";
    std::cout << "Target Date:      " << targetDate << "\n";
    std::cout << "Training samples: " << targetIndex << " (chronological rows with period_start < " << targetDate << ")\n\n";

    printInputs(inputs);

    std::cout << "\nRECOMMENDATION:\n";
    std::cout << "  Predicted multiplier:        " << multiplier << "\n";
    std::cout << "  Required levy:               " << inputs.required_levy << " PKR/litre\n";
    std::cout << "  Recommended levy:            " << recommendedLevy << " PKR/litre\n";
    std::cout << "  Recommended band:            " << bandName(band) << "\n";

    std::cout << "\nEVALUATION VS STORED ROW (" << targetDate << "):\n";
    std::cout << "  Stored crude:                " << storedCrude << " USD/bbl\n";
    std::cout << "  Retail price used (carried): " << inputs.retail_petrol_price << " PKR/litre\n";
    std::cout << "  Stored current_levy:         " << storedCurrentLevy << " PKR/litre\n";
    std::cout << "  Stored required_levy:        " << storedScale << " PKR/litre\n";
    std::cout << "  Stored levy_value:           " << storedLevy << " PKR/litre (band: " << bandName(bandOf(storedLevy)) << ")\n";
    std::cout << "  Absolute error:              " << absError << " PKR/litre\n";
    std::cout << "  Note: Carried target/volume gave required_levy " << inputs.required_levy
              << " vs stored " << storedScale << " (diff: " << (inputs.required_levy - storedScale >= 0 ? "+" : "")
              << (inputs.required_levy - storedScale) << " PKR/litre)\n";
    std::cout << "========================================\n";

    return 0;
}
