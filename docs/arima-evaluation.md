# ARIMA evaluation

Run Forecast returns a future forecast and a chronological holdout evaluation.
The final 12 or 24 observations (matching the selected horizon) are excluded
from both fitting and AR-order selection for evaluation. The existing ARIMA
implementation selects p on rolling validation windows within the earlier
training prefix. It then predicts the entire holdout without seeing any of its
prices. A separate model is fitted on all available history for the future
forecast and selected-model panel. Its p can differ from the evaluation p.

MAPE = mean(abs(predicted - actual) / actual) * 100.
RMSE = sqrt(mean((predicted - actual)^2)).
MAE = mean(abs(predicted - actual)).
Errors are computed on original prices, not logs. MAPE is a percentage;
RMSE and MAE use the selected series' units. These summarize one recent
holdout window and do not guarantee future performance.

The evaluation requires at least 250 + 2 * horizon observations so that the
training prefix has a full internal validation window. Invalid or insufficient
inputs return an explicit error; they do not produce placeholder accuracy.

From the repository root:

    node tests/arima_wasm_test.cjs
    g++ -O2 -std=c++17 -Isrc tests/arima_evaluation_test.cpp src/arima.cpp -o tests/arima_evaluation_test.exe
    ./tests/arima_evaluation_test.exe

The native test checks the metrics against independently fitted holdout
predictions, constant-series errors, invalid inputs, the minimum history
boundary, and evaluation-order isolation from changed holdout values.
The WebAssembly test checks both series and horizons through the JS bridge,
forecast consistency with the original mean API, and levy/tree regressions.
Rebuild deployment artifacts using Binding/build.sh with Emscripten available.
