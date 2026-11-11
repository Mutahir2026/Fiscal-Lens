#!/usr/bin/env bash
set -e
cd "$(dirname "$0")"
SRC=../src
FLAGS="-O2 -std=c++17 -fexceptions -I$SRC"

em++ $FLAGS -Dmain=tree_main         -c $SRC/RegressionTree.cpp   -o RegressionTree.o
em++ $FLAGS -Dmain=gen_forecast_main -c $SRC/gen_forecast_col.cpp -o gen_forecast_col.o

em++ $FLAGS $SRC/arima.cpp $SRC/LevyAdvisor.cpp wasm_bindings.cpp \
  RegressionTree.o gen_forecast_col.o \
  -lembind -sMODULARIZE=1 -sEXPORT_NAME=createLevyModule \
  -sALLOW_MEMORY_GROWTH=1 -sFORCE_FILESYSTEM=1 \
  -sEXPORTED_RUNTIME_METHODS=FS \
  -o levy_wasm.js