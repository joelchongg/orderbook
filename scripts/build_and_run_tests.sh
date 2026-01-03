rm -rf build \
&& cmake -B build -DCMAKE_BUILD_TYPE=Release . >/dev/null 2>&1 \
&& cmake --build build -j >/dev/null 2>&1 \
&& ctest --test-dir build --output-on-failure