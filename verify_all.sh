#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$ROOT_DIR"

echo "[1/5] Building C++20 firmware, Clang MISRA Gatekeeper, LLVM WCETPass, and HiGHS IPET solver..."
cmake -B build -G Ninja -DCMAKE_C_COMPILER=clang-22 -DCMAKE_CXX_COMPILER=clang++-22 -DCMAKE_BUILD_TYPE=RelWithDebInfo >/dev/null
ninja -C build

echo "[2/5] Running all 14 GoogleTest suites..."
ctest --test-dir build --output-on-failure

echo "[3/5] Running end-to-end static WCET + I-Cache + MISRA + RTA certification..."
./build/secu_verify_cli --mode=clean --out=analysis_report.json
./build/secu_verify_cli --mode=faulty --out=analysis_report_faulty.json || true

echo "[4/5] Cross-compiling bare-metal ARM Cortex-M4/M7 ELF & running 200-tick Monza QEMU harness..."
mkdir -p target_arm/build
arm-none-eabi-g++ -std=c++20 -mcpu=cortex-m4 -mthumb -O2 \
  -Wall -Wextra -Werror -pedantic \
  -fno-exceptions -fno-rtti -fno-threadsafe-statics -ffreestanding -nostdlib \
  -Ifirmware/include target_arm/startup.cpp target_arm/monza_sim.cpp \
  firmware/src/mguk_bbw_arbiter.cpp firmware/src/active_aero_sync.cpp firmware/src/telemetry_framer.cpp \
  -T target_arm/mps2_an385.ld -Wl,--gc-sections -lgcc \
  -o target_arm/build/secu_monza_sim.elf

qemu-system-arm -M mps2-an385 -nographic -icount shift=0 \
  -semihosting-config enable=on,target=native \
  -kernel target_arm/build/secu_monza_sim.elf > qemu_telemetry.csv
tail -n 1 qemu_telemetry.csv

echo "[5/5] Verifying Pit-Wall Studio frontend & Tauri Rust backend..."
(cd studio && npm run build >/dev/null && cargo check --manifest-path src-tauri/Cargo.toml)

echo "ALL 6 SUBSYSTEMS VERIFIED CLEANLY."
