pushd ".."
cmake --preset debug
cmake --build --preset debug
ctest --output-on-failure --interactive-debug-mode 0 --timeout 60 --preset debug
popd
