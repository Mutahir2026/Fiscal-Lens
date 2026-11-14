#pragma once
#include <string>
#include <vector>

extern const int MIN_TRAIN;

double forecastMean(const std::vector<double>& prices_up_to_cutoff,
                    int n_days);

// CSV format: date,price (pehli line header). price <= 0 wali rows skip hoti hain.
bool loadCSV(const std::string &path,
             std::vector<std::string> &dates,
             std::vector<double> &prices);

// AR(p) on log-differenced prices (d = 1 fixed).
// x[t] = c + phi1*x[t-1] + ... + phip*x[t-p] + e[t]
class ARIMA
{
public:
    ARIMA(int maxP = 10, int days = 30, int step = 7, int startTrain = 700);

    // p chunta hai (rolling-window validation), phir poore data pe final fit karta hai
    void trainModel(const std::vector<double> &data);

    // agle n din ki prices (antilog ho chuki). Train se pehle empty vector
    std::vector<double> forecast(int n) const;

    int getBestP() const;       // -1 agar train nahi hua / data kam
    double getBestErr() const;  // validation avg RMSE (final model ka RMSE nahi)
    double baselineRMSE(const std::vector<double> &data) const;

private:
    // Step 1: log + difference
    void toLog(std::vector<double> &data) const;
    void toAntiLog(std::vector<double> &data) const;
    void toAntiLog(std::vector<double> &data, size_t start, size_t end) const;
    std::vector<double> makeStationary(std::vector<double> &data) const; // data in-place log ho jata hai

    // Step 2: Yule-Walker via Levinson-Durbin. result[0] = intercept, result[1..p] = phi
    std::vector<double> getCoefficients(const std::vector<double> &data, int p) const;

    // Step 3: recursive multi-step prediction (diff scale)
    std::vector<double> predictValues(const std::vector<double> &data,
                                      const std::vector<double> &coeffs, int days) const;

    // Step 3.5: cumulative sum from original.back() (log) + antilog
    std::vector<double> integrateValues(const std::vector<double> &predicted,
                                        const std::vector<double> &original) const;

    // Step 4: RMSE (actual = log values)
    double getRMSE(const std::vector<double> &predicted,
                   std::vector<double> actual, size_t actualidx) const;

    // Step 5: validation
    double testOneWindow(const std::vector<double> &data, int p, int trainEnd) const;
    double testAllWindows(const std::vector<double> &data, int p) const;
    int findBestP(const std::vector<double> &data);

    int maxP;
    int days;
    int step;
    int startTrain;

    int bestP;
    double bestErr;
    std::vector<double> coeffs;
    std::vector<double> prices; // raw prices (forecast ka starting point)
};
// Metrics on the final horizon observations, excluded from fitting and order selection.
struct ArimaResult {
    bool ok = false;
    std::string error;
    double mean = 0, next = 0, mape = 0, rmse = 0, mae = 0;
    int p = -1, d = 1, testCount = 0, trainCount = 0, evaluationP = -1;
};
ArimaResult forecastWithEvaluation(const std::vector<double>& prices, int horizon);
