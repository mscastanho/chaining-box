#!/usr/bin/env bash

echo "clang: $(clang --version)"
echo "llvm: $(llvm-objdump --version)"
echo "bpftool: $(bpftool --version)"
echo "perf: $(perf --version)"
echo "iproute2: $(ip -V)"
echo "golang: $(go version)"
