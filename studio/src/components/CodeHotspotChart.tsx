import React, { useEffect, useMemo, useState } from "react";
import { invoke } from "@tauri-apps/api/core";
import { BasicBlockNode, DisassemblyBundle, FunctionAnalysis } from "../types";

interface Props {
  functions: FunctionAnalysis[];
  cpuFreqMhz: number;
}

export const CodeHotspotChart: React.FC<Props> = ({ functions, cpuFreqMhz }) => {
  const [viewMode, setViewMode] = useState<"presentation" | "inspector">("presentation");
  const [selectedIdx, setSelectedIdx] = useState<number>(0);
  const [selectedBb, setSelectedBb] = useState<BasicBlockNode | null>(null);
  const [disasm, setDisasm] = useState<DisassemblyBundle | null>(null);

  useEffect(() => {
    invoke<string>("load_disassembly_bundle")
      .then((raw) => setDisasm(JSON.parse(raw) as DisassemblyBundle))
      .catch(() => setDisasm(null));
  }, []);

  const fn = functions[selectedIdx] ?? functions[0];
  const activeBb = selectedBb ?? fn?.blocks.find((b) => b.on_wcep) ?? fn?.blocks[0] ?? null;

  const stats = useMemo(() => {
    if (!fn) {
      return { ah: 0, fm: 0, am: 0, baseCycles: 0, flashPenalty: 0, naiveAllMiss: 0, cacheSavings: 0 };
    }
    let ah = 0, fm = 0, am = 0, baseCycles = 0, flashPenalty = 0, naiveAllMiss = 0;
    for (const b of fn.blocks) {
      if (b.cache_class === "AH") ah++;
      else if (b.cache_class === "FM") fm++;
      else am++;

      if (b.exec_count > 0) {
        const base = b.base_cycles * b.exec_count;
        baseCycles += base;
        flashPenalty += Math.max(0, b.total_cycles_contributed - base);
        naiveAllMiss += (b.base_cycles + 6) * b.exec_count;
      }
    }
    return {
      ah, fm, am,
      baseCycles,
      flashPenalty,
      naiveAllMiss,
      cacheSavings: Math.max(0, naiveAllMiss - fn.wcet_cycles)
    };
  }, [fn]);

  const topHotspots = useMemo(() => {
    if (!fn) return [];
    return [...fn.blocks]
      .filter((b) => b.total_cycles_contributed > 0)
      .sort((a, b) => b.total_cycles_contributed - a.total_cycles_contributed)
      .slice(0, 6);
  }, [fn]);

  if (!fn) return null;

  const maxBlockCycles = Math.max(1, ...topHotspots.map((b) => b.total_cycles_contributed));
  const irBlocks = disasm?.ir_by_mangled?.[fn.mangled_name] ?? [];
  const activeIrBlock = activeBb ? irBlocks[activeBb.id] : null;
  const armKey = Object.keys(disasm?.arm_disasm ?? {}).find((k) => fn.demangled_name.includes(k));
  const armEntry = armKey ? disasm?.arm_disasm[armKey] : null;

  return (
    <div style={{ display: "flex", flexDirection: "column", gap: "20px" }}>
      {/* Header + View Switch */}
      <div style={{ display: "flex", justifyContent: "space-between", alignItems: "flex-end", flexWrap: "wrap", gap: "12px" }}>
        <div>
          <span className="eyebrow">LLVM 22 STATIC ANALYSIS &amp; HIGHS IPET SOLVER</span>
          <h2 className="vercel-h2">Worst-case execution cycles &amp; L1 cache</h2>
        </div>

        <div className="segmented">
          <button
            className={`seg-btn ${viewMode === "presentation" ? "seg-btn-active" : ""}`}
            onClick={() => setViewMode("presentation")}
          >
            Presentation
          </button>
          <button
            className={`seg-btn ${viewMode === "inspector" ? "seg-btn-active" : ""}`}
            onClick={() => setViewMode("inspector")}
          >
            Deep Inspector (IR &amp; ARM ASM)
          </button>
        </div>
      </div>

      {/* Function Selector Pills */}
      <div className="btn-row">
        {functions.map((f, idx) => (
          <button
            key={f.mangled_name + idx}
            className={idx === selectedIdx ? "btn-pill-dark" : "btn-pill-light"}
            onClick={() => {
              setSelectedIdx(idx);
              setSelectedBb(null);
            }}
          >
            {f.demangled_name.replace(/^secu::/, "").replace(/\(.*\)$/, "()")} ·{" "}
            {f.provable ? `${f.wcet_cycles}c` : "Unbounded"}
          </button>
        ))}
      </div>

      {/* 4 Crisp Summary Metrics */}
      <div className="grid-4">
        <div className="v-card">
          <span className="eyebrow">PROVEN WCET BOUND</span>
          <div className="mono" style={{ fontSize: "24px", fontWeight: 500, margin: "6px 0 4px" }}>
            {fn.provable ? `${fn.wcet_cycles} cycles` : "Unbounded"}
          </div>
          <div style={{ fontSize: "12.5px", color: "var(--color-stone)" }}>
            {(fn.wcet_cycles / (cpuFreqMhz || 400)).toFixed(2)} µs @ {cpuFreqMhz} MHz
          </div>
        </div>

        <div className="v-card">
          <span className="eyebrow">CACHE ANALYSIS SAVINGS</span>
          <div className="mono" style={{ fontSize: "24px", fontWeight: 500, color: "var(--color-terminal-green)", margin: "6px 0 4px" }}>
            −{stats.cacheSavings} cycles
          </div>
          <div style={{ fontSize: "12.5px", color: "var(--color-stone)" }}>
            Saved vs. naive all-miss ({stats.naiveAllMiss}c)
          </div>
        </div>

        <div className="v-card">
          <span className="eyebrow">MUTEX PATH PRUNING</span>
          <div className="mono" style={{ fontSize: "24px", fontWeight: 500, margin: "6px 0 4px" }}>
            −{fn.infeasible_path_savings_cycles} cycles
          </div>
          <div style={{ fontSize: "12.5px", color: "var(--color-stone)" }}>
            Infeasible branch combinations eliminated
          </div>
        </div>

        <div className="v-card">
          <span className="eyebrow">SCEV LOOP PROOFS</span>
          <div className="mono" style={{ fontSize: "24px", fontWeight: 500, margin: "6px 0 4px" }}>
            {fn.loops.length === 0
              ? "Loop-Free"
              : fn.loops.map((l) => (l.bounded ? `${l.max_iterations}×` : "∞")).join(", ")}
          </div>
          <div style={{ fontSize: "12.5px", color: "var(--color-stone)" }}>
            {fn.blocks.length} basic blocks · {fn.edges.length} CFG edges
          </div>
        </div>
      </div>

      {viewMode === "presentation" ? (
        <div className="grid-2">
          {/* Top Cycle Hotspots Bar Chart */}
          <div className="v-card">
            <span className="eyebrow">WORST-CASE EXECUTION PATH</span>
            <h3 className="vercel-card-title" style={{ marginTop: "4px", marginBottom: "16px" }}>
              Top cycle-consuming basic blocks
            </h3>

            {topHotspots.length === 0 ? (
              <div style={{ color: "var(--color-alert-red)", fontSize: "13px" }}>
                Unbounded loop — finite cycle distribution cannot be proven.
              </div>
            ) : (
              <div style={{ display: "flex", flexDirection: "column", gap: "14px" }}>
                {topHotspots.map((b) => {
                  const pct = Math.max(4, (b.total_cycles_contributed / maxBlockCycles) * 100);
                  return (
                    <div key={b.id}>
                      <div style={{ display: "flex", justifyContent: "space-between", fontSize: "13px", marginBottom: "4px" }}>
                        <span className="mono">
                          BB{b.id} ({b.label}) · <strong>{b.cache_class}</strong>
                        </span>
                        <span className="mono">
                          <strong>{b.total_cycles_contributed}c</strong> ({b.exec_count}×)
                        </span>
                      </div>
                      <div style={{ height: "8px", background: "#ebebeb", borderRadius: "9999px", overflow: "hidden" }}>
                        <div style={{ width: `${pct}%`, height: "100%", background: "#171717", borderRadius: "9999px" }} />
                      </div>
                    </div>
                  );
                })}
              </div>
            )}
          </div>

          {/* 4 KB L1 Instruction Cache Distribution */}
          <div className="v-card" style={{ display: "flex", flexDirection: "column", justifyContent: "space-between" }}>
            <div>
              <span className="eyebrow">4 KB L1 INSTRUCTION CACHE MODEL</span>
              <h3 className="vercel-card-title" style={{ marginTop: "4px", marginBottom: "8px" }}>
                Must/May abstract cache classification
              </h3>
              <p style={{ fontSize: "13px", color: "var(--color-stone)", margin: "0 0 18px" }}>
                Blocks proven resident in the 4 KB 2-way cache avoid the +6 cycle Flash memory wait-state penalty.
              </p>

              <div style={{ display: "flex", height: "12px", borderRadius: "9999px", overflow: "hidden", background: "#ebebeb", marginBottom: "16px" }}>
                <div style={{ width: `${(stats.ah / Math.max(1, fn.blocks.length)) * 100}%`, background: "#171717" }} />
                <div style={{ width: `${(stats.fm / Math.max(1, fn.blocks.length)) * 100}%`, background: "#7d7d7d" }} />
                <div style={{ width: `${(stats.am / Math.max(1, fn.blocks.length)) * 100}%`, background: "#c9c9c9" }} />
              </div>

              <div style={{ display: "flex", flexDirection: "column", gap: "10px", fontSize: "13px" }}>
                <div style={{ display: "flex", justifyContent: "space-between" }}>
                  <span>■ Always Hit (AH — 0 wait states)</span>
                  <strong className="mono">{stats.ah} blocks</strong>
                </div>
                <div style={{ display: "flex", justifyContent: "space-between" }}>
                  <span>■ First Miss (FM — cached after 1st loop entry)</span>
                  <strong className="mono">{stats.fm} blocks</strong>
                </div>
                <div style={{ display: "flex", justifyContent: "space-between" }}>
                  <span>■ Always Miss (AM — +6c Flash fetch)</span>
                  <strong className="mono">{stats.am} blocks</strong>
                </div>
              </div>
            </div>

            <div className="v-cli" style={{ marginTop: "16px" }}>
              <span className="vercel-triangle">▲</span> highs_ipet_solver --objective=max_cycles<br />
              <span style={{ color: "var(--color-terminal-green)" }}>✓</span> Base pipeline: <strong>{stats.baseCycles}c</strong> + Flash penalty: <strong>{stats.flashPenalty}c</strong> = <strong>{fn.wcet_cycles}c</strong>
            </div>
          </div>
        </div>
      ) : (
        /* Deep Inspector Mode: Basic Block Table + Side-by-Side LLVM IR & ARM Thumb-2 Disassembly */
        <div style={{ display: "flex", flexDirection: "column", gap: "16px" }}>
          <div className="v-card" style={{ maxHeight: "260px", overflowY: "auto" }}>
            <span className="eyebrow" style={{ marginBottom: "8px" }}>
              BASIC BLOCK COST TABLE — CLICK ANY ROW TO INSPECT EXACT LLVM IR
            </span>
            <table className="v-table">
              <thead>
                <tr>
                  <th>Block</th>
                  <th>IR Label</th>
                  <th>Cache</th>
                  <th>Base</th>
                  <th>1st / Steady</th>
                  <th>Exec Count</th>
                  <th>Total Cycles</th>
                  <th>Path</th>
                </tr>
              </thead>
              <tbody>
                {fn.blocks.map((b) => (
                  <tr
                    key={b.id}
                    onClick={() => setSelectedBb(b)}
                    className={activeBb?.id === b.id ? "row-selected" : ""}
                    style={{ cursor: "pointer" }}
                  >
                    <td>BB{b.id}</td>
                    <td>{b.label}</td>
                    <td>{b.cache_class}</td>
                    <td>{b.base_cycles}c</td>
                    <td>{b.first_iter_cycles}/{b.steady_iter_cycles}c</td>
                    <td>{b.exec_count}×</td>
                    <td>{b.total_cycles_contributed}c</td>
                    <td style={{ color: b.on_wcep ? "var(--color-terminal-green)" : "var(--color-graphite)" }}>
                      {b.on_wcep ? "HOT WCEP" : "—"}
                    </td>
                  </tr>
                ))}
              </tbody>
            </table>
          </div>

          <div className="grid-2">
            <div className="v-card">
              <div style={{ display: "flex", justifyContent: "space-between", marginBottom: "10px" }}>
                <span className="eyebrow">LLVM 22 IR // {activeBb ? `BB${activeBb.id} (${activeBb.label})` : "SELECT BLOCK"}</span>
                <span className="eyebrow">{activeIrBlock ? `${activeIrBlock.lines.length} INSTS` : ""}</span>
              </div>
              <pre className="v-cli-dark" style={{ margin: 0, maxHeight: "290px", overflow: "auto" }}>
                {activeIrBlock
                  ? `${activeIrBlock.label}:\n  ` + activeIrBlock.lines.join("\n  ")
                  : "; Select a basic block above to inspect canonicalized LLVM IR."}
              </pre>
            </div>

            <div className="v-card">
              <div style={{ display: "flex", justifyContent: "space-between", marginBottom: "10px" }}>
                <span className="eyebrow">ARMV7E-M THUMB-2 DISASSEMBLY (OBJDUMP)</span>
                <span className="eyebrow">{armEntry ? `${armEntry.address} · ${armEntry.instruction_count} INSTS` : ""}</span>
              </div>
              <pre className="v-cli-dark" style={{ margin: 0, maxHeight: "290px", overflow: "auto" }}>
                {armEntry
                  ? `<${armEntry.symbol}> @ ${armEntry.address}\n` + armEntry.lines.join("\n")
                  : "; Faulty translation units are excluded from bare-metal ARM linking."}
              </pre>
            </div>
          </div>
        </div>
      )}
    </div>
  );
};
