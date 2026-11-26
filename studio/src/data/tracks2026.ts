import { F1Team2026 } from "./grid2026";

export type TrackId = "monza" | "spa" | "monaco" | "silverstone" | "australia";
export type WeatherMode = "dry" | "changing" | "heavy_rain";
export type FirmwareMode = "clean" | "faulty";

export interface TrackZone {
  endU: number;
  name: string;
  entryKmh: number;
  apexKmh: number;
  brakePct: number;
  mgukKw: number;
  frontFlap: number;
  rearFlap: number;
}

export interface TrackDefinition {
  id: TrackId;
  name: string;
  shortName: string;
  country: string;
  laps: number;
  lengthKm: number;
  baseLapSec: number;
  pitLossSec: number;
  overtakingEase: number; // 0.2 (Monaco) to 0.95 (Monza)
  tireWearFactor: number;
  downforceLabel: string;
  svgPath: string;
  waypoints: Array<[number, number]>;
  labels: Array<{ x: number; y: number; text: string }>;
  zones: TrackZone[];
}

export interface LiveTelemetryFrame {
  idx: number;
  distanceM: number;
  trackX: number;
  trackY: number;
  zoneName: string;
  speedKmh: number;
  rivalSpeedKmh: number;
  throttlePct: number;
  brakePct: number;
  mgukKw: number;
  batterySocPct: number;
  frontFlapDeg: number;
  rearFlapDeg: number;
  aeroState: string;
  secuCycles: number;
  wcetBreached: boolean;
  activeFaultTag: string | null;
  gapToRivalSec: number;
}

export const TRACKS_2026: Record<TrackId, TrackDefinition> = {
  monza: {
    id: "monza",
    name: "Autodromo Nazionale Monza",
    shortName: "Monza",
    country: "Italy",
    laps: 53,
    lengthKm: 5.793,
    baseLapSec: 81.712,
    pitLossSec: 20.5,
    overtakingEase: 0.95,
    tireWearFactor: 0.88,
    downforceLabel: "Ultra-Low Drag",
    svgPath:
      "M 115 230 L 420 230 Q 462 230 450 206 L 340 94 Q 315 74 282 76 L 210 56 Q 178 56 168 88 L 140 160 Q 124 182 94 174 Q 66 174 70 212 Q 74 230 115 230 Z",
    waypoints: [
      [115, 230], [265, 230], [420, 230], [452, 212], [412, 168],
      [340, 94], [282, 76], [210, 56], [170, 86], [140, 160],
      [96, 174], [70, 208], [115, 230]
    ],
    labels: [
      { x: 120, y: 252, text: "Start / Pit Straight" },
      { x: 400, y: 252, text: "Turn 1 Rettifilo" },
      { x: 185, y: 42, text: "Lesmo 1 & 2" },
      { x: 18, y: 170, text: "Ascari" }
    ],
    zones: [
      { endU: 0.18, name: "Main Pit Straight", entryKmh: 318, apexKmh: 358, brakePct: 0, mgukKw: 345, frontFlap: 18, rearFlap: 28 },
      { endU: 0.26, name: "Turn 1 Rettifilo Chicane", entryKmh: 358, apexKmh: 84, brakePct: 98, mgukKw: -350, frontFlap: 2, rearFlap: 4 },
      { endU: 0.42, name: "Curva Grande & Roggia", entryKmh: 155, apexKmh: 338, brakePct: 65, mgukKw: 310, frontFlap: 16, rearFlap: 24 },
      { endU: 0.58, name: "Lesmo 1 & Lesmo 2", entryKmh: 260, apexKmh: 174, brakePct: 48, mgukKw: -160, frontFlap: 3, rearFlap: 6 },
      { endU: 0.76, name: "Serraglio & Ascari Chicane", entryKmh: 340, apexKmh: 188, brakePct: 72, mgukKw: 290, frontFlap: 18, rearFlap: 28 },
      { endU: 1.0, name: "Back Straight & Parabolica", entryKmh: 344, apexKmh: 212, brakePct: 45, mgukKw: -180, frontFlap: 4, rearFlap: 8 }
    ]
  },
  spa: {
    id: "spa",
    name: "Circuit de Spa-Francorchamps",
    shortName: "Spa",
    country: "Belgium",
    laps: 44,
    lengthKm: 7.004,
    baseLapSec: 103.85,
    pitLossSec: 23.0,
    overtakingEase: 0.90,
    tireWearFactor: 1.15,
    downforceLabel: "Medium-Low Aero",
    svgPath:
      "M 95 210 L 145 245 Q 170 250 185 225 L 220 155 L 325 65 Q 360 46 392 72 L 440 125 Q 456 150 422 168 L 355 178 L 315 212 L 245 198 L 175 182 Q 120 175 95 210 Z",
    waypoints: [
      [95, 210], [150, 244], [185, 224], [220, 155], [325, 65],
      [388, 70], [440, 128], [418, 168], [355, 178], [315, 212],
      [245, 198], [160, 180], [95, 210]
    ],
    labels: [
      { x: 64, y: 242, text: "La Source" },
      { x: 185, y: 145, text: "Eau Rouge / Raidillon" },
      { x: 298, y: 48, text: "Kemmel Straight" },
      { x: 398, y: 190, text: "Pouhon & Stavelot" }
    ],
    zones: [
      { endU: 0.14, name: "La Source Hairpin", entryKmh: 305, apexKmh: 76, brakePct: 96, mgukKw: -350, frontFlap: 2, rearFlap: 4 },
      { endU: 0.34, name: "Eau Rouge, Raidillon & Kemmel", entryKmh: 280, apexKmh: 348, brakePct: 0, mgukKw: 350, frontFlap: 18, rearFlap: 28 },
      { endU: 0.50, name: "Les Combes & Malmedy", entryKmh: 348, apexKmh: 138, brakePct: 88, mgukKw: -320, frontFlap: 3, rearFlap: 5 },
      { endU: 0.72, name: "Pouhon Double-Left & Campus", entryKmh: 295, apexKmh: 242, brakePct: 28, mgukKw: 180, frontFlap: 4, rearFlap: 8 },
      { endU: 0.88, name: "Stavelot & Blanchimont", entryKmh: 260, apexKmh: 334, brakePct: 0, mgukKw: 340, frontFlap: 18, rearFlap: 27 },
      { endU: 1.0, name: "Bus Stop Chicane", entryKmh: 334, apexKmh: 82, brakePct: 98, mgukKw: -350, frontFlap: 2, rearFlap: 4 }
    ]
  },
  monaco: {
    id: "monaco",
    name: "Circuit de Monaco",
    shortName: "Monaco",
    country: "Monte Carlo",
    laps: 78,
    lengthKm: 3.337,
    baseLapSec: 70.62,
    pitLossSec: 21.5,
    overtakingEase: 0.22,
    tireWearFactor: 0.68,
    downforceLabel: "Max Downforce",
    svgPath:
      "M 110 218 L 195 190 L 245 120 Q 268 74 325 78 Q 375 82 395 118 Q 418 142 385 152 Q 355 142 365 124 L 425 148 Q 452 170 420 196 L 310 212 L 250 228 L 165 236 Q 112 242 110 218 Z",
    waypoints: [
      [110, 218], [195, 190], [245, 120], [315, 78], [390, 114],
      [375, 146], [428, 154], [424, 194], [310, 212], [235, 228],
      [155, 236], [110, 218]
    ],
    labels: [
      { x: 112, y: 184, text: "Sainte Devote" },
      { x: 278, y: 62, text: "Casino Square" },
      { x: 405, y: 120, text: "Fairmont Hairpin" },
      { x: 325, y: 235, text: "Tunnel & Chicane" },
      { x: 145, y: 256, text: "Swimming Pool & Rascasse" }
    ],
    zones: [
      { endU: 0.16, name: "Sainte Devote & Beau Rivage", entryKmh: 286, apexKmh: 108, brakePct: 86, mgukKw: -310, frontFlap: 2, rearFlap: 4 },
      { endU: 0.34, name: "Massenet & Casino Square", entryKmh: 268, apexKmh: 132, brakePct: 62, mgukKw: -210, frontFlap: 2, rearFlap: 4 },
      { endU: 0.50, name: "Mirabeau & Fairmont Hairpin", entryKmh: 165, apexKmh: 48, brakePct: 95, mgukKw: -340, frontFlap: 1, rearFlap: 2 },
      { endU: 0.68, name: "Portier & Monaco Tunnel", entryKmh: 150, apexKmh: 294, brakePct: 0, mgukKw: 345, frontFlap: 16, rearFlap: 24 },
      { endU: 0.84, name: "Nouvelle Chicane & Tabac", entryKmh: 294, apexKmh: 74, brakePct: 96, mgukKw: -350, frontFlap: 2, rearFlap: 4 },
      { endU: 1.0, name: "Swimming Pool & La Rascasse", entryKmh: 225, apexKmh: 62, brakePct: 88, mgukKw: -290, frontFlap: 2, rearFlap: 4 }
    ]
  },
  silverstone: {
    id: "silverstone",
    name: "Silverstone Circuit",
    shortName: "Silverstone",
    country: "United Kingdom",
    laps: 52,
    lengthKm: 5.891,
    baseLapSec: 85.84,
    pitLossSec: 20.0,
    overtakingEase: 0.78,
    tireWearFactor: 1.25,
    downforceLabel: "High-Speed Aero",
    svgPath:
      "M 110 198 L 195 232 L 285 215 L 270 168 L 355 138 L 430 92 Q 446 66 408 62 L 325 76 L 235 66 Q 175 68 158 106 L 192 142 L 122 152 Q 92 168 110 198 Z",
    waypoints: [
      [110, 198], [195, 232], [285, 215], [270, 168], [355, 138],
      [430, 92], [402, 62], [325, 76], [235, 66], [162, 104],
      [190, 142], [118, 156], [110, 198]
    ],
    labels: [
      { x: 135, y: 248, text: "Abbey & Farm" },
      { x: 352, y: 158, text: "Wellington Straight" },
      { x: 392, y: 48, text: "Copse" },
      { x: 202, y: 52, text: "Maggotts & Becketts" },
      { x: 64, y: 145, text: "Stowe & Vale" }
    ],
    zones: [
      { endU: 0.16, name: "Abbey, Village & The Loop", entryKmh: 304, apexKmh: 102, brakePct: 88, mgukKw: -315, frontFlap: 2, rearFlap: 4 },
      { endU: 0.34, name: "Wellington Straight & Brooklands", entryKmh: 215, apexKmh: 326, brakePct: 72, mgukKw: 340, frontFlap: 18, rearFlap: 28 },
      { endU: 0.50, name: "Copse High-Speed Entry", entryKmh: 318, apexKmh: 274, brakePct: 12, mgukKw: 260, frontFlap: 4, rearFlap: 8 },
      { endU: 0.68, name: "Maggotts, Becketts & Chapel", entryKmh: 306, apexKmh: 218, brakePct: 35, mgukKw: 120, frontFlap: 2, rearFlap: 4 },
      { endU: 0.84, name: "Hangar Straight & Stowe", entryKmh: 265, apexKmh: 334, brakePct: 78, mgukKw: 350, frontFlap: 18, rearFlap: 28 },
      { endU: 1.0, name: "Vale & Club Chicane", entryKmh: 270, apexKmh: 94, brakePct: 94, mgukKw: -345, frontFlap: 2, rearFlap: 4 }
    ]
  },
  australia: {
    id: "australia",
    name: "Albert Park Circuit",
    shortName: "Australia",
    country: "Melbourne",
    laps: 58,
    lengthKm: 5.278,
    baseLapSec: 76.24,
    pitLossSec: 21.0,
    overtakingEase: 0.65,
    tireWearFactor: 0.96,
    downforceLabel: "Medium-High Aero",
    svgPath:
      "M 105 182 L 245 218 Q 275 224 295 196 L 348 164 L 425 142 Q 452 124 432 96 L 352 72 Q 290 64 232 94 L 164 114 L 102 128 Q 76 156 105 182 Z",
    waypoints: [
      [105, 182], [245, 218], [295, 196], [348, 164], [425, 142],
      [432, 96], [352, 72], [232, 94], [164, 114], [98, 134], [105, 182]
    ],
    labels: [
      { x: 132, y: 232, text: "Pit Straight & T1" },
      { x: 372, y: 174, text: "Lakeside Drive" },
      { x: 345, y: 58, text: "Turn 9–10 Fast Chicane" },
      { x: 64, y: 112, text: "Turn 13–14 Complex" }
    ],
    zones: [
      { endU: 0.18, name: "Main Straight & Turn 1–2", entryKmh: 324, apexKmh: 122, brakePct: 88, mgukKw: -330, frontFlap: 16, rearFlap: 26 },
      { endU: 0.36, name: "Turn 3 Braking & Sector 1 Sweep", entryKmh: 312, apexKmh: 96, brakePct: 94, mgukKw: -345, frontFlap: 2, rearFlap: 4 },
      { endU: 0.56, name: "Lakeside High-Speed Straight", entryKmh: 240, apexKmh: 331, brakePct: 0, mgukKw: 350, frontFlap: 18, rearFlap: 28 },
      { endU: 0.72, name: "Turn 9–10 Ultra-Fast Chicane", entryKmh: 331, apexKmh: 248, brakePct: 24, mgukKw: 160, frontFlap: 3, rearFlap: 6 },
      { endU: 0.88, name: "Turn 11 & Turn 13 Heavy Stop", entryKmh: 308, apexKmh: 108, brakePct: 92, mgukKw: -340, frontFlap: 2, rearFlap: 4 },
      { endU: 1.0, name: "Turn 14 Exit to Finish", entryKmh: 215, apexKmh: 298, brakePct: 18, mgukKw: 310, frontFlap: 16, rearFlap: 24 }
    ]
  }
};

export const TRACK_LIST: TrackDefinition[] = [
  TRACKS_2026.monza,
  TRACKS_2026.spa,
  TRACKS_2026.monaco,
  TRACKS_2026.silverstone,
  TRACKS_2026.australia
];

function sampleWaypoints(pts: Array<[number, number]>, tNorm: number): [number, number] {
  const scaled = tNorm * (pts.length - 1);
  const i0 = Math.floor(scaled);
  const i1 = Math.min(pts.length - 1, i0 + 1);
  const frac = scaled - i0;
  const [x0, y0] = pts[i0];
  const [x1, y1] = pts[i1];
  return [x0 + (x1 - x0) * frac, y0 + (y1 - y0) * frac];
}

export function getTeamTrackLapSec(
  team: F1Team2026,
  track: TrackDefinition,
  weather: WeatherMode
): number {
  const relToPole = team.monzaQualiSec / 81.786;
  const rainPenalty = weather === "heavy_rain" ? 1.115 : weather === "changing" ? 1.045 : 1.0;
  return track.baseLapSec * relToPole * rainPenalty;
}

export function getOurTrackLapSec(
  track: TrackDefinition,
  firmwareMode: FirmwareMode,
  weather: WeatherMode
): number {
  const rainPenalty = weather === "heavy_rain" ? 1.105 : weather === "changing" ? 1.04 : 1.0;
  const firmwarePenalty = firmwareMode === "faulty" ? 1.046 : 1.0062;
  return track.baseLapSec * rainPenalty * firmwarePenalty;
}

export function formatLapTime(sec: number): string {
  const mins = Math.floor(sec / 60);
  const rem = sec - mins * 60;
  return `${mins}:${rem < 10 ? "0" : ""}${rem.toFixed(3)}`;
}

export function generateTrackTelemetryStream(
  track: TrackDefinition,
  firmwareMode: FirmwareMode,
  weather: WeatherMode,
  rival: F1Team2026,
  staticCeiling: number,
  totalSteps = 220
): LiveTelemetryFrame[] {
  const frames: LiveTelemetryFrame[] = [];
  const ourLap = getOurTrackLapSec(track, firmwareMode, weather);
  const rivalLap = getTeamTrackLapSec(rival, track, weather);
  let soc = 72.0;

  for (let i = 0; i < totalSteps; i++) {
    const u = i / totalSteps;
    const distanceM = Math.round(u * track.lengthKm * 1000);
    const [trackX, trackY] = sampleWaypoints(track.waypoints, u);

    const zone = track.zones.find((z) => u <= z.endU) ?? track.zones[track.zones.length - 1];
    const zoneIdx = track.zones.indexOf(zone);
    const startU = zoneIdx === 0 ? 0 : track.zones[zoneIdx - 1].endU;
    const localK = Math.max(0, Math.min(1, (u - startU) / Math.max(0.01, zone.endU - startU)));

    // Smooth speed interpolation within each track zone
    let baseSpeed =
      zone.entryKmh + (zone.apexKmh - zone.entryKmh) * Math.sin(localK * Math.PI * 0.78);
    let mgukKw = zone.mgukKw;
    let frontFlap = zone.frontFlap;
    let rearFlap = zone.rearFlap;
    let aeroState = frontFlap >= 12 ? "Straight (Low Drag)" : "Corner (High Downforce)";
    let secuCycles = i % 10 === 0 ? 54 + (i % 7) : 16 + (i % 6);
    let wcetBreached = false;
    let activeFaultTag: string | null = null;

    // Weather adjustments (wet-weather single-flap fallback from active_aero_sync.cpp)
    const isWetHere =
      weather === "heavy_rain" || (weather === "changing" && u >= 0.42 && u <= 0.82);
    if (isWetHere) {
      baseSpeed *= weather === "heavy_rain" ? 0.89 : 0.94;
      frontFlap = 0;
      rearFlap = Math.min(rearFlap, 12);
      aeroState = "Wet Fallback (Front Locked)";
    }

    let speedKmh = baseSpeed * 0.991;
    const rivalSpeedKmh = baseSpeed * rival.speedScale;

    // Direct Firmware Coupling: Faulty firmware injects real static-analysis bugs into live telemetry!
    if (firmwareMode === "faulty") {
      // Aero sensor narrowing/uninitialized bias -> forces fail-safe closed flaps
      frontFlap = 0;
      rearFlap = 0;
      aeroState = "Fail-Safe Locked (MISRA-11.6.1)";
      speedKmh *= 0.945;

      // Periodic unbounded CAN poll stall (SECU-WCET-001) + Heap jitter (MISRA-21.6.1)
      if (i % 29 === 11 || i % 29 === 12) {
        secuCycles = Math.round(staticCeiling * 1.18);
        wcetBreached = true;
        activeFaultTag = "SECU-WCET-001: Unbounded CAN Poll Watchdog Stall";
        mgukKw = Math.round(mgukKw * 0.25);
        speedKmh *= 0.91;
      } else if (i % 19 === 7) {
        secuCycles = Math.round(staticCeiling * 0.85);
        activeFaultTag = "MISRA-CPP-21.6.1: Dynamic Heap Allocation Jitter";
      } else {
        secuCycles += 26;
      }
    }

    soc = Math.max(16, Math.min(96, soc - (mgukKw / 350) * 0.32));
    const gapToRivalSec = (ourLap - rivalLap) * u;

    frames.push({
      idx: i,
      distanceM,
      trackX,
      trackY,
      zoneName: zone.name,
      speedKmh: Math.round(speedKmh * 10) / 10,
      rivalSpeedKmh: Math.round(rivalSpeedKmh * 10) / 10,
      throttlePct: zone.brakePct > 30 ? 0 : 100,
      brakePct: zone.brakePct,
      mgukKw,
      batterySocPct: Math.round(soc * 10) / 10,
      frontFlapDeg: frontFlap,
      rearFlapDeg: rearFlap,
      aeroState,
      secuCycles,
      wcetBreached,
      activeFaultTag,
      gapToRivalSec: Math.round(gapToRivalSec * 1000) / 1000
    });
  }

  return frames;
}
