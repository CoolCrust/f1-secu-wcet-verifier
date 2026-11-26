import React, { useEffect, useState } from "react";
import { invoke } from "@tauri-apps/api/core";
import { AnalysisReport, ExportBundleResult, QemuTickSample } from "./types";
import { FirmwareMode, TrackId, WeatherMode } from "./data/tracks2026";
import { LivePitWallSim } from "./components/LivePitWallSim";
import { FullRaceSim } from "./components/FullRaceSim";
import { DeadlineChart } from "./components/DeadlineChart";
import { CodeHotspotChart } from "./components/CodeHotspotChart";
import { SimulationChart } from "./components/SimulationChart";

type ActiveTab = "overview" | "deadlines" | "hotspots" | "qemu" | "telemetry" | "race";

function parseQemuCsv(csv: string): QemuTickSample[] {
  const lines = csv.split(/\r?\n/);
  const out: QemuTickSample[] = [];
  for (const line of lines) {
    if (!line || line.startsWith("tick,") || line.startsWith("#")) continue;
    const cols = line.split(",");
    if (cols.length < 10) continue;
    out.push({
      tick: Number(cols[0]),
      segment: cols[1],
      task1_cycles: Number(cols[2]),
      task2_cycles: Number(cols[3]),
      task3_cycles: Number(cols[4]),
      total_tick_cycles: Number(cols[5]),
      mguk_power_kw: Number(cols[6]) / 1000,
      lap_harvest_kj: Number(cols[7]) / 1000,
      aero_mode: Number(cols[8]),
      fault_bits: Number(cols[9])
    });
  }
  return out;
}

const PLAIN_RULES: Record<string, string> = {
  "SECU-WCET-001": "Unbounded data-dependent loop; static WCET cannot be guaranteed.",
  "MISRA-CPP-21.6.1": "Dynamic heap allocation (new / delete / std::vector) forbidden in hard real-time ISRs.",
  "MISRA-CPP-11.6.1": "Uninitialized local variable feeds indeterminate stack bytes into active aero control.",
  "MISRA-CPP-9.6.1": "Unstructured goto jump breaks CFG reducibility and loop-bound analysis.",
  "MISRA-CPP-8.0.1": "Implicit floating-point to 16-bit integer narrowing risks silent overflow.",
  "MISRA-CPP-18.1.1": "C++ exception throw introduces unbounded stack-unwinding latency."
};

export const App: React.FC = () => {
  const [activeTab, setActiveTab] = useState<ActiveTab>("overview");
  const [firmwareMode, setFirmwareMode] = useState<FirmwareMode>("clean");
  const [selectedTrackId, setSelectedTrackId] = useState<TrackId>("monza");
  const [weather, setWeather] = useState<WeatherMode>("dry");
  const [report, setReport] = useState<AnalysisReport | null>(null);
  const [telemetry, setTelemetry] = useState<QemuTickSample[]>([]);
  const [loading, setLoading] = useState<boolean>(false);
  const [exporting, setExporting] = useState<boolean>(false);
  const [exportResult, setExportResult] = useState<ExportBundleResult | null>(null);
  const [question, setQuestion] = useState<string>("");
  const [answer, setAnswer] = useState<string>(
    "All 3 periodic tasks pass MISRA C++:2023 and meet 1 ms, 2 ms, and 10 ms deadlines."
  );

  const runAudit = async (targetMode: FirmwareMode) => {
    setLoading(true);
    try {
      const jsonStr = await invoke<string>("run_verification", { mode: targetMode });
      const parsed = JSON.parse(jsonStr) as AnalysisReport;
      setReport(parsed);
      setFirmwareMode(targetMode);
      setAnswer(
        parsed.overall_certified
          ? `Certified: 0 MISRA violations and ${parsed.rta_summary.total_utilization_percent.toFixed(2)}% worst-case CPU load.`
          : `Rejected: ${parsed.misra_gatekeeper.violation_count} safety violations detected; faulty behavior coupled to HIL & race tabs.`
      );
    } finally {
      setLoading(false);
    }
  };

  const handleExportEvidence = async () => {
    setExporting(true);
    try {
      const res = await invoke<ExportBundleResult>("export_certification_bundle", { mode: firmwareMode });
      setExportResult(res);
    } finally {
      setExporting(false);
    }
  };

  useEffect(() => {
    void runAudit("clean");
    invoke<string>("load_qemu_telemetry").then((csv) => setTelemetry(parseQemuCsv(csv)));
  }, []);

  const handleAsk = (e: React.FormEvent) => {
    e.preventDefault();
    if (!report) return;
    const q = question.toLowerCase();
    if (q.includes("bug") || q.includes("misra") || q.includes("fault")) {
      setAnswer(
        report.misra_gatekeeper.passed
          ? "Zero MISRA C++:2023 violations exist in the clean firmware build."
          : `${report.misra_gatekeeper.violation_count} violations caught (SECU-WCET-001, MISRA-21.6.1, 11.6.1, 9.6.1, 8.0.1, 18.1.1).`
      );
    } else if (q.includes("cpu") || q.includes("load") || q.includes("rta")) {
      setAnswer(
        `Worst-case utilization is ${report.rta_summary.total_utilization_percent.toFixed(3)}% at 400 MHz (Liu-Layland limit: ${report.rta_summary.liu_layland_bound_percent.toFixed(1)}%).`
      );
    } else {
      setAnswer(
        `Currently inspecting ${firmwareMode} firmware on ${selectedTrackId.toUpperCase()}. Switch tabs on the left rail for RTA, IR/ASM, STM32 HIL, or Race Sim.`
      );
    }
    setQuestion("");
  };

  const peakQemu = Math.max(0, ...telemetry.map((s) => s.total_tick_cycles));
  const staticCeiling = report ? Math.max(100, ...report.rta_tasks.map((t) => t.wcet_cycles)) : 6261;
  const cpuFreq = report?.cpu_freq_mhz ?? 400;

  const navItems: Array<{ id: ActiveTab; label: string; tag: string }> = [
    { id: "overview", label: "Overview & Verdict", tag: "01" },
    { id: "deadlines", label: "Real-Time Deadlines", tag: "02" },
    { id: "hotspots", label: "Cycles, IR & ARM ASM", tag: "03" },
    { id: "qemu", label: "STM32 HIL & QEMU", tag: "04" },
    { id: "telemetry", label: "Live Telemetry", tag: "05" },
    { id: "race", label: "Race Simulator", tag: "06" }
  ];

  return (
    <div className="vercel-shell">
      {/* Slim Left-Rail Workbench Navigation */}
      <aside className="vercel-sidebar">
        <div>
          <div className="vercel-wordmark">
            <span className="vercel-triangle">▲</span>
            <span>SECU Verifier</span>
          </div>
          <div className="eyebrow" style={{ marginTop: "4px", fontSize: "10px" }}>
            LLVM 22 · HIGHS · ARM-M7
          </div>

          <nav className="sidebar-nav">
            {navItems.map((item) => (
              <button
                key={item.id}
                className={`nav-item ${activeTab === item.id ? "nav-item-active" : ""}`}
                onClick={() => setActiveTab(item.id)}
              >
                <span>{item.label}</span>
                <span className="mono" style={{ fontSize: "11px", opacity: 0.65 }}>{item.tag}</span>
              </button>
            ))}
          </nav>
        </div>

        {/* Global Firmware Toggle & Export in Sidebar Footer */}
        <div style={{ display: "flex", flexDirection: "column", gap: "10px", paddingTop: "16px", borderTop: "1px solid var(--color-hairline)" }}>
          <span className="eyebrow">ACTIVE FIRMWARE TARGET</span>
          <button
            className={firmwareMode === "clean" ? "btn-primary btn-sm" : "btn-ghost btn-sm"}
            disabled={loading}
            onClick={() => void runAudit("clean")}
            style={{ width: "100%", textAlign: "left" }}
          >
            {firmwareMode === "clean" ? "✓ Clean Production" : "Switch to Clean"}
          </button>
          <button
            className={firmwareMode === "faulty" ? "btn-primary btn-sm" : "btn-ghost btn-sm"}
            disabled={loading}
            onClick={() => void runAudit("faulty")}
            style={{ width: "100%", textAlign: "left" }}
          >
            {firmwareMode === "faulty" ? "✕ Faulty Build Active" : "Inject Faulty Build"}
          </button>
        </div>
      </aside>

      {/* Main Content Canvas */}
      <main className="vercel-main">
        {activeTab === "overview" && report && (
          <div style={{ display: "flex", flexDirection: "column", gap: "24px" }}>
            {/* Asymmetric Vercel Hero */}
            <div style={{ display: "flex", justifyContent: "space-between", alignItems: "flex-end", flexWrap: "wrap", gap: "16px" }}>
              <div>
                <span className="eyebrow">STATIC WCET, I-CACHE &amp; MISRA C++:2023 VERIFICATION</span>
                <h1 className="vercel-hero-title">
                  Deterministic timing,<br />proven before deployment.
                </h1>
              </div>

              <div className="btn-row">
                <button className="btn-ghost" onClick={() => setActiveTab("deadlines")}>
                  Inspect RTA Proofs →
                </button>
                <button className="btn-primary" disabled={exporting} onClick={() => void handleExportEvidence()}>
                  {exporting ? "Signing SHA-256..." : "Export Evidence Bundle"}
                </button>
              </div>
            </div>

            {exportResult && (
              <div className="v-cli">
                <span className="vercel-triangle">▲</span> export_certification_bundle --mode={exportResult.mode}<br />
                <span style={{ color: "var(--color-terminal-green)" }}>✓</span> SHA-256: <strong>{exportResult.bundle_sha256}</strong><br />
                <span style={{ color: "var(--color-terminal-green)" }}>✓</span> Saved HTML/PDF &amp; JSON to Desktop: {exportResult.html_path}
              </div>
            )}

            {/* 4 Bordered KPI Cards */}
            <div className="grid-4">
              <div className="v-card">
                <span className="eyebrow">SAFETY GATEKEEPER</span>
                <div className="mono" style={{ fontSize: "24px", fontWeight: 500, margin: "6px 0 4px", color: report.misra_gatekeeper.passed ? "var(--color-terminal-green)" : "var(--color-alert-red)" }}>
                  {report.misra_gatekeeper.passed ? "0 Violations" : `${report.misra_gatekeeper.violation_count} Caught`}
                </div>
                <div style={{ fontSize: "12.5px", color: "var(--color-stone)" }}>Clang 22 AST Matcher</div>
              </div>

              <div className="v-card">
                <span className="eyebrow">WORST-CASE CPU</span>
                <div className="mono" style={{ fontSize: "24px", fontWeight: 500, margin: "6px 0 4px" }}>
                  {report.rta_summary.total_utilization_percent.toFixed(2)}%
                </div>
                <div style={{ fontSize: "12.5px", color: "var(--color-stone)" }}>
                  RM Bound: {report.rta_summary.liu_layland_bound_percent.toFixed(1)}%
                </div>
              </div>

              <div className="v-card">
                <span className="eyebrow">STATIC WCET CEILING</span>
                <div className="mono" style={{ fontSize: "24px", fontWeight: 500, margin: "6px 0 4px" }}>
                  {staticCeiling} cycles
                </div>
                <div style={{ fontSize: "12.5px", color: "var(--color-stone)" }}>HiGHS IPET MILP solver</div>
              </div>

              <div className="v-card">
                <span className="eyebrow">HARDWARE HIL PROOF</span>
                <div className="mono" style={{ fontSize: "24px", fontWeight: 500, margin: "6px 0 4px" }}>
                  {peakQemu}c peak
                </div>
                <div style={{ fontSize: "12.5px", color: "var(--color-stone)" }}>QEMU + STM32H750 Virtual</div>
              </div>
            </div>

            {/* Signature Vercel Inverted CLI Terminal Card */}
            <div className="v-card-inverted">
              <div style={{ display: "flex", justifyContent: "space-between", alignItems: "center", marginBottom: "12px" }}>
                <span className="mono" style={{ fontSize: "11px", letterSpacing: "0.071em", color: "#a8a8a8" }}>
                  ▲ SECU_VERIFY_CLI --MODE={firmwareMode.toUpperCase()}
                </span>
                <span className="mono" style={{ fontSize: "11px", color: report.overall_certified ? "#4ade80" : "#f87171" }}>
                  {report.overall_certified ? "✓ CERTIFIED" : "✕ REJECTED"}
                </span>
              </div>

              <div className="mono" style={{ fontSize: "13px", lineHeight: 1.65 }}>
                <div>
                  <span style={{ color: report.overall_certified ? "#4ade80" : "#f87171" }}>
                    {report.overall_certified ? "✓" : "✕"}
                  </span>{" "}
                  {report.overall_certified
                    ? "Zero MISRA C++:2023 violations. All loops bounded via LLVM ScalarEvolution. All deadlines proven."
                    : `Deployment blocked: ${report.misra_gatekeeper.violation_count} safety violations found across faulty translation units.`}
                </div>
                <div style={{ color: "#a8a8a8", marginTop: "4px" }}>→ {answer}</div>
              </div>

              <form onSubmit={handleAsk} style={{ display: "flex", gap: "8px", marginTop: "14px" }}>
                <input
                  value={question}
                  onChange={(e) => setQuestion(e.target.value)}
                  placeholder="Ask anything about deadlines, cache savings, or caught violations..."
                  style={{
                    flex: 1,
                    background: "#262626",
                    border: "1px solid #383838",
                    borderRadius: "6px",
                    padding: "8px 12px",
                    color: "#ffffff",
                    fontFamily: "var(--font-geist-sans)",
                    fontSize: "13px",
                    outline: "none"
                  }}
                />
                <button
                  type="submit"
                  style={{
                    background: "#ffffff",
                    color: "#171717",
                    border: "none",
                    borderRadius: "6px",
                    padding: "8px 14px",
                    fontSize: "13px",
                    fontWeight: 500,
                    cursor: "pointer"
                  }}
                >
                  Run
                </button>
              </form>
            </div>

            {/* Violations Table (Only appears when Faulty Firmware is injected) */}
            {report.misra_gatekeeper.violations.length > 0 && (
              <div className="v-card">
                <span className="eyebrow" style={{ marginBottom: "10px", color: "var(--color-alert-red)" }}>
                  CAUGHT SAFETY &amp; TIMING VIOLATIONS ({report.misra_gatekeeper.violation_count})
                </span>
                <table className="v-table">
                  <thead>
                    <tr>
                      <th>Rule ID</th>
                      <th>Category</th>
                      <th>Location</th>
                      <th>Plain-English Explanation</th>
                    </tr>
                  </thead>
                  <tbody>
                    {report.misra_gatekeeper.violations.map((v, i) => (
                      <tr key={i}>
                        <td style={{ color: "var(--color-alert-red)", fontWeight: 500 }}>{v.rule_id}</td>
                        <td>{v.category}</td>
                        <td>{v.file.split("/").pop()}:{v.line}</td>
                        <td style={{ fontFamily: "var(--font-geist-sans)" }}>{PLAIN_RULES[v.rule_id] ?? v.message}</td>
                      </tr>
                    ))}
                  </tbody>
                </table>
              </div>
            )}
          </div>
        )}

        {activeTab === "deadlines" && report && (
          <DeadlineChart
            tasks={report.rta_tasks}
            totalUtilPercent={report.rta_summary.total_utilization_percent}
            rmBoundPercent={report.rta_summary.liu_layland_bound_percent}
            cpuFreqMhz={cpuFreq}
          />
        )}

        {activeTab === "hotspots" && report && (
          <CodeHotspotChart functions={report.functions} cpuFreqMhz={cpuFreq} />
        )}

        {activeTab === "qemu" && report && (
          <SimulationChart
            samples={telemetry}
            rtaTasks={report.rta_tasks}
            cpuFreqMhz={cpuFreq}
            firmwareMode={firmwareMode}
          />
        )}

        {activeTab === "telemetry" && (
          <LivePitWallSim
            firmwareMode={firmwareMode}
            staticWcetCeiling={staticCeiling}
            selectedTrackId={selectedTrackId}
            onSelectTrack={setSelectedTrackId}
            weather={weather}
            onSelectWeather={setWeather}
          />
        )}

        {activeTab === "race" && (
          <FullRaceSim
            firmwareMode={firmwareMode}
            selectedTrackId={selectedTrackId}
            onSelectTrack={setSelectedTrackId}
            weather={weather}
            onSelectWeather={setWeather}
          />
        )}
      </main>
    </div>
  );
};
