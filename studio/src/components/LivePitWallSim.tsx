import React, { useEffect, useMemo, useState } from "react";
import { F1Team2026, GRID_2026_TEAMS } from "../data/grid2026";
import {
  FirmwareMode,
  TRACKS_2026,
  TRACK_LIST,
  TrackId,
  WeatherMode,
  formatLapTime,
  generateTrackTelemetryStream,
  getOurTrackLapSec,
  getTeamTrackLapSec
} from "../data/tracks2026";

interface Props {
  firmwareMode: FirmwareMode;
  staticWcetCeiling: number;
  selectedTrackId: TrackId;
  onSelectTrack: (id: TrackId) => void;
  weather: WeatherMode;
  onSelectWeather: (w: WeatherMode) => void;
}

export const LivePitWallSim: React.FC<Props> = ({
  firmwareMode,
  staticWcetCeiling,
  selectedTrackId,
  onSelectTrack,
  weather,
  onSelectWeather
}) => {
  const [subView, setSubView] = useState<"oscilloscopes" | "grid">("oscilloscopes");
  const [selectedRivalId, setSelectedRivalId] = useState<string>("redbull");
  const [isPlaying, setIsPlaying] = useState<boolean>(true);
  const [playbackSpeed, setPlaybackSpeed] = useState<number>(1);
  const [cursor, setCursor] = useState<number>(36);

  const track = TRACKS_2026[selectedTrackId];
  const rival: F1Team2026 = useMemo(
    () => GRID_2026_TEAMS.find((t) => t.id === selectedRivalId) ?? GRID_2026_TEAMS[0],
    [selectedRivalId]
  );

  const stream = useMemo(
    () => generateTrackTelemetryStream(track, firmwareMode, weather, rival, staticWcetCeiling, 220),
    [track, firmwareMode, weather, rival, staticWcetCeiling]
  );

  useEffect(() => {
    if (!isPlaying) return;
    const ms = Math.max(16, Math.round(45 / playbackSpeed));
    const timer = window.setInterval(() => {
      setCursor((prev) => (prev + 1) % stream.length);
    }, ms);
    return () => window.clearInterval(timer);
  }, [isPlaying, playbackSpeed, stream.length]);

  const current = stream[cursor % stream.length] ?? stream[0];
  const ourLapSec = getOurTrackLapSec(track, firmwareMode, weather);
  const rivalLapSec = getTeamTrackLapSec(rival, track, weather);
  const rivalCursor = Math.min(
    stream.length - 1,
    Math.max(0, Math.round((cursor % stream.length) * (ourLapSec / rivalLapSec)))
  );
  const rivalPt = stream[rivalCursor] ?? current;

  const rollingSlice = useMemo(() => {
    const out = [];
    for (let k = 84; k >= 0; k--) {
      out.push(stream[(cursor - k + stream.length) % stream.length]);
    }
    return out;
  }, [cursor, stream]);

  const gridStandings = useMemo(() => {
    const rows = [
      {
        id: "our_secu",
        isOurs: true,
        team: "SECU Prototype (Our Car)",
        detail: firmwareMode === "clean" ? "Certified SECU · Upper-Midfield" : "Faulty SECU · Aero Lockout",
        lapSec: ourLapSec
      },
      ...GRID_2026_TEAMS.map((t) => ({
        id: t.id,
        isOurs: false,
        team: t.team,
        detail: `${t.leadDriver} · ${t.chassis}`,
        lapSec: getTeamTrackLapSec(t, track, weather)
      }))
    ];
    rows.sort((a, b) => a.lapSec - b.lapSec);
    const pole = rows[0].lapSec;
    return rows.map((r, i) => ({ ...r, pos: i + 1, gap: r.lapSec - pole }));
  }, [track, firmwareMode, weather, ourLapSec]);

  const buildPoly = (vals: number[], minV: number, maxV: number, w: number, h: number) =>
    vals
      .map((v, i) => {
        const x = (i / Math.max(1, vals.length - 1)) * w;
        const n = (v - minV) / Math.max(1e-6, maxV - minV);
        const y = h - 6 - Math.max(0, Math.min(1, n)) * (h - 12);
        return `${x.toFixed(1)},${y.toFixed(1)}`;
      })
      .join(" ");

  const ourSpeedPoly = buildPoly(rollingSlice.map((p) => p.speedKmh), 40, 365, 520, 96);
  const rivalSpeedPoly = buildPoly(rollingSlice.map((p) => p.rivalSpeedKmh), 40, 365, 520, 96);
  const mgukPoly = buildPoly(rollingSlice.map((p) => p.mgukKw), -360, 360, 520, 82);
  const cpuChartMax = firmwareMode === "faulty" ? Math.round(staticWcetCeiling * 1.3) : 120;
  const cpuPoly = buildPoly(rollingSlice.map((p) => p.secuCycles), 0, cpuChartMax, 520, 82);
  const ceilingLineY = firmwareMode === "faulty" ? 82 - 6 - (staticWcetCeiling / cpuChartMax) * 70 : 8;

  return (
    <div style={{ display: "flex", flexDirection: "column", gap: "20px" }}>
      <div style={{ display: "flex", justifyContent: "space-between", alignItems: "flex-end", flexWrap: "wrap", gap: "12px" }}>
        <div>
          <span className="eyebrow">5-CIRCUIT TELEMETRY &amp; 11-TEAM BENCHMARK</span>
          <h2 className="vercel-h2">{track.name} ({formatLapTime(ourLapSec)})</h2>
        </div>

        <div className="segmented">
          <button
            className={`seg-btn ${subView === "oscilloscopes" ? "seg-btn-active" : ""}`}
            onClick={() => setSubView("oscilloscopes")}
          >
            Live Telemetry Traces
          </button>
          <button
            className={`seg-btn ${subView === "grid" ? "seg-btn-active" : ""}`}
            onClick={() => setSubView("grid")}
          >
            2026 11-Team Pace Table
          </button>
        </div>
      </div>

      {/* Track, Weather & Playback Bar */}
      <div className="v-card" style={{ display: "flex", justifyContent: "space-between", flexWrap: "wrap", gap: "10px", padding: "12px 16px" }}>
        <div className="btn-row">
          {TRACK_LIST.map((t) => (
            <button
              key={t.id}
              className={t.id === selectedTrackId ? "btn-pill-dark" : "btn-pill-light"}
              onClick={() => onSelectTrack(t.id)}
            >
              {t.shortName}
            </button>
          ))}
        </div>

        <div className="btn-row">
          {(["dry", "changing", "heavy_rain"] as WeatherMode[]).map((w) => (
            <button
              key={w}
              className={weather === w ? "btn-pill-dark" : "btn-pill-light"}
              onClick={() => onSelectWeather(w)}
            >
              {w === "dry" ? "Dry" : w === "changing" ? "Shower" : "Heavy Rain"}
            </button>
          ))}
        </div>

        <div className="btn-row">
          <button className="btn-primary btn-sm" onClick={() => setIsPlaying((p) => !p)}>
            {isPlaying ? "Pause" : "Play"}
          </button>
          {[1, 2, 4].map((s) => (
            <button
              key={s}
              className={playbackSpeed === s ? "btn-pill-dark" : "btn-pill-light"}
              onClick={() => setPlaybackSpeed(s)}
            >
              {s}×
            </button>
          ))}
        </div>
      </div>

      {subView === "oscilloscopes" ? (
        <div style={{ display: "grid", gridTemplateColumns: "minmax(0, 1fr) minmax(0, 1.3fr)", gap: "16px" }}>
          <div className="v-card" style={{ display: "flex", flexDirection: "column", justifyContent: "space-between" }}>
            <div>
              <span className="eyebrow">{track.lengthKm} KM · {track.downforceLabel}</span>
              <h3 className="vercel-card-title" style={{ marginTop: "4px" }}>{current.zoneName}</h3>

              <svg width="100%" viewBox="0 0 520 270" style={{ display: "block", margin: "12px 0" }}>
                <path d={track.svgPath} fill="none" stroke="#ebebeb" strokeWidth="12" strokeLinecap="round" strokeLinejoin="round" />
                <path d={track.svgPath} fill="none" stroke="#171717" strokeOpacity="0.3" strokeWidth="1.2" />
                {track.labels.map((lb, idx) => (
                  <text key={idx} x={lb.x} y={lb.y} fontSize="10" fill="#7d7d7d" fontFamily="monospace">
                    {lb.text}
                  </text>
                ))}
                <circle cx={rivalPt.trackX} cy={rivalPt.trackY} r="5.5" fill="#ffffff" stroke="#7d7d7d" strokeWidth="2" />
                <circle cx={current.trackX} cy={current.trackY} r="6.5" fill={firmwareMode === "faulty" ? "#c92a2a" : "#171717"} />
              </svg>
            </div>

            <div className="grid-3" style={{ paddingTop: "12px", borderTop: "1px solid var(--color-hairline)" }}>
              <div>
                <span className="eyebrow">SPEED</span>
                <div className="mono" style={{ fontSize: "16px", fontWeight: 500 }}>{current.speedKmh} km/h</div>
              </div>
              <div>
                <span className="eyebrow">MGU-K</span>
                <div className="mono" style={{ fontSize: "16px", fontWeight: 500 }}>{current.mgukKw > 0 ? `+${current.mgukKw}` : current.mgukKw} kW</div>
              </div>
              <div>
                <span className="eyebrow">SECU CYCLES</span>
                <div className="mono" style={{ fontSize: "16px", fontWeight: 500, color: current.wcetBreached ? "var(--color-alert-red)" : "var(--color-terminal-green)" }}>
                  {current.secuCycles}c
                </div>
              </div>
            </div>

            <input
              type="range"
              min={0}
              max={stream.length - 1}
              value={cursor % stream.length}
              onChange={(e) => {
                setIsPlaying(false);
                setCursor(Number(e.target.value));
              }}
              style={{ width: "100%", marginTop: "12px", accentColor: "#171717" }}
            />
          </div>

          <div style={{ display: "flex", flexDirection: "column", gap: "12px" }}>
            <div className="v-card" style={{ padding: "14px 16px" }}>
              <div className="mono" style={{ display: "flex", justifyContent: "space-between", fontSize: "11.5px", marginBottom: "6px" }}>
                <span>SPEED (KM/H) — OUR CAR VS. {rival.team.toUpperCase()}</span>
                <span>{current.speedKmh} / {current.rivalSpeedKmh}</span>
              </div>
              <svg width="100%" viewBox="0 0 520 96" style={{ display: "block" }}>
                <polyline fill="none" stroke="#a8a8a8" strokeWidth="1.5" strokeDasharray="4 3" points={rivalSpeedPoly} />
                <polyline fill="none" stroke="#171717" strokeWidth="2.2" points={ourSpeedPoly} />
              </svg>
            </div>

            <div className="v-card" style={{ padding: "14px 16px" }}>
              <div className="mono" style={{ display: "flex", justifyContent: "space-between", fontSize: "11.5px", marginBottom: "6px" }}>
                <span>350 KW MGU-K DEPLOY (+) &amp; REGEN (−)</span>
                <span>{current.mgukKw} kW · {current.aeroState}</span>
              </div>
              <svg width="100%" viewBox="0 0 520 82" style={{ display: "block" }}>
                <line x1="0" y1="41" x2="520" y2="41" stroke="#ebebeb" strokeWidth="1" />
                <polyline fill="none" stroke="#297a3a" strokeWidth="2" points={mgukPoly} />
              </svg>
            </div>

            <div className="v-card" style={{ padding: "14px 16px" }}>
              <div className="mono" style={{ display: "flex", justifyContent: "space-between", fontSize: "11.5px", marginBottom: "6px" }}>
                <span>SECU CONTROLLER CYCLES VS. STATIC WCET ({staticWcetCeiling}c)</span>
                <span style={{ color: firmwareMode === "faulty" ? "var(--color-alert-red)" : "var(--color-terminal-green)" }}>
                  {current.secuCycles}c
                </span>
              </div>
              <svg width="100%" viewBox="0 0 520 82" style={{ display: "block" }}>
                <line x1="0" y1={ceilingLineY} x2="520" y2={ceilingLineY} stroke="#c92a2a" strokeDasharray="5 3" strokeWidth="1.4" />
                <polyline fill="none" stroke={firmwareMode === "faulty" ? "#c92a2a" : "#171717"} strokeWidth="2" points={cpuPoly} />
              </svg>
            </div>
          </div>
        </div>
      ) : (
        <div className="v-card">
          <div style={{ display: "flex", justifyContent: "space-between", marginBottom: "12px" }}>
            <span className="eyebrow">2026 11-CONSTRUCTOR PACE BENCHMARK @ {track.shortName.toUpperCase()} — CLICK TEAM TO SET GHOST RIVAL</span>
            <span className="eyebrow">GHOST: {rival.team}</span>
          </div>
          <table className="v-table">
            <thead>
              <tr>
                <th>Pos</th>
                <th>Constructor</th>
                <th>Reference</th>
                <th>Lap Time</th>
                <th>Gap to Pole</th>
              </tr>
            </thead>
            <tbody>
              {gridStandings.map((row) => (
                <tr
                  key={row.id}
                  onClick={() => { if (!row.isOurs) setSelectedRivalId(row.id); }}
                  className={row.isOurs ? "row-inverted" : row.id === rival.id ? "row-selected" : ""}
                  style={{ cursor: row.isOurs ? "default" : "pointer" }}
                >
                  <td>P{row.pos}</td>
                  <td>{row.team}</td>
                  <td>{row.detail}</td>
                  <td>{formatLapTime(row.lapSec)}</td>
                  <td>{row.gap === 0 ? "Pole" : `+${row.gap.toFixed(3)}s`}</td>
                </tr>
              ))}
            </tbody>
          </table>
        </div>
      )}
    </div>
  );
};
