#include <iostream>
#include <vector>
#include <cmath>
#include <algorithm>
using namespace std;

const double MIN_GAIN              = 1e-4;  // ignore splits that only fit rounding noise

struct TreeNode {
    bool isLeaf;
    int featureIdx;     // which feature this node splits on
    double threshold;   // the best Split Score given by a feature on this node
    double prediction;  // mean of y of the samples that reached this node
    int nSamples;
    TreeNode* left;          // value <= threshold
    TreeNode* right;         // value >  threshold
    TreeNode() : isLeaf(true), featureIdx(-1), threshold(0), prediction(0),
             nSamples(0), left(nullptr), right(nullptr) {}
};

// Result of searching for the best question to ask at a node
struct Split {
    bool found;
    int featureIdx;
    double threshold;
    double cost;   // combined SSE (Sum of Squared Errors) of left + right after the split
    // total cost after splitting (lower = better)
};

// a new split must beat the best one by a tiny margin, 
// so rounding noise cannot flip ties differently between the two search methods
static bool improves(double cost, double best) {
    return cost < best - 1e-12 * (1.0 + fabs(best));
}

/* ============================================================
   REGRESSION TREE
   X = table of inputs: X[i][j] = feature j of sample i
   y = list of answers: y[i]    = levy of sample i
   ============================================================ */
class RegressionTree {
private:
    TreeNode* root;
    int maxDepth;
    int minSamplesSplit;  // a node with fewer samples than this stays a leaf
    int minSamplesLeaf;

    // Recursive partitioning: greedy, locally best split, no backtracking.
    // idx = indices of the rows that reached this node.
    TreeNode* build(const vector<vector<double>>& X, const vector<double>& y, const vector<int>& idx, int depth) {
        TreeNode* node = new TreeNode();
        int n = idx.size();
        node->nSamples = n;

        // compute prediction and own error (SSE) of this node
        double sum = 0, sumSq = 0;
        for (int k = 0; k < n; k++) { sum += y[idx[k]]; sumSq += y[idx[k]] * y[idx[k]]; }
        node->prediction = sum / n;
        double ownError = sumSq - sum * sum / n;

        if (depth >= maxDepth) return node;          // base cases
        if (n < minSamplesSplit) return node;
        if (ownError < 1e-12) return node;

        Split s = findBestSplit(X, y, idx);
        if (!s.found) return node;
        if (s.cost >= ownError - MIN_GAIN) return node; // split barely helps

        vector<int> leftIdx, rightIdx;
        for (int k = 0; k < n; k++) {
            int row = idx[k];
            if (X[row][s.featureIdx] <= s.threshold) 
                leftIdx.push_back(row);
            else 
                rightIdx.push_back(row);
        }
        node->isLeaf = false;
        node->featureIdx = s.featureIdx;
        node->threshold = s.threshold;
        node->left = build(X, y, leftIdx, depth + 1);
        node->right = build(X, y, rightIdx, depth + 1);
        return node;
    }

    // Ek node ko kis Question ki basis pe split kary??
    // SSE of a group = sum(y^2) - (sum(y))^2 / count. 
    // And Cost of a split = SSE(left) + SSE(right); lower is better.
    Split findBestSplit(const vector<vector<double>>& X, const vector<double>& y,
                              const vector<int>& idx) {
        
        Split best{false, -1, 0, 1e300};        
        int n = idx.size(), numFeatures = X[0].size();
        
        double totalSum = 0, totalSq = 0;
        for (int k = 0; k < n; k++) { 
            double v = y[idx[k]]; 
            totalSum += v; 
            totalSq += v * v; 
        }

        for (int j = 0; j < numFeatures; j++) {
            vector<pair<double, double>> data;
            for (int k = 0; k < n; k++) {
                data.push_back({X[idx[k]][j], y[idx[k]]});
            }
            sort(data.begin(), data.end());

            double sumLeft = 0, sqLeft = 0; // running totals...
            for (int k = 0; k < n - 1; k++) {
                sumLeft += data[k].second;
                sqLeft += data[k].second * data[k].second;
                
                // cnt of samples/rows in left and right child.
                int cL = k + 1, cR = n - cL;

                if (data[k].first == data[k + 1].first) continue;     // cannot cut between equal values
                if (cL < minSamplesLeaf || cR < minSamplesLeaf) continue;
                
                double sumRight = totalSum - sumLeft, sqRight = totalSq - sqLeft;
                double cost = (sqLeft - sumLeft * sumLeft / cL) + (sqRight - sumRight * sumRight / cR);
                
                if (improves(cost, best.cost)) {
                    best = {true, j, (data[k].first + data[k + 1].first) / 2.0, cost};
                }
            }
        }
        return best;
    }
};


int main() {

}