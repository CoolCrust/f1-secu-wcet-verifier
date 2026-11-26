#!/usr/bin/env python3
import hashlib
import json
import re
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
BUILD = ROOT / "build"
BUILD.mkdir(exist_ok=True)

SOURCES = [
    ROOT / "firmware/src/mguk_bbw_arbiter.cpp",
    ROOT / "firmware/src/active_aero_sync.cpp",
    ROOT / "firmware/src/telemetry_framer.cpp",
    ROOT / "firmware/faulty/faulty_unbounded_loop.cpp",
]
ELF_PATH = ROOT / "target_arm/build/secu_monza_sim.elf"


def sha256_file(path: Path) -> str:
    if not path.exists():
        return "MISSING"
    h = hashlib.sha256()
    h.update(path.read_bytes())
    return h.hexdigest()


def thumb_inst_cycles(mnemonic: str) -> int:
    m = mnemonic.lower().split(".")[0]
    if m.startswith(("ldr", "str", "ldm", "stm", "push", "pop")):
        return 2
    if m.startswith(("sdiv", "udiv")):
        return 4
    if m.startswith(("mul", "mla", "mls", "smull", "umull")):
        return 2
    if m.startswith(("b", "cbz", "cbnz", "it")):
        return 2
    return 1


def extract_ir_blocks(cpp_path: Path) -> dict:
    raw_ll = Path("/tmp/secu_disasm_raw.ll")
    opt_ll = Path("/tmp/secu_disasm_opt.ll")

    subprocess.run(
        [
            "clang++-22", "-std=c++20", "-O1", "-Xclang", "-disable-llvm-passes",
            "-fno-exceptions", "-fno-rtti", f"-I{ROOT / 'firmware/include'}",
            "-S", "-emit-llvm", str(cpp_path), "-o", str(raw_ll),
        ],
        check=True,
    )
    subprocess.run(
        [
            "opt-22", "-S",
            "-passes=module(function(mem2reg),cgscc(inline),function(mem2reg,loop-simplify,lcssa))",
            str(raw_ll), "-o", str(opt_ll),
        ],
        check=True,
    )

    text = opt_ll.read_text(errors="replace")
    fn_map = {}
    define_re = re.compile(r"^define\s+.*?@([^\s(]+)\(")
    label_re = re.compile(r"^([A-Za-z0-9_$.-]+):\s*(?:;.*)?$")

    lines = text.splitlines()
    i = 0
    while i < len(lines):
        m = define_re.match(lines[i])
        if not m:
            i += 1
            continue
        mangled = m.group(1).strip('"')
        i += 1
        blocks = []
        cur_label = "entry"
        cur_lines = []

        while i < len(lines) and lines[i] != "}":
            line = lines[i]
            lm = label_re.match(line)
            if lm:
                if cur_lines:
                    blocks.append({"id": len(blocks), "label": cur_label, "lines": cur_lines})
                cur_label = lm.group(1)
                cur_lines = []
            else:
                stripped = line.strip()
                if stripped and not stripped.startswith(";") and not stripped.startswith("#dbg_"):
                    cur_lines.append(stripped)
            i += 1

        if cur_lines:
            blocks.append({"id": len(blocks), "label": cur_label, "lines": cur_lines})
        if blocks:
            fn_map[mangled] = blocks
        i += 1

    return fn_map


def extract_arm_disasm(elf_path: Path) -> dict:
    if not elf_path.exists():
        return {}
    proc = subprocess.run(
        ["arm-none-eabi-objdump", "-d", "-C", str(elf_path)],
        capture_output=True,
        text=True,
        check=True,
    )
    hdr_re = re.compile(r"^([0-9a-fA-F]+)\s+<([^>]+)>:$")
    inst_re = re.compile(r"^\s*([0-9a-fA-F]+):\s+([0-9a-fA-F ]+?)\s+([a-zA-Z.]+.*)$")
    want = [
        "MgukBbwArbiter::step",
        "ActiveAeroController::step",
        "TelemetryFramer::pack_and_enqueue",
        "TelemetryFramer::compute_crc16_ccitt",
    ]
    out = {}
    cur_key = None
    cur_sym = ""
    cur_addr = ""
    cur_lines = []
    cur_cycles = 0

    def flush():
        if cur_key and cur_lines:
            out[cur_key] = {
                "symbol": cur_sym,
                "address": f"0x{cur_addr}",
                "instruction_count": len(cur_lines),
                "estimated_thumb_cycles": cur_cycles,
                "lines": cur_lines[:160],
            }

    for line in proc.stdout.splitlines():
        hm = hdr_re.match(line)
        if hm:
            flush()
            cur_key = None
            cur_lines = []
            cur_cycles = 0
            addr, sym = hm.group(1), hm.group(2)
            for w in want:
                if w in sym:
                    cur_key = w
                    cur_sym = sym
                    cur_addr = addr
                    break
        elif cur_key:
            im = inst_re.match(line)
            if im:
                i_addr, opcode, asm_txt = im.group(1), im.group(2).strip(), im.group(3).strip()
                mnemonic = asm_txt.split()[0]
                cyc = thumb_inst_cycles(mnemonic)
                cur_cycles += cyc
                cur_lines.append(f"0x{i_addr}:  {opcode:<11}  {asm_txt:<28} ; ~{cyc}c")
    flush()
    return out


def main():
    ir_by_mangled = {}
    for src in SOURCES:
        ir_by_mangled.update(extract_ir_blocks(src))

    arm_disasm = extract_arm_disasm(ELF_PATH)
    hashes = {str(p.relative_to(ROOT)): sha256_file(p) for p in SOURCES}
    hashes["target_arm/build/secu_monza_sim.elf"] = sha256_file(ELF_PATH)
    hashes["target_stm32/stm32_hil_harness.cpp"] = sha256_file(ROOT / "target_stm32/stm32_hil_harness.cpp")

    bundle = {
        "toolchain": {
            "host_compiler": "clang++-22 / LLVM 22",
            "host_flags": "-std=c++20 -O1 -Xclang -disable-llvm-passes -fno-exceptions -fno-rtti",
            "arm_compiler": "arm-none-eabi-g++ 14.2.1",
            "arm_flags": "-std=c++20 -mcpu=cortex-m4 -mthumb -O2 -ffreestanding -nostdlib -fno-exceptions -fno-rtti",
            "ilp_solver": "HiGHS 1.12.0 MILP (IPET + Must/May Cache + Mutex Pruning)",
            "hil_targets": "Virtual STM32H750VBT6 (400 MHz M7) / STM32F411CEU6 (100 MHz M4) + Physical USB-UART",
        },
        "artifact_hashes_sha256": hashes,
        "ir_by_mangled": ir_by_mangled,
        "arm_disasm": arm_disasm,
    }

    out_path = BUILD / "disassembly_bundle.json"
    out_path.write_text(json.dumps(bundle, indent=2))
    print(f"Wrote {out_path} ({len(ir_by_mangled)} IR fns, {len(arm_disasm)} Thumb-2 fns)")


if __name__ == "__main__":
    main()
