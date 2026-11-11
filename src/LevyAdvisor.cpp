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

std::vector<std::string> splitLine(const std::string& line, char delim) {
    std::vector<std::string> parts;
    std::string cell;
    std::stringstream ss(line);
    while (std::getline(ss, cell, delim)) {
        if (!cell.empty() && cell.back() == '\r') cell.pop_back();
        parts.push_back(cell);
    }
    return parts;
}

int findCol(const std::vector<std::string>& header, const std::string& name) {
    for (size_t i = 0; i < header.size(); i++) {
        std::string h = header[i];
        if (!h.empty() && h.back() == '\r') h.pop_back();
        if (h == name) return static_cast<int>(i);
    }
    return -1;
}

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

bool loadPetrolBuildup(const std::string& path, std::vector<PetrolBuildupRow>& rows) {
    std::ifstream file(path);
    if (!file) {
        std::string fallback = "data/raw/petrol_price_buildup_fortnightly.csv";
        file.open(fallback);
        if (!file) {
            std::cout << "ERROR: cannot open petrol buildup file: " << path << "\n";
            return false;
        }
    }
    std::string line;
    if (!std::getline(file, line)) {
        std::cout << "ERROR: petrol buildup file is empty: " << path << "\n";
        return false;
    }
    char delim = (line.find('\t') != std::string::npos) ? '\t' : ',';
    std::vector<std::string> header = splitLine(line, delim);

    int cStart = findCol(header, "period_start");
    int cEnd = findCol(header, "period_end");
    int cPetroleum = findCol(header, "petroleum_levy_pkr_l");
    int cCarbon = findCol(header, "carbon_levy_pkr_l");
    int cRetail = findCol(header, "retail_petrol_price_model");

    if (cStart < 0 || cEnd < 0 || cPetroleum < 0 || cCarbon < 0 || cRetail < 0) {
        std::cout << "ERROR: petrol buildup file missing required columns by name\n";
        return false;
    }

    int maxIdx = std::max({cStart, cEnd, cPetroleum, cCarbon, cRetail});
    while (std::getline(file, line)) {
        if (line.empty()) continue;
        std::vector<std::string> cells = splitLine(line, delim);
        if (static_cast<int>(cells.size()) <= maxIdx) continue;
        try {
            PetrolBuildupRow r;
            r.period_start = cells[cStart];
            r.period_end = cells[cEnd];
            r.petroleum_levy_pkr_l = std::stod(cells[cPetroleum]);
            r.carbon_levy_pkr_l = std::stod(cells[cCarbon]);
            r.retail_petrol_price_model = std::stod(cells[cRetail]);
            rows.push_back(r);
        } catch (const std::exception&) {
            std::cout << "ERROR: invalid numerical value in petrol buildup line: " << line << "\n";
            return false;
        }
    }
    return !rows.empty();
}

bool loadCpiIncome(const std::string& path, std::vector<CpiIncomeRow>& rows) {
    std::ifstream file(path);
    if (!file) {
        std::string fallback = "data/raw/cpi_and_income_monthly.csv";
        file.open(fallback);
        if (!file) {
            std::cout << "ERROR: cannot open CPI/income file: " << path << "\n";
            return false;
        }
    }
    std::string line;
    if (!std::getline(file, line)) {
        std::cout << "ERROR: CPI/income file is empty: " << path << "\n";
        return false;
    }
    char delim = (line.find('\t') != std::string::npos) ? '\t' : ',';
    std::vector<std::string> header = splitLine(line, delim);

    int cMonth = findCol(header, "month");
    int cCpi = findCol(header, "cpi_index_oct2023_100");
    int cWage = findCol(header, "min_wage_pkr_month");

    if (cMonth < 0 || cCpi < 0 || cWage < 0) {
        std::cout << "ERROR: CPI/income file missing required columns by name\n";
        return false;
    }

    int maxIdx = std::max({cMonth, cCpi, cWage});
    while (std::getline(file, line)) {
        if (line.empty()) continue;
        std::vector<std::string> cells = splitLine(line, delim);
        if (static_cast<int>(cells.size()) <= maxIdx) continue;
        try {
            CpiIncomeRow r;
            r.month = cells[cMonth];
            r.cpi_index_oct2023_100 = std::stod(cells[cCpi]);
            r.min_wage_pkr_month = std::stod(cells[cWage]);
            rows.push_back(r);
        } catch (const std::exception&) {
            std::cout << "ERROR: invalid numerical value in CPI/income line: " << line << "\n";
            return false;
        }
    }
    return !rows.empty();
}

bool LevyAdvisor::loadRawFiles(const std::string& buildupPath, const std::string& cpiPath) {
    buildupRows.clear();
    cpiRows.clear();
    if (!loadPetrolBuildup(buildupPath, buildupRows)) return false;
    if (!loadCpiIncome(cpiPath, cpiRows)) return false;
    std::sort(buildupRows.begin(), buildupRows.end(), [](const PetrolBuildupRow& a, const PetrolBuildupRow& b) {
        return a.period_start < b.period_start;
    });
    std::sort(cpiRows.begin(), cpiRows.end(), [](const CpiIncomeRow& a, const CpiIncomeRow& b) {
        return a.month < b.month;
    });
    return true;
}

std::string LevyAdvisor::getLivePeriodStart() const {
    if (buildupRows.empty()) return "";
    return nextDay(buildupRows.back().period_end);
}

bool LevyAdvisor::buildRow(const std::string& periodStart,
                           double crudeForecast,
                           const Dataset& ds,
                           bool isLive,
                           std::vector<double>& featureRow,
                           AssembledInputs& inputs) const {
    inputs.periodStart = periodStart;

    // 1. forecast_crude_price
    inputs.forecast_crude_price = crudeForecast;
    inputs.crude_label = isLive ? "CALCULATED" : "CARRIED";
    std::ostringstream crudeOss;
    crudeOss << std::fixed << std::setprecision(3) << crudeForecast;
    inputs.crude_detail = isLive ? ("user argument --crude " + crudeOss.str())
                                 : ("stored in levy_training.csv for " + periodStart);

    // 2. retail_petrol_price & current_levy
    int foundBuildup = -1;
    for (int i = static_cast<int>(buildupRows.size()) - 1; i >= 0; i--) {
        if (buildupRows[i].period_end < periodStart) {
            foundBuildup = i;
            break;
        }
    }
    if (foundBuildup < 0) {
        std::cout << "ERROR: no build-up period found with period_end < " << periodStart << "\n";
        return false;
    }
    const auto& bRow = buildupRows[foundBuildup];
    inputs.retail_petrol_price = bRow.retail_petrol_price_model;
    inputs.retail_label = "CARRIED";
    inputs.retail_detail = "latest build-up period " + bRow.period_start + " to " + bRow.period_end;

    inputs.current_levy = bRow.petroleum_levy_pkr_l + bRow.carbon_levy_pkr_l;
    inputs.current_levy_label = "CALCULATED";
    std::ostringstream clOss;
    clOss << std::fixed << std::setprecision(3)
          << "petroleum " << bRow.petroleum_levy_pkr_l << " + carbon " << bRow.carbon_levy_pkr_l;
    inputs.current_levy_detail = clOss.str();

    // 3. min_wage and cpi_index
    std::string targetMonth = periodStart.substr(0, 7);
    int foundCpi = -1;
    for (int i = 0; i < static_cast<int>(cpiRows.size()); i++) {
        if (cpiRows[i].month == targetMonth) {
            foundCpi = i;
            break;
        }
    }
    bool carriedLatestCpi = false;
    if (foundCpi < 0) {
        if (cpiRows.empty()) {
            std::cout << "ERROR: CPI rows empty\n";
            return false;
        }
        foundCpi = static_cast<int>(cpiRows.size()) - 1;
        carriedLatestCpi = true;
    }
    const auto& cRow = cpiRows[foundCpi];
    inputs.min_wage_pkr_month = cRow.min_wage_pkr_month;
    inputs.min_wage_label = "CARRIED";
    inputs.min_wage_detail = (carriedLatestCpi ? "latest available month " : "month ") + cRow.month;

    inputs.cpi_index_oct2023_100 = cRow.cpi_index_oct2023_100;
    inputs.cpi_label = "CARRIED";
    inputs.cpi_detail = (carriedLatestCpi ? "latest available month " : "month ") + cRow.month;

    // 4. affordability_index = min_wage_pkr_month / (40 * retail_petrol_price) * (100 / cpi_index_oct2023_100) * 10
    if (inputs.retail_petrol_price <= 0 || inputs.cpi_index_oct2023_100 <= 0) {
        std::cout << "ERROR: non-positive retail price or CPI index for affordability formula\n";
        return false;
    }
    inputs.affordability_index = (inputs.min_wage_pkr_month / (40.0 * inputs.retail_petrol_price)) *
                                 (100.0 / inputs.cpi_index_oct2023_100) * 10.0;
    inputs.affordability_label = "CALCULATED";

    // 5. levy_revenue_target_pkr_bn, levied_volume_bn_litres, fiscal_gap_pkr_bn
    int carryRow = -1;
    if (isLive) {
        if (ds.period_start.empty()) {
            std::cout << "ERROR: dataset empty for live carry\n";
            return false;
        }
        carryRow = static_cast<int>(ds.period_start.size()) - 1;
    } else {
        for (int i = static_cast<int>(ds.period_start.size()) - 1; i >= 0; i--) {
            if (ds.period_start[i] < periodStart) {
                carryRow = i;
                break;
            }
        }
        if (carryRow < 0) {
            std::cout << "ERROR: no earlier row to carry from for date " << periodStart << "\n";
            return false;
        }
    }

    int idxTarget = -1, idxVolume = -1, idxFiscalGap = -1;
    for (size_t j = 0; j < ds.featureNames.size(); j++) {
        if (ds.featureNames[j] == "levy_revenue_target_pkr_bn") idxTarget = static_cast<int>(j);
        else if (ds.featureNames[j] == "levied_volume_bn_litres") idxVolume = static_cast<int>(j);
        else if (ds.featureNames[j] == "fiscal_gap_pkr_bn") idxFiscalGap = static_cast<int>(j);
    }
    if (idxTarget < 0 || idxVolume < 0 || idxFiscalGap < 0) {
        std::cout << "ERROR: dataset missing required feature columns (target/volume/fiscal_gap)\n";
        return false;
    }

    inputs.levy_revenue_target_pkr_bn = ds.X[carryRow][idxTarget];
    inputs.target_label = "CARRIED";
    inputs.target_detail = "from period " + ds.period_start[carryRow];

    inputs.levied_volume_bn_litres = ds.X[carryRow][idxVolume];
    inputs.volume_label = "CARRIED";
    inputs.volume_detail = "from period " + ds.period_start[carryRow];

    inputs.fiscal_gap_pkr_bn = ds.X[carryRow][idxFiscalGap];
    inputs.fiscal_gap_label = "CARRIED";
    inputs.fiscal_gap_detail = "from period " + ds.period_start[carryRow];

    // 6. required_levy = target / volume
    if (inputs.levied_volume_bn_litres <= 0) {
        std::cout << "ERROR: non-positive volume for required_levy\n";
        return false;
    }
    inputs.required_levy = inputs.levy_revenue_target_pkr_bn / inputs.levied_volume_bn_litres;
    inputs.required_levy_label = "CALCULATED";

    // 7. Order the values by Dataset.featureNames
    featureRow.clear();
    for (const std::string& name : ds.featureNames) {
        if (name == "forecast_crude_price") featureRow.push_back(inputs.forecast_crude_price);
        else if (name == "affordability_index") featureRow.push_back(inputs.affordability_index);
        else if (name == "levy_revenue_target_pkr_bn") featureRow.push_back(inputs.levy_revenue_target_pkr_bn);
        else if (name == "fiscal_gap_pkr_bn") featureRow.push_back(inputs.fiscal_gap_pkr_bn);
        else if (name == "current_levy") featureRow.push_back(inputs.current_levy);
        else if (name == "retail_petrol_price") featureRow.push_back(inputs.retail_petrol_price);
        else if (name == "levied_volume_bn_litres") featureRow.push_back(inputs.levied_volume_bn_litres);
        else if (name == "required_levy") featureRow.push_back(inputs.required_levy);
        else {
            std::cout << "ERROR: unknown feature name in ds.featureNames: " << name << "\n";
            return false;
        }
    }

    return true;
}

static void printInputs(const AssembledInputs& in) {
    std::cout << "INPUTS:\n";
    std::cout << "  forecast_crude_price:        " << std::setw(8) << in.forecast_crude_price
              << "  [" << in.crude_label << "] (" << in.crude_detail << ")\n";
    std::cout << "  retail_petrol_price:         " << std::setw(8) << in.retail_petrol_price
              << "  [" << in.retail_label << "] (" << in.retail_detail << ")\n";
    std::cout << "  current_levy:                " << std::setw(8) << in.current_levy
              << "  [" << in.current_levy_label << "] (" << in.current_levy_detail << ")\n";
    std::cout << "  min_wage_pkr_month:          " << std::setw(8) << in.min_wage_pkr_month
              << "  [" << in.min_wage_label << "] (" << in.min_wage_detail << ")\n";
    std::cout << "  cpi_index_oct2023_100:       " << std::setw(8) << in.cpi_index_oct2023_100
              << "  [" << in.cpi_label << "] (" << in.cpi_detail << ")\n";
    std::cout << "  affordability_index:         " << std::setw(8) << in.affordability_index
              << "  [" << in.affordability_label << "] (formula: wage / (40 * retail) * (100 / cpi) * 10)\n";
    std::cout << "  levy_revenue_target_pkr_bn:  " << std::setw(8) << in.levy_revenue_target_pkr_bn
              << "  [" << in.target_label << "] (" << in.target_detail << ")\n";
    std::cout << "  levied_volume_bn_litres:     " << std::setw(8) << in.levied_volume_bn_litres
              << "  [" << in.volume_label << "] (" << in.volume_detail << ")\n";
    std::cout << "  fiscal_gap_pkr_bn:           " << std::setw(8) << in.fiscal_gap_pkr_bn
              << "  [" << in.fiscal_gap_label << "] (" << in.fiscal_gap_detail << ")\n";
    std::cout << "  required_levy:               " << std::setw(8) << in.required_levy
              << "  [" << in.required_levy_label << "] (target / volume)\n";
}

int runLiveMode(double crudePrice) {
    Dataset ds;
    if (!loadDataset(CSV_PATH, ds)) return 1;
    if (ds.y.size() < 10) {
        std::cout << "ERROR: dataset too small.\n";
        return 1;
    }

    LevyAdvisor advisor;
    if (!advisor.loadRawFiles()) return 1;

    std::string periodStart = advisor.getLivePeriodStart();
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
    if (!advisor.loadRawFiles()) return 1;

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
    std::cout << "  Stored retail price:         " << inputs.retail_petrol_price << " PKR/litre\n";
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
