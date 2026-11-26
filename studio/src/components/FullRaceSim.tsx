import React, { useEffect, useMemo, useState } from "react";
import { PIT_STRATEGIES, PitStrategyId, simulateFullRace } from "../data/raceEngine";
import { FirmwareMode, TRACKS_2026, TRACK_LIST, TrackId, WeatherMode, formatLapTime } from "../data/tracks2026";

interface Props {
  firmwareMode: FirmwareMode;
  selectedTrackId: TrackId;
  onSelectTrack: (id: TrackId) => void;
  weather: WeatherMode;
  onSelectWeather: (w: WeatherMode) => void;
}

export const FullRaceSim: React.FC<Props> = ({
  firmwareMode,
  selectedTrackId,
  onSelectTrack,
  weather,
  onSelectWeather
}) => {
  const [startPos, setStartPos] = useState<number>(10);
  const [strategy, setStrategy] = useState<PitStrategyId>("two_stop_sms");
  const [currentLapIdx, setCurrentLapIdx] = useState<number>(0);
  const [isRunning, setIsRunning] = useState<boolean>(true);
  const [simSpeed, setSimSpeed] = useState<number>(1);

  const track = TRACKS_2026[selectedTrackId];
  const race = useMemo(
    () => simulateFullRace(track, startPos, strategy, weather, firmwareMode),
    [track, startPos, strategy, weather, firmwareMode]
  );

  useEffect(() => {
    setCurrentLapIdx(0);
    setIsRunning(true);
  }, [selectedTrackId, startPos, strategy, weather, firmwareMode]);

  useEffect(() => {
    if (!isRunning) return;
    if (currentLapIdx >= race.snapshots.length - 1) {
      setIsRunning(false);
      return;
    }
    const t = window.setTimeout(() => {
      setCurrentLapIdx((p) => Math.min(race.snapshots.length - 1, p + 1));
    }, Math.max(35, Math.round(300 / simSpeed)));
    return () => window.clearTimeout(t);
  }, [isRunning, currentLapIdx, race.snapshots.length, simSpeed]);

  const snap = race.snapshots[Math.min(currentLapIdx, race.snapshots.length - 1)];
  const visibleEvents = race.events.filter((e) => e.lap <= snap.lap).slice(-6).reverse();

  const w = 520, h = 135;
  const posPoints = race.snapshots
    .slice(0, currentLapIdx + 1)
    .map((s, idx) => {
      const x = 12 + (idx / Math.max(1, race.snapshots.length - 1)) * (w - 24);
      const y = 12 + ((s.ourPos - 1) / 21) * (h - 24);
      return `${x.toFixed(1)},${y.toFixed(1)}`;
    })
    .join(" ");

  return (
    <div style={{ display: "flex", flexDirection: "column", gap: "20px" }}>
      <div style={{ display: "flex", justifyContent: "space-between", alignItems: "flex-end", flexWrap: "wrap", gap: "12px" }}>
        <div>
          <span className="eyebrow">22-CAR GRAND PRIX STRATEGY &amp; WEATHER SIMULATION</span>
          <h2 className="vercel-h2">{track.name} ({track.laps} Laps)</h2>
        </div>

        <div className="btn-row">
          <button className="btn-primary btn-sm" onClick={() => setIsRunning((r) => !r)}>
            {isRunning ? "Pause" : currentLapIdx >= race.snapshots.length - 1 ? "Finished" : "Resume"}
          </button>
          <button className="btn-ghost btn-sm" onClick={() => { setCurrentLapIdx(0); setIsRunning(true); }}>Restart</button>
          <button className="btn-ghost btn-sm" onClick={() => { setIsRunning(false); setCurrentLapIdx(race.snapshots.length - 1); }}>Skip to Flag</button>
          {[1, 4].map((spd) => (
            <button key={spd} className={simSpeed === spd ? "btn-pill-dark" : "btn-pill-light"} onClick={() => setSimSpeed(spd)}>
              {spd}×
            </button>
          ))}
        </div>
      </div>

      {/* Strategy, Track, Weather & Grid Selector Card */}
      <div className="v-card" style={{ display: "flex", flexDirection: "column", gap: "12px" }}>
        <div style={{ display: "flex", justifyContent: "space-between", flexWrap: "wrap", gap: "10px" }}>
          <div className="btn-row">
            {TRACK_LIST.map((t) => (
              <button key={t.id} className={t.id === selectedTrackId ? "btn-pill-dark" : "btn-pill-light"} onClick={() => onSelectTrack(t.id)}>
                {t.shortName}
              </button>
            ))}
          </div>
          <div className="btn-row">
            {(["dry", "changing", "heavy_rain"] as WeatherMode[]).map((wm) => (
              <button key={wm} className={weather === wm ? "btn-pill-dark" : "btn-pill-light"} onClick={() => onSelectWeather(wm)}>
                {wm === "dry" ? "Dry" : wm === "changing" ? "Shower" : "Heavy Rain"}
              </button>
            ))}
          </div>
        </div>

        <div style={{ display: "flex", justifyContent: "space-between", flexWrap: "wrap", gap: "12px", alignItems: "center" }}>
          <div className="btn-row">
            {PIT_STRATEGIES.map((st) => (
              <button key={st.id} className={strategy === st.id ? "btn-pill-dark" : "btn-pill-light"} onClick={() => setStrategy(st.id)}>
                {st.label}
              </button>
            ))}
          </div>
          <div style={{ display: "flex", alignItems: "center", gap: "10px", fontSize: "13px" }}>
            <span style={{ color: "var(--color-stone)" }}>Start Grid:</span>
            <strong className="mono">P{startPos}</strong>
            <input type="range" min={1} max={22} value={startPos} onChange={(e) => setStartPos(Number(e.target.value))} style={{ width: "120px", accentColor: "#171717" }} />
          </div>
        </div>
      </div>

      {/* 4 Clean Race Metrics */}
      <div className="grid-4">
        <div className="v-card">
          <span className="eyebrow">LAP / WETNESS</span>
          <div className="mono" style={{ fontSize: "22px", fontWeight: 500, margin: "6px 0 4px" }}>{snap.lap} / {track.laps}</div>
          <div style={{ fontSize: "12.5px", color: "var(--color-stone)" }}>Track wetness: {snap.wetnessPct}%</div>
        </div>
        <div className="v-card">
          <span className="eyebrow">RACE POSITION</span>
          <div className="mono" style={{ fontSize: "22px", fontWeight: 500, margin: "6px 0 4px" }}>P{snap.ourPos} / 22</div>
          <div style={{ fontSize: "12.5px", color: "var(--color-stone)" }}>Started P{startPos}</div>
        </div>
        <div className="v-card">
          <span className="eyebrow">TIRE &amp; STRATEGY</span>
          <div className="mono" style={{ fontSize: "22px", fontWeight: 500, margin: "6px 0 4px" }}>{snap.ourTire} ({snap.ourTireHealthPct}%)</div>
          <div style={{ fontSize: "12.5px", color: "var(--color-stone)" }}>{snap.ourPits} pit stop(s) · Last {formatLapTime(snap.ourLastLapSec)}</div>
        </div>
        <div className="v-card">
          <span className="eyebrow">SECU CONTROLLER</span>
          <div className="mono" style={{ fontSize: "22px", fontWeight: 500, color: firmwareMode === "clean" ? "var(--color-terminal-green)" : "var(--color-alert-red)", margin: "6px 0 4px" }}>
            {firmwareMode === "clean" ? "Nominal" : snap.ourStatus}
          </div>
          <div style={{ fontSize: "12.5px", color: "var(--color-stone)" }}>{race.totalFaults} firmware fault(s)</div>
        </div>
      </div>

      {/* Position Chart + CLI Radio Log vs 22-Car Table */}
      <div className="grid-2">
        <div style={{ display: "flex", flexDirection: "column", gap: "16px" }}>
          <div className="v-card">
            <span className="eyebrow" style={{ marginBottom: "8px" }}>POSITION PROGRESSION (P1 TOP → P22 BOTTOM)</span>
            <svg width="100%" viewBox={`0 0 ${w} ${h}`} style={{ display: "block" }}>
              <line x1="12" y1={h / 2} x2={w - 12} y2={h / 2} stroke="#ebebeb" />
              <polyline fill="none" stroke="#171717" strokeWidth="2.2" points={posPoints} />
            </svg>
          </div>

          <div className="v-cli-dark">
            <div style={{ color: "#8f8f8f", marginBottom: "8px" }}>▲ tail -f /var/log/secu_race_control.log</div>
            {visibleEvents.map((ev, idx) => (
              <div key={idx} style={{ color: ev.kind === "fault" ? "#ff6b6b" : "#ededed", marginBottom: "4px" }}>
                [Lap {ev.lap}] {ev.message}
              </div>
            ))}
          </div>
        </div>

        <div className="v-card" style={{ maxHeight: "440px", overflowY: "auto" }}>
          <span className="eyebrow" style={{ marginBottom: "8px" }}>LIVE 22-CAR CLASSIFICATION // LAP {snap.lap}</span>
          <table className="v-table">
            <thead>
              <tr>
                <th>Pos</th>
                <th>Team &amp; Driver</th>
                <th>Tire</th>
                <th>Pits</th>
                <th>Gap</th>
              </tr>
            </thead>
            <tbody>
              {snap.standings.map((car) => (
                <tr key={car.id} className={car.isOurs ? "row-inverted" : ""}>
                  <td>P{car.pos}</td>
                  <td>{car.team} · {car.driver}</td>
                  <td>{car.tire}</td>
                  <td>{car.pits}</td>
                  <td>{car.status === "DNF" ? "DNF" : car.pos === 1 ? "Leader" : `+${car.gapToLeaderSec.toFixed(1)}s`}</td>
                </tr>
              ))}
            </tbody>
          </table>
        </div>
      </div>
    </div>
  );
};
