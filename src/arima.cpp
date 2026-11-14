#include <iostream>
#include <vector>
#include <cmath>
#include <fstream>
#include <sstream>
#include <algorithm>
#include "arima.h"
using namespace std;

const int MIN_TRAIN = 250;
// x[t] = c + φ1·x[t-1] + φ2·x[t-2] + ... + φp·x[t-p] + e[t] -> general eq for AR(p)
bool loadCSV(const string &path, vector<string> &dates, vector<double> &prices)
{
    ifstream file(path);
    if (!file.is_open())
        return false;

    string line;
    getline(file, line);

    while (getline(file, line))
    {
        if (line.empty())
            continue;
        stringstream ss(line);
        string date, priceStr;
        getline(ss, date, ',');
        getline(ss, priceStr, ',');

        try
        {
            double price = stod(priceStr);
            if (price <= 0) // log(0 ya negative) se bachne ke liye
                continue;
            dates.push_back(date);
            prices.push_back(price);
        }
        catch (...)
        {
            continue;
        }
    }
    return !prices.empty(); // empty file pe false
}

// Step 1
void ARIMA::toLog(vector<double> &data) const
{
    for (double &d : data)
        d = log(d);
}
void ARIMA::toAntiLog(vector<double> &data) const
{
    for (double &d : data)
        d = exp(d);
}
void ARIMA::toAntiLog(vector<double> &data, size_t start, size_t end) const
{
    end = min(end, data.size());
    for (size_t i = start; i < end; i++)
        data[i] = exp(data[i]);
}
vector<double> ARIMA::makeStationary(vector<double> &data) const
{
    toLog(data);
    vector<double> newData;
    for (size_t i = 0; i + 1 < data.size(); i++) // underflow fix
        newData.push_back(data[i + 1] - data[i]);
    return newData;
}
// Step 2
vector<double> ARIMA::getCoefficients(const vector<double> &data, int p) const
{
    int n = data.size();

    // Mean
    double mean = 0.0;
    for (int i = 0; i < n; i++)
        mean += data[i];
    mean /= n;

    // Autocorrelations r[0] to r[p]
    vector<double> r(p + 1, 0.0);
    for (int k = 0; k <= p; k++)
    {
        for (int t = k; t < n; t++)
            r[k] += (data[t] - mean) * (data[t - k] - mean);
        r[k] /= n;
    }
    if (r[0] == 0.0)
        return vector<double>(p + 1, 0.0);

    // Single-pass Levinson-Durbin: each order m builds on order m-1.
    // Order selection is done by rolling-window RMSE validation,
    // not AIC/BIC.
    vector<double> phi(p + 1, 0.0);
    vector<double> phi_prev(p + 1, 0.0);
    double sigma2 = r[0];

    phi[1] = r[1] / r[0];
    sigma2 *= (1.0 - phi[1] * phi[1]);
    if (sigma2 <= 0.0)
        sigma2 = 1e-12;

    for (int m = 2; m <= p; m++)
    {
        phi_prev = phi;
        double num = r[m];
        for (int j = 1; j <= m - 1; j++)
            num -= phi_prev[j] * r[m - j];
        double km = num / sigma2;
        phi[m] = km;
        for (int j = 1; j <= m - 1; j++)
            phi[j] = phi_prev[j] - km * phi_prev[m - j];
        sigma2 *= (1.0 - km * km);
        if (sigma2 <= 0.0)
            sigma2 = 1e-12;
    }

    // coeffs[0] = intercept, coeffs[1]=phi1, coeffs[2]=phi2 ...
    vector<double> result(p + 1);
    double sumPhi = 0.0;
    for (int i = 1; i <= p; i++) sumPhi += phi[i];
    result[0] = mean * (1.0 - sumPhi);
    for (int i = 1; i <= p; i++)
        result[i] = phi[i];
    return result;
}

// Step 3
vector<double> ARIMA::predictValues(const vector<double> &data, const vector<double> &coeffs, int days) const
{
    vector<double> predictedval;
    if (coeffs.size() < 2 || data.size() < coeffs.size() - 1) // bounds check
        return predictedval;

    vector<double> pseudo_data = data;
    int pseudo_data_handler = pseudo_data.size() - 1;
    int pseudo_days = 1;
    while (pseudo_days <= days)
    {
        double val = 0;
        for (size_t i = 0; i < coeffs.size(); i++)
        {
            if (i == 0)
                val += coeffs[i];
            else
                val += pseudo_data[pseudo_data_handler--] * coeffs[i];
        }
        predictedval.push_back(val);
        pseudo_data.push_back(val);
        pseudo_data_handler = pseudo_data.size() - 1;
        pseudo_days++;
    }
    return predictedval;
}

// Step 3.5
vector<double> ARIMA::integrateValues(const vector<double> &predicted, const vector<double> &original) const
{
    vector<double> integrated_val;
    if (predicted.empty() || original.empty()) // crash se bachne ke liye
        return integrated_val;

    integrated_val.push_back(original.back() + predicted[0]);
    for (size_t i = 1; i < predicted.size(); i++)
        integrated_val.push_back(integrated_val.back() + predicted[i]);
    toAntiLog(integrated_val);
    return integrated_val;
}

// Step 4
double ARIMA::getRMSE(const vector<double> &predicted, vector<double> actual, size_t actualidx) const
{
    // actual: log values (by-value, taaki caller ka vector na badle)
    size_t end = min(actualidx + predicted.size(), actual.size());
    if (actualidx >= end)
        return -1;

    toAntiLog(actual, actualidx, end);

    double sum = 0;
    for (size_t i = actualidx; i < end; i++)
    {
        double e = predicted[i - actualidx] - actual[i];
        sum += e * e;
    }
    return sqrt(sum / (end - actualidx));
}

// Step 5
double ARIMA::testOneWindow(const vector<double> &data, int p, int trainEnd) const
{
    if (trainEnd < 2 || (size_t)trainEnd >= data.size()) // invalid window
        return -1;

    vector<double> logAll = data;
    toLog(logAll); // RMSE ke liye poora log vector

    vector<double> train(data.begin(), data.begin() + trainEnd);
    vector<double> diff = makeStationary(train); // train ab log ho gaya

    vector<double> c = getCoefficients(diff, p);
    vector<double> pred = predictValues(diff, c, days);
    if (pred.empty())
        return -1;
    vector<double> integ = integrateValues(pred, train);
    return getRMSE(integ, logAll, trainEnd);
}
double ARIMA::testAllWindows(const vector<double> &data, int p) const
{
    double err = 0;
    int window_count = 0;
    for (size_t i = startTrain; i + days <= data.size(); i += step)
    {
        double temp = testOneWindow(data, p, i);
        if (temp < 0)
            continue;
        err += temp;

        window_count++;
    }
    if (window_count == 0)
        return -1;
    return err / window_count;
}
int ARIMA::findBestP(const vector<double> &data)
{
    double Min = __DBL_MAX__;
    int minP = -1;
    for (int i = 1; i <= maxP; i++)
    {
        double temp = testAllWindows(data, i);
        if (temp < 0)
            continue;
        if (temp < Min)
        {
            Min = temp;
            minP = i;
        }
    }
    // bestP = minP;
    bestErr = (minP == -1) ? -1 : Min;
    return minP;
}
double ARIMA::baselineRMSE(const vector<double> &data) const{

    // min 2 vals required
    if(data.size() < 2)
        return -1;

    double sum = 0.0;
    int count = 0;

    for(size_t i = 1; i < data.size(); i++){

        double predicted = data[i - 1];
        double actual = data[i];

        double err = predicted - actual; //calcs error
        sum += err * err;
        count++;
    }

    // this is the baselineRMSE:
    return sqrt(sum / count);
}

int ARIMA::getBestP() const
{
    if (bestP==-1) return -1;
    return bestP;
}

ARIMA::ARIMA(int maxP, int days, int step, int startTrain)
    : maxP(maxP), days(days), step(step), startTrain(startTrain), bestP(-1), bestErr(-1)
{
}

void ARIMA::trainModel(const vector<double> &data)
{
    bestP = findBestP(data);
    if (bestP < 1) return;

    prices = data;                       // raw prices store

    vector<double> logPrices = data;
    vector<double> diff = makeStationary(logPrices);
    coeffs = getCoefficients(diff, bestP);
}

vector<double> ARIMA::forecast(int n) const
{
    if (bestP < 1 || prices.size() < 2) return {};

    vector<double> logPrices = prices;   // copy, kyunki const function hai
    vector<double> diff = makeStationary(logPrices);

    vector<double> pred = predictValues(diff, coeffs, n);
    return integrateValues(pred, vector<double>{logPrices.back()});
}

double ARIMA::getBestErr() const
{
    return bestErr;
}

double forecastMean(const std::vector<double>& prices_up_to_cutoff,
                    int n_days)
{
    ARIMA model(10, n_days, 7, MIN_TRAIN);
    model.trainModel(prices_up_to_cutoff);
    if (model.getBestP() == -1)
        return -1.0;

    const std::vector<double> predicted = model.forecast(n_days);
    if (predicted.empty())
        return -1.0;

    double sum = 0.0;
    for (double price : predicted)
        sum += price;
    return sum / predicted.size();
}

#ifdef ARIMA_STANDALONE
int main()
{
    return 0;
}
#endif
ArimaResult forecastWithEvaluation(const std::vector<double>& prices, int horizon) {
    ArimaResult result;
    if (horizon < 1 || horizon > 365) {
        result.error = "Forecast horizon must be between 1 and 365 observations.";
        return result;
    }
    // Order selection needs at least one validation window inside the training prefix.
    if (prices.size() < static_cast<size_t>(MIN_TRAIN + 2 * horizon)) {
        result.error = "Insufficient history for a separate chronological holdout and model selection.";
        return result;
    }
    for (double price : prices) {
        if (!std::isfinite(price) || price <= 0) {
            result.error = "ARIMA requires finite, positive prices.";
            return result;
        }
    }
    result.testCount = horizon;
    result.trainCount = static_cast<int>(prices.size()) - horizon;
    std::vector<double> train(prices.begin(), prices.begin() + result.trainCount);
    ARIMA evaluation(10, horizon, 7, MIN_TRAIN);
    evaluation.trainModel(train);
    const auto heldOut = evaluation.forecast(horizon);
    if (heldOut.size() != static_cast<size_t>(horizon)) {
        result.error = "ARIMA could not fit the holdout evaluation model.";
        return result;
    }
    double squared = 0, absolute = 0, percentage = 0;
    for (int i = 0; i < horizon; ++i) {
        if (!std::isfinite(heldOut[i]) || heldOut[i] <= 0) {
            result.error = "ARIMA returned an invalid holdout prediction.";
            return result;
        }
        const double actual = prices[result.trainCount + i];
        const double error = heldOut[i] - actual;
        squared += error * error;
        absolute += std::abs(error);
        percentage += std::abs(error) / actual;
    }
    result.rmse = std::sqrt(squared / horizon);
    result.mae = absolute / horizon;
    result.mape = 100 * percentage / horizon;
    result.evaluationP = evaluation.getBestP();

    // Refit on all known observations for the future forecast only after evaluation.
    ARIMA finalModel(10, horizon, 7, MIN_TRAIN);
    finalModel.trainModel(prices);
    const auto future = finalModel.forecast(horizon);
    if (future.size() != static_cast<size_t>(horizon)) {
        result.error = "ARIMA could not fit the future forecast model.";
        return result;
    }
    for (double prediction : future) {
        if (!std::isfinite(prediction) || prediction <= 0) {
            result.error = "ARIMA returned an invalid future prediction.";
            return result;
        }
        result.mean += prediction / horizon;
    }
    result.next = future.front();
    result.p = finalModel.getBestP();
    result.ok = true;
    return result;
}
