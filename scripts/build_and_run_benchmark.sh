rm -rf build \
&& cmake -B build -DCMAKE_BUILD_TYPE=Release . >/dev/null 2>&1 \
&& cmake --build build -j >/dev/null 2>&1 \
&& ./build/orderbook_benchmark --benchmark_repetitions=10 --benchmark_report_aggregates_only=true \