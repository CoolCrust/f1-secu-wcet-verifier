# SECU Static WCET, L1 Cache & MISRA C++:2023 Verifier

A static timing analysis toolchain, compile-time safety gatekeeper, and hardware-in-the-loop (HIL) verification workbench for deterministic C++20 embedded control firmware targeting ARM Cortex-M4/M7 microcontrollers.

In hard real-time automotive and aerospace electronic control units (ECUs), functional correctness is insufficient if a control loop misses its deadline. Dynamic testing on sample inputs can miss rare worst-case branch combinations, cold instruction-cache evictions, or interrupt preemptions. This project proves upper execution-time bounds mathematically from the compiler IR before firmware is flashed to silicon, then cross-checks those bounds against bare-metal ARM execution.

---

## System Architecture

The verification pipeline runs in five deterministic stages:

```text
C++20 Control Firmware (mguk_bbw_arbiter.cpp, active_aero_sync.cpp, telemetry_framer.cpp)
       |
       +---> [1] Clang 22 AST Gatekeeper (MISRA C++:2023 & Bounded Loop Enforcement)
       |
       +---> [2] LLVM 22 IR Pass (CFG Extraction, SCEV Loop Bounds, Must/May L1 I-Cache)
       |
       +---> [3] HiGHS MILP Solver (Implicit Path Enumeration + Mutex Path Pruning)
       |
       +---> [4] Rate Monotonic RTA (Fixed-Point Preemptive Schedulability Proof)
       |
       +---> [5] Bare-Metal ARM Cortex-M4/M7 ELF (QEMU & STM32H750/F411 DWT->CYCCNT HIL)
```

### 1. Clang 22 AST Safety Gatekeeper (`gatekeeper/`)
Implemented with Clang LibTooling (`RecursiveASTVisitor`), the gatekeeper inspects the C++20 Abstract Syntax Tree before LLVM code generation and rejects constructs that destroy timing determinism:
- **Dynamic Heap Allocation (`MISRA-CPP-21.6.1`):** Blocks `new`, `delete`, and heap-backed STL containers (`std::vector`, `std::string`), enforcing fixed-capacity `std::array` and lock-free ring buffers.
- **Unbounded Loops (`SECU-WCET-001`):** Rejects `while`, `do-while`, and `for` loops unless the loop bound can be resolved statically or carries an explicit `[[clang::annotate("wcet_max_iter=N")]]` upper bound.
- **Control Flow & Exception Determinism (`MISRA-CPP-9.6.1`, `MISRA-CPP-18.1.1`):** Forbids `goto`, recursion, and C++ `throw`/`try`/`catch` blocks.
- **Memory & Type Safety (`MISRA-CPP-11.6.1`, `MISRA-CPP-8.0.1`):** Flags uninitialized local variables and implicit floating-point to integer narrowing conversions.

### 2. LLVM 22 Loop & L1 Instruction Cache Pass (`passes/wcet_pass.cpp`)
Runs as a custom New Pass Manager module pass over canonicalized LLVM IR (`mem2reg`, `inline`, `loop-simplify`, `lcssa`):
- **Loop Trip-Count Resolution:** Queries LLVM `ScalarEvolution` (SCEV) for exact back-edge taken counts (such as the 8-tap Q16.16 FIR filter and 24-byte CRC-16 loop) and falls back to Clang loop annotations when bounds depend on bounded sensor ranges.
- **Must/May Abstract Cache Interpretation:** Models a 4 KB, 2-way set-associative L1 instruction cache with 32-byte cache lines and a +6 cycle Flash wait-state miss penalty. Fixed-point dataflow equations over the Control Flow Graph classify every basic block into one of three states:
  - **Always Hit (`AH`):** Guaranteed cache hit on every execution (0 Flash wait-state penalty).
  - **First Miss (`FM`):** Cold fetch on initial loop entry, then guaranteed resident on subsequent iterations.
  - **Always Miss (`AM`):** Conservative Flash fetch penalty charged on every execution.

### 3. Implicit Path Enumeration via HiGHS MILP (`solver/ipet_solver.cpp`)
Rather than enumerating an exponential number of execution paths, the solver encodes the CFG into a Mixed-Integer Linear Program solved by **HiGHS**:

$$\text{Maximize} \quad \sum_{i \in V} c_{i,\text{steady}} \cdot x_i + \sum_{i \in V_{\text{FM}}} (c_{i,\text{first}} - c_{i,\text{steady}}) \cdot z_i$$

Subject to:
- **Kirchhoff Flow Conservation:** $\sum_{e \in \text{out}(v)} f_e - \sum_{e \in \text{in}(v)} f_e = 0$ for every internal basic block $v$, with unit flow entering `entry` and leaving `exit`.
- **Loop Back-Edge Bounds:** $\sum_{e \in \text{back}(L)} f_e \le B_L \cdot \sum_{e \in \text{entry}(L)} f_e$ for each loop $L$ with proven bound $B_L$.
- **First-Miss Linearization:** $z_i \le x_i$, $z_i \le \sum_{e \in \text{entry}(L)} f_e$, and $z_i \in \{0, 1\}$.
- **Infeasible-Path Mutual Exclusion:** $\sum_{t \in G} x_t \le 1$ for mutually exclusive operating regimes (such as wet-weather flap lock vs. straight-line low-drag mode), tightening the WCET bound without sacrificing soundness.

### 4. Rate Monotonic Response-Time Analysis (`rta/rta_analyzer.cpp`)
Evaluates preemptive fixed-priority schedulability at a 400 MHz reference clock using the Liu-Layland utilization test and Joseph & Pandya's exact fixed-point recurrence:

$$R_i^{(k+1)} = C_i + \sum_{j \in hp(i)} \left\lceil \frac{R_i^{(k)}}{T_j} \right\rceil C_j \le D_i$$

| Priority | Task ID | Rate | Period $T_i$ | Proven WCET | Exec $C_i$ (@ 400 MHz) | WCRT $R_i$ | CPU Util |
| :--- | :--- | :--- | :--- | :--- | :--- | :--- | :--- |
| **P1** | `TASK_1_MGUK` (350 kW MGU-K & Brake-by-Wire Arbiter) | 1 kHz | 1000 us | 3,370 cycles | 8.43 us | 8.43 us | 0.843% |
| **P2** | `TASK_2_AERO` (Active Aero Sync & Asymmetry Guard) | 500 Hz | 2000 us | 1,342 cycles | 3.35 us | 11.78 us | 0.168% |
| **P3** | `TASK_3_TELEM` (24-Byte Telemetry & CRC-16 Framer) | 100 Hz | 10000 us | 6,261 cycles | 15.65 us | 27.43 us | 0.157% |

Total worst-case processor utilization is **1.167%** (well within the **77.98%** 3-task Liu-Layland bound).

### 5. Bare-Metal ARM Execution & Workbench Studio (`target_arm/`, `target_stm32/`, `studio/`)
- **Bare-Metal Cross-Compilation:** Compiles the firmware with `arm-none-eabi-g++ -std=c++20 -mcpu=cortex-m4 -mthumb -O2 -ffreestanding -nostdlib` (4,812 bytes `.text`, 0 bytes `.data`, 0 bytes `.bss`) and runs a 200-tick stress trace on `qemu-system-arm -M mps2-an385 -icount shift=0`.
- **STM32 Hardware-in-the-Loop Harness:** Includes `target_stm32/stm32_hil_harness.cpp` using ARM CoreSight `DWT->CYCCNT` cycle counters over 115200-baud UART, paired with a virtual STM32H750VBT6 (400 MHz Cortex-M7) / STM32F411CEU6 (100 MHz Cortex-M4) emulator in the Tauri v2 desktop application.
- **Certification Evidence Export:** Generates tamper-evident SHA-256 signed JSON and printable HTML/PDF reports containing source file hashes, compiler flags, HiGHS ILP constraints, and measured vs. static bounds.

---

## Repository Layout

```text
f1-secu-wcet-verifier/
├── CMakeLists.txt                  # Top-level CMake build for Clang/LLVM passes, solver & tests
├── verify_all.sh                   # One-command full pipeline build & verification script
├── firmware/
│   ├── include/secu/               # Fixed-point Q16.16 math, ring buffer & WCET annotations
│   ├── src/                        # Deterministic C++20 control tasks (MGU-K, Aero, Telemetry)
│   └── faulty/                     # Intentional MISRA & unbounded-loop test cases
├── gatekeeper/                     # Clang 22 LibTooling AST safety checker
├── passes/                         # LLVM 22 NewPM CFG, SCEV & Must/May L1 I-Cache pass
├── solver/                         # HiGHS MILP IPET formulation & mutex path pruning
├── rta/                            # Rate Monotonic fixed-point response-time analyzer
├── target_arm/                     # Bare-metal startup, linker script & 200-tick QEMU harness
├── target_stm32/                   # Hardware DWT->CYCCNT UART harness for STM32H750 / F411
├── tests/                          # 14 GoogleTest suites covering AST, IR, ILP & RTA
├── tools/extract_disasm.py         # Extracts LLVM IR blocks & arm-none-eabi-objdump Thumb-2 asm
└── studio/                         # Tauri v2 (Rust) + React/TypeScript desktop workbench
```

---

## Prerequisites (Ubuntu 24.04 / WSL2)

- **Host Compiler & LLVM:** `clang-22`, `clang++-22`, `llvm-22-dev`, `libclang-22-dev`, `lld-22`
- **Build & Solvers:** `cmake` (>= 3.24), `ninja-build`, `libhighs-dev` (or FetchContent HiGHS), `libgtest-dev`
- **Cross-Compilation & Emulation:** `gcc-arm-none-eabi`, `binutils-arm-none-eabi`, `qemu-system-arm`
- **Desktop Workbench:** `rustc` / `cargo`, `nodejs` (>= 20), `npm`, `libwebkit2gtk-4.1-dev`

---

## Building & Running

### 1. Build Static Analyzer, Passes & Run All 14 Test Suites
```bash
cmake -B build -G Ninja \
  -DCMAKE_C_COMPILER=clang-22 \
  -DCMAKE_CXX_COMPILER=clang++-22 \
  -DCMAKE_BUILD_TYPE=RelWithDebInfo
ninja -C build
ctest --test-dir build --output-on-failure
```

### 2. Run CLI Verification (Clean vs. Faulty Firmware)
```bash
./build/secu_verify_cli --mode=clean --out=analysis_report.json
./build/secu_verify_cli --mode=faulty --out=analysis_report_faulty.json
```

### 3. Cross-Compile Bare-Metal ARM ELF & Run QEMU Trace
```bash
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
```

### 4. Run Full Pipeline End-to-End
```bash
./verify_all.sh
```

### 5. Launch the Desktop Workbench (Linux / WSLg)
```bash
cd studio
npm install
WEBKIT_DISABLE_DMABUF_RENDERER=1 npm run tauri dev
```

Windows users can download the prebuilt standalone binary (`F1-SECU-WCET-Verifier.exe`) from the **Releases** page.
