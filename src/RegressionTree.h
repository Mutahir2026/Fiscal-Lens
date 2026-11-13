#pragma once
#include <vector>
#include <string>
using namespace std;

const string CSV_PATH = "../data/processed/levy_training.csv";
const bool PREDICT_MULTIPLIER = true;
const bool USE_REQUIRED_LEVY_FEATURE = true;
const bool RULE_FEATURES_ONLY = false;
const double TRAIN_FRACTION = 0.8;
const double MIN_GAIN = 1e-4;

struct Node {
    bool isLeaf;
    int featureIndex;
    double threshold;
    double prediction;
    int nSamples;
    Node* left;
    Node* right;
    Node() : isLeaf(true), featureIndex(-1), threshold(0), prediction(0),
             nSamples(0), left(nullptr), right(nullptr) {}
};

struct Split {
    bool found;
    int feature;
    double threshold;
    double cost;
};

class RegressionTree {
private:
    Node* root;
    int maxDepth, minSamplesSplit, minSamplesLeaf;

    Split findBestSplit(const vector<vector<double>>& X, const vector<double>& y,
                        const vector<int>& idx);
    Node* build(const vector<vector<double>>& X, const vector<double>& y,
                const vector<int>& idx, int depth);
    void freeTree(Node* node);
    int countLeaves(Node* node) const;
    void printNode(Node* node, const vector<string>& names, const string& label, int indent) const;

public:
    RegressionTree(int depth = 4, int minSplit = 6, int minLeaf = 3);
    ~RegressionTree();
    RegressionTree(const RegressionTree&) = delete;
    RegressionTree& operator=(const RegressionTree&) = delete;

    void fit(const vector<vector<double>>& X, const vector<double>& y);
    double predict(const vector<double>& x) const;
    int leaves() const;
    void print(const vector<string>& names, const string& label) const;
};

struct Dataset {
    vector<string> featureNames;
    vector<vector<double>> X;
    vector<double> y;
    vector<double> levy;
    vector<double> scale;
    vector<double> currentLevy;
    vector<string> period_start;
};

struct Metrics { double rmse, mae, mape, r2; };

bool loadDataset(const string& path, Dataset& ds);
Metrics evaluate(const vector<double>& actual, const vector<double>& pred);
double rmseOf(const vector<double>& a, const vector<double>& p);
int bandOf(double levy);
const char* bandName(int b);
double levyPrediction(const RegressionTree& t, const Dataset& ds, int i);
double walkForward(const Dataset& ds, int trainCount, int startSize, int step,
                   int depth, int minSplit, int minLeaf);
double walkForwardPersistence(const Dataset& ds, int trainCount, int startSize, int step);
