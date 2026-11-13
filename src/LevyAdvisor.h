#pragma once
#include "RegressionTree.h"
#include <string>
#include <vector>

struct AssembledInputs {
    std::string periodStart;
    std::string carryPeriod;       // training row the carried values came from
    double forecast_crude_price;
    std::string crude_detail;
    double retail_petrol_price;
    double current_levy;
    double affordability_index;
    double levy_revenue_target_pkr_bn;
    double levied_volume_bn_litres;
    double fiscal_gap_pkr_bn;
    double required_levy;
};

std::string nextDay(const std::string& dateStr);
bool isValidDateFormat(const std::string& dateStr);
std::string nextFortnightStart(const std::string& lastStart);

class LevyAdvisor {
public:
    bool buildRow(const std::string& periodStart,
                  double crudeForecast,
                  const Dataset& ds,
                  bool isLive,
                  std::vector<double>& featureRow,
                  AssembledInputs& inputs) const;
};

int runLiveMode(double crudePrice);
int runBacktestMode(const std::string& targetDate);
