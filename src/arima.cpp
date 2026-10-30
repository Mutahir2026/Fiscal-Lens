#include <iostream>
#include <vector>
#include <cmath>
#include <fstream>
#include <sstream>
using namespace std;
// x[t] = c + φ1·x[t-1] + φ2·x[t-2] + ... + φp·x[t-p] + e[t] -> general eq for AR(p)
bool loadCSV(const string &path, vector<string> &dates, vector<double> &prices)
{
    ifstream file(path);
    if (!file.is_open())
        return false; // file nahi mili

    string line;
    getline(file, line); // pehli line (header) skip

    while (getline(file, line))
    {
        if (line.empty())
            continue;
        stringstream ss(line);
        string date, priceStr;
        getline(ss, date, ','); // this breakage can be done with the help of stringstream
        getline(ss, priceStr, ',');

        try
        {
            double price = stod(priceStr); // string se double
            dates.push_back(date);
            prices.push_back(price);
        }
        catch (...)
        {
            continue;
        }
    }
    return true;
}
class ARIMA
{
    // days ye batata hai ke har window mein kitne din aage predict karne hain. Default 30 hai,
    //  matlab 700 din pe train karo to 701 se 730 tak ke 30 din predict honge aur unka RMSE nikalega.
    // step ye batata hai ke agli window kitna aage khiskegi. Default 7 hai,
    // Step 1
    void toLog(vector<double> &data)
    {
        for (double &d : data)
        {
            d = log(d);
        }
    }
    void toAntiLog(vector<double> &data)
    {
        for (double &d : data)
        {
            d = exp(d);
        }
    }
    vector<double> makeStationary(vector<double> &data)
    {
        toLog(data);
        vector<double> newData;
        for (int i = 0; i < data.size() - 1; i++)
        {
            newData.push_back(data[i + 1] - data[i]);
        }
        return newData;
    }
    // Step 2
    vector<double> getCoefficients(const vector<double>& data, int p)
    {
    int n = data.size();

    // Mean
    double mean = 0.0;
    for (int i = 0; i < n; i++) mean += data[i];
    mean /= n;

    // Autocorrelations r[0] to r[p]
    vector<double> r(p + 1, 0.0);
    for (int k = 0; k <= p; k++) {
        for (int t = k; t < n; t++)
            r[k] += (data[t] - mean) * (data[t - k] - mean);
        r[k] /= n;
    }
    if (r[0] == 0.0) return vector<double>(p + 1, 0.0);

    // Levinson-Durbin
    vector<double> phi(p + 1, 0.0);
    vector<double> phi_prev(p + 1, 0.0);
    double sigma2 = r[0];

    phi[1] = r[1] / r[0];
    sigma2 *= (1.0 - phi[1] * phi[1]);
    if (sigma2 <= 0.0) sigma2 = 1e-12;

    for (int m = 2; m <= p; m++) {
        phi_prev = phi;
        double num = r[m];
        for (int j = 1; j <= m - 1; j++)
            num -= phi_prev[j] * r[m - j];
        double km = num / sigma2;
        phi[m] = km;
        for (int j = 1; j <= m - 1; j++)
            phi[j] = phi_prev[j] - km * phi_prev[m - j];
        sigma2 *= (1.0 - km * km);
        if (sigma2 <= 0.0) sigma2 = 1e-12;
    }

    // coeffs[0] = intercept, coeffs[1]=phi1, coeffs[2]=phi2 ...
    vector<double> result(p + 1);
    result[0] = mean;                              // intercept = mean
    for (int i = 1; i <= p; i++) result[i] = phi[i];
    return result;
    }

    // Step 3
    vector<double> predictValues(const vector<double> &data, const vector<double> &coeffs, int days)
    {
        vector<double> predictedval;
        vector<double> pseudo_data=data;
         int pseudo_data_handler=pseudo_data.size()-1;
        int pseudo_days = 1;
        while (pseudo_days <= days)
        {
            double val = 0;
            for (int i = 0; i < coeffs.size(); i++)
            {
                if (i == 0)
                {
                    val += coeffs[i];
                }
                else 
                {
                    val += pseudo_data[pseudo_data_handler--] * coeffs[i];
                }
            }
            predictedval.push_back(val);
            pseudo_data.push_back(val);
            pseudo_data_handler=pseudo_data.size()-1;
            pseudo_days++;
        }
        return predictedval;
    }

    // Step 3.5
    vector<double> integrateValues(const vector<double> &predicted,const vector<double> &original)
    {
        vector<double> integrated_val;
        integrated_val.push_back(original.back()+predicted[0]);
         for (int i = 1; i < predicted.size(); i++)
        {
            integrated_val.push_back(integrated_val.back()+predicted[i]);
        }
        toAntiLog(integrated_val);
        return integrated_val;
    }

    // Step 4
    double getRMSE(const vector<double> &predicted,const vector<double> &actual,size_t actualidx)
   {
    double sum=0;
    int count_n=0;
       for (size_t i=actualidx;i<actualidx+predicted.size();i++)
       {
        if (i>=actual.size()) break;
        double e=predicted[i-actualidx]-actual[i];
        sum+=e*e;
        count_n++;
       }
       if (count_n==0) return -1;
       double ans=sum*(1.0/count_n);
          return sqrt(ans);
   }

    // Step 5 (improved)
    double testOneWindow(const vector<double> &data, int p, int trainEnd) const;
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
    ARIMA(int maxP = 10, int days = 30, int step = 7, int startTrain = 700);
    void trainModel(const vector<double> &data);
    vector<double> forecast(int days) const;
};
int main()
{
    return 0;
}