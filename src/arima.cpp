#include <iostream>
#include <vector>
#include <cmath>
#include <fstream>
#include <sstream>
#include <algorithm>
using namespace std;
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

class ARIMA
{
    // Step 1
    void toLog(vector<double> &data) const
    {
        for (double &d : data)
            d = log(d);
    }
    void toAntiLog(vector<double> &data) const
    {
        for (double &d : data)
            d = exp(d);
    }
    void toAntiLog(vector<double> &data, size_t start, size_t end) const
    {
        end = min(end, data.size());
        for (size_t i = start; i < end; i++)
            data[i] = exp(data[i]);
    }
    vector<double> makeStationary(vector<double> &data) const
    {
        toLog(data);
        vector<double> newData;
        for (size_t i = 0; i + 1 < data.size(); i++) // underflow fix
            newData.push_back(data[i + 1] - data[i]);
        return newData;
    }
    // Step 2
    vector<double> getCoefficients(const vector<double> &data, int p)const
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

        // Levinson-Durbin
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
        result[0] = mean; // intercept = mean
        for (int i = 1; i <= p; i++)
            result[i] = phi[i];
        return result;
    }

    // Step 3
    vector<double> predictValues(const vector<double> &data, const vector<double> &coeffs, int days) const
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
    vector<double> integrateValues(const vector<double> &predicted, const vector<double> &original) const
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
    double getRMSE(const vector<double> &predicted, vector<double> actual, size_t actualidx) const
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
    double testOneWindow(const vector<double> &data, int p, int trainEnd) const
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
    double testAllWindows(const vector<double> &data, int p) const;
    int findBestP(const vector<double> &data);
    double baselineRMSE(const vector<double> &data) const;

    int getBestP() const;
    const vector<double> &getErrors() const; // errors[p-1] = avg RMSE of AR(p)

    int maxP;
    int days;
    int step;
    int startTrain;

    int bestP;
    vector<double> errors;
    vector<double> coeffs;
    vector<double> history;

public:
    ARIMA(int maxP = 10, int days = 30, int step = 7, int startTrain = 700):maxP(maxP),days(days),step(step),startTrain(startTrain){}
    void trainModel(const vector<double> &data);
    vector<double> forecast(int days) const;
};
int main()
{
    return 0;
}