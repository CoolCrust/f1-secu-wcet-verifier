import React, { useMemo, useState } from "react";
import { RtaTask } from "../types";

interface Props {
  tasks: RtaTask[];
  totalUtilPercent: number;
  rmBoundPercent: number;
  cpuFreqMhz: number;
}

interface IterStep {
  iter: number;
  rPrev: number;
  rNext: number;
  preemptionDetail: string;
}

interface ComputedRtaRow {
  task: RtaTask;
  scaledWcetUs: number;
  scaledWcrtUs: number;
  interferenceUs: number;
  slackUs: number;
  utilPct: number;
  releasesPerHyperperiod: number;
  schedulable: boolean;
  steps: IterStep[];
}

const TASK_PLAIN: Record<string, { title: string; subtitle: string }> = {
  TASK_1_MGUK: {
    title: "Braking & Battery Recovery (1 kHz)",
    subtitle: "8-tap Q16.16 FIR filter & 350 kW MGU-K torque arbiter · NVIC Prio 1"
  },
  TASK_2_AERO: {
    title: "Active Wing & Flap Guard (500 Hz)",
    subtitle: "Front/rear flap synchronization & 2.0° asymmetry interlock · NVIC Prio 2"
  },
  TASK_3_TELEM: {
    title: "FIA Telemetry & CRC-16 Framer (100 Hz)",
    subtitle: "24-byte packet serializer & lock-free ring buffer · NVIC Prio 3"
  },
  TASK_4_CAN_POLL: {
    title: "Unbounded CAN Mailbox Wait (Injected Fault)",
    subtitle: "Data-dependent while loop with no static iteration ceiling · NVIC Prio 4"
  }
};

export const DeadlineChart: React.FC<Props> = ({
  tasks,
  rmBoundPercent,
  cpuFreqMhz
}) => {
  const [viewMode, setViewMode] = useState<"presentation" | "inspector">("presentation");
  const [simClockMhz, setSimClockMhz] = useState<number>(cpuFreqMhz || 400);
  const [contentionMult, setContentionMult] = useState<number>(1);
  const [selectedTaskId, setSelectedTaskId] = useState<string>(tasks[2]?.task_id ?? tasks[0]?.task_id ?? "");

  const hyperperiodUs = useMemo(() => Math.max(10000, ...tasks.map((t) => t.period_us)), [tasks]);

  const analysis = useMemo(() => {
    const ordered = [...tasks].sort((a, b) => a.priority - b.priority);
    const rows: ComputedRtaRow[] = [];
    let totalUtil = 0;
    let allPass = ordered.length > 0;

    for (let i = 0; i < ordered.length; i++) {
      const t = ordered[i];
      if (!t.schedulable || t.wcet_cycles === 0) {
        allPass = false;
        rows.push({
          task: t,
          scaledWcetUs: 0,
          scaledWcrtUs: Infinity,
          interferenceUs: Infinity,
          slackUs: -Infinity,
          utilPct: 0,
          releasesPerHyperperiod: Math.round(hyperperiodUs / Math.max(1, t.period_us)),
          schedulable: false,
          steps: []
        });
        continue;
      }

      const cUs = (t.wcet_cycles / simClockMhz) * contentionMult;
      const utilPct = (cUs / t.period_us) * 100;
      totalUtil += utilPct;

      const steps: IterStep[] = [];
      let rPrev = cUs;
      let converged = false;

      for (let k = 0; k < 64; k++) {
        let interference = 0;
        const parts: string[] = [];
        for (let hp = 0; hp < i; hp++) {
          const higher = rows[hp];
          if (!higher.schedulable) continue;
          const nPreempt = Math.ceil(rPrev / higher.task.period_us);
          interference += nPreempt * higher.scaledWcetUs;
          parts.push(`⌈${rPrev.toFixed(1)}/${higher.task.period_us.toFixed(0)}⌉×${higher.scaledWcetUs.toFixed(2)}`);
        }
        const rNext = cUs + interference;
        steps.push({
          iter: k + 1,
          rPrev,
          rNext,
          preemptionDetail: parts.length > 0 ? parts.join(" + ") : "0 (Highest priority)"
        });
        if (rNext > t.deadline_us) {
          rPrev = rNext;
          converged = false;
          break;
        }
        if (Math.abs(rNext - rPrev) < 1e-6) {
          rPrev = rNext;
          converged = true;
          break;
        }
        rPrev = rNext;
      }

      const ok = converged && rPrev <= t.deadline_us;
      if (!ok) allPass = false;
      rows.push({
        task: t,
        scaledWcetUs: cUs,
        scaledWcrtUs: rPrev,
        interferenceUs: Math.max(0, rPrev - cUs),
        slackUs: t.deadline_us - rPrev,
        utilPct,
        releasesPerHyperperiod: Math.round(hyperperiodUs / Math.max(1, t.period_us)),
        schedulable: ok,
        steps
      });
    }
    return { rows, totalUtil, allPass };
  }, [tasks, simClockMhz, contentionMult, hyperperiodUs]);

  const ganttData = useMemo(() => {
    const valid = analysis.rows.filter((r) => r.schedulable);
    const totalBusy = valid.reduce((acc, r) => acc + r.scaledWcetUs, 0);
    const windowUs = Math.max(40, Math.ceil(totalBusy * 1.35));
    let cursor = 0;
    const slices = valid.map((r, idx) => {
      const start = cursor;
      const end = cursor + r.scaledWcetUs;
      cursor = end;
      return {
        id: r.task.task_id,
        start,
        end,
        duration: r.scaledWcetUs,
        shade: idx === 0 ? "#171717" : idx === 1 ? "#4d4d4d" : "#297a3a"
      };
    });
    return { slices, totalBusy, windowUs };
  }, [analysis.rows]);

  const activeRow =
    analysis.rows.find((r) => r.task.task_id === selectedTaskId) ??
    analysis.rows[analysis.rows.length - 1];

  return (
    <div style={{ display: "flex", flexDirection: "column", gap: "20px" }}>
      {/* Header Row with Presentation / Inspector Segmented Control */}
      <div style={{ display: "flex", justifyContent: "space-between", alignItems: "flex-end", flexWrap: "wrap", gap: "12px" }}>
        <div>
          <span className="eyebrow">RATE MONOTONIC RESPONSE-TIME ANALYSIS</span>
          <h2 className="vercel-h2">Hard real-time deadline proofs</h2>
        </div>

        <div style={{ display: "flex", alignItems: "center", gap: "12px" }}>
          <span className={`v-badge ${analysis.allPass ? "v-badge-green" : "v-badge-red"}`}>
            {analysis.allPass ? "✓ ALL DEADLINES MET" : "✕ DEADLINE VIOLATION"}
          </span>
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
              Deep Inspector
            </button>
          </div>
        </div>
      </div>

      {/* 3 Clean Summary Cards */}
      <div className="grid-3">
        <div className="v-card">
          <span className="eyebrow">WORST-CASE CPU UTILIZATION</span>
          <div className="mono" style={{ fontSize: "26px", fontWeight: 500, margin: "6px 0 4px" }}>
            {analysis.totalUtil.toFixed(2)}%
          </div>
          <div style={{ fontSize: "13px", color: "var(--color-stone)" }}>
            Theoretical Liu-Layland bound: {rmBoundPercent.toFixed(1)}%
          </div>
        </div>

        <div className="v-card">
          <span className="eyebrow">CRITICAL INSTANT BUSY WINDOW</span>
          <div className="mono" style={{ fontSize: "26px", fontWeight: 500, margin: "6px 0 4px" }}>
            {ganttData.totalBusy.toFixed(2)} µs
          </div>
          <div style={{ fontSize: "13px", color: "var(--color-stone)" }}>
            Out of 1,000 µs fastest control period
          </div>
        </div>

        <div className="v-card">
          <span className="eyebrow">STRESS TEST PARAMETERS</span>
          <div className="mono" style={{ fontSize: "20px", fontWeight: 500, margin: "6px 0 4px" }}>
            {simClockMhz} MHz · {contentionMult}× load
          </div>
          <div style={{ fontSize: "13px", color: "var(--color-stone)" }}>
            Adjust sliders below to test clock scaling
          </div>
        </div>
      </div>

      {/* Interactive Clock & Contention Sliders */}
      <div className="v-card" style={{ display: "grid", gridTemplateColumns: "1fr 1fr auto", gap: "24px", alignItems: "center" }}>
        <div>
          <div style={{ display: "flex", justifyContent: "space-between", fontSize: "13px", marginBottom: "6px" }}>
            <span style={{ color: "var(--color-charcoal)" }}>Target Cortex-M7 Clock Frequency</span>
            <strong className="mono">{simClockMhz} MHz</strong>
          </div>
          <input
            type="range"
            min={16}
            max={480}
            step={8}
            value={simClockMhz}
            onChange={(e) => setSimClockMhz(Number(e.target.value))}
            style={{ width: "100%", accentColor: "#171717" }}
          />
        </div>

        <div>
          <div style={{ display: "flex", justifyContent: "space-between", fontSize: "13px", marginBottom: "6px" }}>
            <span style={{ color: "var(--color-charcoal)" }}>Bus / Interrupt Contention Multiplier</span>
            <strong className="mono">{contentionMult}×</strong>
          </div>
          <input
            type="range"
            min={1}
            max={40}
            step={1}
            value={contentionMult}
            onChange={(e) => setContentionMult(Number(e.target.value))}
            style={{ width: "100%", accentColor: "#171717" }}
          />
        </div>

        <button
          className="btn-ghost btn-sm"
          onClick={() => {
            setSimClockMhz(cpuFreqMhz || 400);
            setContentionMult(1);
          }}
        >
          Reset 400 MHz
        </button>
      </div>

      {viewMode === "presentation" ? (
        <div className="grid-2">
          {/* Clean Visual Deadline Margin Bars */}
          <div className="v-card">
            <span className="eyebrow">DEADLINE HEADROOM BY TASK</span>
            <h3 className="vercel-card-title" style={{ marginTop: "4px", marginBottom: "16px" }}>
              Worst-case response time vs. period budget
            </h3>

            <div style={{ display: "flex", flexDirection: "column", gap: "18px" }}>
              {analysis.rows.map((r) => {
                const info = TASK_PLAIN[r.task.task_id] ?? { title: r.task.task_name, subtitle: r.task.demangled_function };
                const usedPct = r.schedulable ? Math.max(2, (r.scaledWcrtUs / r.task.deadline_us) * 100) : 100;
                return (
                  <div key={r.task.task_id}>
                    <div style={{ display: "flex", justifyContent: "space-between", marginBottom: "2px" }}>
                      <span style={{ fontWeight: 500, fontSize: "14px" }}>{info.title}</span>
                      <span className="mono" style={{ fontSize: "12.5px", color: r.schedulable ? "var(--color-terminal-green)" : "var(--color-alert-red)" }}>
                        {r.schedulable ? `${r.scaledWcrtUs.toFixed(2)} / ${r.task.deadline_us.toFixed(0)} µs` : "UNBOUNDED"}
                      </span>
                    </div>
                    <div style={{ fontSize: "12.5px", color: "var(--color-stone)", marginBottom: "8px" }}>{info.subtitle}</div>
                    <div style={{ width: "100%", height: "8px", background: "#ebebeb", borderRadius: "9999px", overflow: "hidden" }}>
                      <div
                        style={{
                          width: `${Math.min(100, usedPct)}%`,
                          height: "100%",
                          background: r.schedulable ? "#171717" : "#c92a2a",
                          borderRadius: "9999px"
                        }}
                      />
                    </div>
                  </div>
                );
              })}
            </div>
          </div>

          {/* Clean Critical-Instant Timeline */}
          <div className="v-card" style={{ display: "flex", flexDirection: "column", justifyContent: "space-between" }}>
            <div>
              <span className="eyebrow">SIMULTANEOUS RELEASE (t = 0 µs)</span>
              <h3 className="vercel-card-title" style={{ marginTop: "4px", marginBottom: "6px" }}>
                Critical instant preemption timeline
              </h3>
              <p style={{ fontSize: "13px", color: "var(--color-stone)", margin: "0 0 18px" }}>
                Shows exact sequential execution when all periodic interrupts trigger simultaneously at t = 0 µs.
              </p>

              <svg width="100%" viewBox="0 0 520 145" style={{ display: "block" }}>
                {[0, 0.25, 0.5, 0.75, 1].map((f, i) => {
                  const x = 12 + f * 496;
                  return (
                    <g key={i}>
                      <line x1={x} y1="8" x2={x} y2="118" stroke="#ebebeb" strokeWidth="1" />
                      <text x={x} y="136" textAnchor="middle" fill="#8f8f8f" fontSize="10" fontFamily="monospace">
                        {(f * ganttData.windowUs).toFixed(1)} µs
                      </text>
                    </g>
                  );
                })}
                {ganttData.slices.map((sl, idx) => {
                  const x = 12 + (sl.start / ganttData.windowUs) * 496;
                  const w = Math.max(5, (sl.duration / ganttData.windowUs) * 496);
                  const y = 16 + idx * 32;
                  return (
                    <g key={sl.id}>
                      <rect x={x} y={y} width={w} height="20" rx="3" fill={sl.shade} />
                      <text x={Math.min(395, x + w + 8)} y={y + 14} fill="#171717" fontSize="11" fontFamily="monospace">
                        {sl.id} ({sl.duration.toFixed(2)} µs)
                      </text>
                    </g>
                  );
                })}
              </svg>
            </div>

            <div className="v-cli" style={{ marginTop: "12px" }}>
              <span className="vercel-triangle">▲</span> rta_certifier --hyperperiod=10ms<br />
              <span style={{ color: "var(--color-terminal-green)" }}>✓</span> Busy window closes at{" "}
              <strong>{ganttData.totalBusy.toFixed(2)} µs</strong> ({Math.max(0, 1000 - ganttData.totalBusy).toFixed(2)} µs idle slack before next 1 kHz tick)
            </div>
          </div>
        </div>
      ) : (
        /* Deep Inspector Mode: Full 10-Column Matrix + Fixed-Point Iteration Proof */
        <div style={{ display: "flex", flexDirection: "column", gap: "16px" }}>
          <div className="v-card" style={{ overflowX: "auto" }}>
            <div style={{ display: "flex", justifyContent: "space-between", marginBottom: "12px" }}>
              <span className="eyebrow">COMPLETE RATE MONOTONIC RTA TABLE (CLICK ROW TO INSPECT RECURRENCE)</span>
              <span className="eyebrow">H = {(hyperperiodUs / 1000).toFixed(1)} MS</span>
            </div>
            <table className="v-table">
              <thead>
                <tr>
                  <th>Prio</th>
                  <th>Task ID</th>
                  <th>WCET (c)</th>
                  <th>Exec C_i</th>
                  <th>Period T_i</th>
                  <th>Preempt I_i</th>
                  <th>WCRT R_i</th>
                  <th>Slack</th>
                  <th>Util U_i</th>
                  <th>Status</th>
                </tr>
              </thead>
              <tbody>
                {analysis.rows.map((r) => (
                  <tr
                    key={r.task.task_id}
                    onClick={() => setSelectedTaskId(r.task.task_id)}
                    className={activeRow?.task.task_id === r.task.task_id ? "row-selected" : ""}
                    style={{ cursor: "pointer" }}
                  >
                    <td>P{r.task.priority}</td>
                    <td>{r.task.task_id}</td>
                    <td>{r.task.wcet_cycles > 0 ? `${r.task.wcet_cycles}c` : "∞"}</td>
                    <td>{r.schedulable ? `${r.scaledWcetUs.toFixed(3)} µs` : "∞"}</td>
                    <td>{r.task.period_us.toFixed(0)} µs</td>
                    <td>{r.schedulable ? `+${r.interferenceUs.toFixed(3)} µs` : "N/A"}</td>
                    <td>{r.schedulable ? `${r.scaledWcrtUs.toFixed(3)} µs` : "FAIL"}</td>
                    <td>{r.schedulable ? `${r.slackUs.toFixed(2)} µs` : "−∞"}</td>
                    <td>{r.utilPct.toFixed(3)}%</td>
                    <td style={{ color: r.schedulable ? "var(--color-terminal-green)" : "var(--color-alert-red)" }}>
                      {r.schedulable ? "✓ PROVEN" : "✕ FAIL"}
                    </td>
                  </tr>
                ))}
              </tbody>
            </table>
          </div>

          <div className="v-card">
            <span className="eyebrow">JOSEPH &amp; PANDYA FIXED-POINT ITERATION TRACE — {activeRow?.task.task_id}</span>
            {activeRow && activeRow.steps.length > 0 ? (
              <table className="v-table" style={{ marginTop: "10px" }}>
                <thead>
                  <tr>
                    <th>Iteration k</th>
                    <th>R_i^(k) (µs)</th>
                    <th>Higher-Priority Preemption Expansion</th>
                    <th>R_i^(k+1) (µs)</th>
                  </tr>
                </thead>
                <tbody>
                  {activeRow.steps.map((st) => (
                    <tr key={st.iter}>
                      <td>k = {st.iter}</td>
                      <td>{st.rPrev.toFixed(3)}</td>
                      <td>{st.preemptionDetail}</td>
                      <td style={{ color: st.rNext <= activeRow.task.deadline_us ? "var(--color-terminal-green)" : "var(--color-alert-red)" }}>
                        {st.rNext.toFixed(3)} {Math.abs(st.rNext - st.rPrev) < 1e-6 ? "✓ Converged" : ""}
                      </td>
                    </tr>
                  ))}
                </tbody>
              </table>
            ) : (
              <div className="v-cli" style={{ marginTop: "10px", color: "var(--color-alert-red)" }}>
                ✕ Cannot compute recurrence: C_i = ∞ (Unbounded loop detected by LLVM ScalarEvolution).
              </div>
            )}
          </div>
        </div>
      )}
    </div>
  );
};
