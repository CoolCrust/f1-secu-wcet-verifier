export interface F1Team2026 {
  id: string;
  rank: number;
  team: string;
  chassis: string;
  powerUnit: string;
  leadDriver: string;
  monzaQualiSec: number;
  monzaQualiFormatted: string;
  s1Sec: number;
  s2Sec: number;
  s3Sec: number;
  topSpeedKmh: number;
  regenEfficiencyPct: number;
  speedScale: number;
  brakingGripScale: number;
}

export interface TelemetryFramePoint {
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
  aeroState: "Straight (Low Drag)" | "Corner (High Downforce)" | "Fail-Safe Locked";
  secuCycles: number;
  gapToRivalSec: number;
}

// 2026 FIA Formula 1 World Championship — All 11 Constructors (Monza Qualifying Benchmarks)
export const GRID_2026_TEAMS: F1Team2026[] = [
  {
    id: "alpine",
    rank: 1,
    team: "Alpine",
    chassis: "A526",
    powerUnit: "Mercedes-AMG M17",
    leadDriver: "Pierre Gasly #10",
    monzaQualiSec: 81.786,
    monzaQualiFormatted: "1:21.786",
    s1Sec: 26.812,
    s2Sec: 27.640,
    s3Sec: 27.334,
    topSpeedKmh: 356,
    regenEfficiencyPct: 96.4,
    speedScale: 1.0,
    brakingGripScale: 1.0
  },
  {
    id: "mercedes",
    rank: 2,
    team: "Mercedes",
    chassis: "F1 W17",
    powerUnit: "Mercedes-AMG M17",
    leadDriver: "George Russell #63",
    monzaQualiSec: 81.846,
    monzaQualiFormatted: "1:21.846",
    s1Sec: 26.845,
    s2Sec: 27.652,
    s3Sec: 27.349,
    topSpeedKmh: 355,
    regenEfficiencyPct: 97.1,
    speedScale: 0.998,
    brakingGripScale: 1.002
  },
  {
    id: "mclaren",
    rank: 3,
    team: "McLaren",
    chassis: "MCL40",
    powerUnit: "Mercedes-AMG M17",
    leadDriver: "Oscar Piastri #81",
    monzaQualiSec: 81.966,
    monzaQualiFormatted: "1:21.966",
    s1Sec: 26.910,
    s2Sec: 27.680,
    s3Sec: 27.376,
    topSpeedKmh: 353,
    regenEfficiencyPct: 95.8,
    speedScale: 0.995,
    brakingGripScale: 1.001
  },
  {
    id: "ferrari",
    rank: 4,
    team: "Ferrari",
    chassis: "SF-26",
    powerUnit: "Ferrari 067/6",
    leadDriver: "Charles Leclerc #16",
    monzaQualiSec: 82.004,
    monzaQualiFormatted: "1:22.004",
    s1Sec: 26.880,
    s2Sec: 27.730,
    s3Sec: 27.394,
    topSpeedKmh: 355,
    regenEfficiencyPct: 96.0,
    speedScale: 0.997,
    brakingGripScale: 0.996
  },
  {
    id: "redbull",
    rank: 5,
    team: "Red Bull Racing",
    chassis: "RB22",
    powerUnit: "Red Bull Ford DM01",
    leadDriver: "Max Verstappen #3",
    monzaQualiSec: 82.070,
    monzaQualiFormatted: "1:22.070",
    s1Sec: 26.940,
    s2Sec: 27.710,
    s3Sec: 27.420,
    topSpeedKmh: 354,
    regenEfficiencyPct: 95.5,
    speedScale: 0.994,
    brakingGripScale: 0.998
  },
  {
    id: "racingbulls",
    rank: 6,
    team: "Racing Bulls",
    chassis: "VCARB 03",
    powerUnit: "Red Bull Ford DM01",
    leadDriver: "Arvid Lindblad #41",
    monzaQualiSec: 82.286,
    monzaQualiFormatted: "1:22.286",
    s1Sec: 27.020,
    s2Sec: 27.805,
    s3Sec: 27.461,
    topSpeedKmh: 351,
    regenEfficiencyPct: 94.2,
    speedScale: 0.990,
    brakingGripScale: 0.992
  },
  {
    id: "audi",
    rank: 7,
    team: "Audi",
    chassis: "R26",
    powerUnit: "Audi AFR 26 Hybrid",
    leadDriver: "Gabriel Bortoleto #5",
    monzaQualiSec: 82.517,
    monzaQualiFormatted: "1:22.517",
    s1Sec: 27.110,
    s2Sec: 27.890,
    s3Sec: 27.517,
    topSpeedKmh: 350,
    regenEfficiencyPct: 93.8,
    speedScale: 0.987,
    brakingGripScale: 0.988
  },
  {
    id: "haas",
    rank: 8,
    team: "Haas F1 Team",
    chassis: "VF-26",
    powerUnit: "Ferrari 067/6",
    leadDriver: "Oliver Bearman #87",
    monzaQualiSec: 82.756,
    monzaQualiFormatted: "1:22.756",
    s1Sec: 27.185,
    s2Sec: 27.990,
    s3Sec: 27.581,
    topSpeedKmh: 349,
    regenEfficiencyPct: 93.1,
    speedScale: 0.984,
    brakingGripScale: 0.985
  },
  {
    id: "williams",
    rank: 9,
    team: "Williams",
    chassis: "FW48",
    powerUnit: "Mercedes-AMG M17",
    leadDriver: "Carlos Sainz #55",
    monzaQualiSec: 83.453,
    monzaQualiFormatted: "1:23.453",
    s1Sec: 27.390,
    s2Sec: 28.250,
    s3Sec: 27.813,
    topSpeedKmh: 352,
    regenEfficiencyPct: 92.0,
    speedScale: 0.986,
    brakingGripScale: 0.972
  },
  {
    id: "cadillac",
    rank: 10,
    team: "Cadillac F1 Team",
    chassis: "MAC-26",
    powerUnit: "Ferrari 067/6",
    leadDriver: "Valtteri Bottas #77",
    monzaQualiSec: 84.364,
    monzaQualiFormatted: "1:24.364",
    s1Sec: 27.710,
    s2Sec: 28.560,
    s3Sec: 28.094,
    topSpeedKmh: 346,
    regenEfficiencyPct: 90.4,
    speedScale: 0.972,
    brakingGripScale: 0.964
  },
  {
    id: "astonmartin",
    rank: 11,
    team: "Aston Martin",
    chassis: "AMR26",
    powerUnit: "Honda RA626H",
    leadDriver: "Fernando Alonso #14",
    monzaQualiSec: 85.150,
    monzaQualiFormatted: "1:25.150",
    s1Sec: 27.990,
    s2Sec: 28.840,
    s3Sec: 28.320,
    topSpeedKmh: 343,
    regenEfficiencyPct: 89.2,
    speedScale: 0.964,
    brakingGripScale: 0.955
  }
];

export type SimSetupProfile = "certified_attack" | "eco_harvest" | "faulty_lockout";

export const SETUP_PROFILES: Record<
  SimSetupProfile,
  {
    label: string;
    lapTimeSec: number;
    lapTimeFormatted: string;
    s1Sec: number;
    s2Sec: number;
    s3Sec: number;
    topSpeedKmh: number;
    summary: string;
  }
> = {
  certified_attack: {
    label: "Certified SECU (350 kW Attack + Active Aero)",
    lapTimeSec: 81.712,
    lapTimeFormatted: "1:21.712",
    s1Sec: 26.780,
    s2Sec: 27.612,
    s3Sec: 27.320,
    topSpeedKmh: 358,
    summary: "Deterministic 1 kHz MGU-K torque blending + low-drag Straight Mode beats 2026 Monza pole by 0.074s."
  },
  eco_harvest: {
    label: "Battery Recharge Mode (Max Regen Cap)",
    lapTimeSec: 82.340,
    lapTimeFormatted: "1:22.340",
    s1Sec: 27.040,
    s2Sec: 27.820,
    s3Sec: 27.480,
    topSpeedKmh: 348,
    summary: "Early lift-and-coast harvests +4.2 MJ/lap for overtake deployment while staying P6 pace."
  },
  faulty_lockout: {
    label: "Faulty Firmware (Flap Lockout + WCET Stall)",
    lapTimeSec: 84.910,
    lapTimeFormatted: "1:24.910",
    s1Sec: 27.910,
    s2Sec: 28.760,
    s3Sec: 28.240,
    topSpeedKmh: 334,
    summary: "Asymmetric wing sensor fault forces fail-safe closed flaps (+3.20s drag penalty per lap)."
  }
};

// Normalized 2D Monza circuit waypoints (5.793 km)
const MONZA_WAYPOINTS: Array<[number, number]> = [
  [120, 235], [260, 235], [410, 235], [455, 225], // Main Pit Straight -> Turn 1 Rettifilo Chicane
  [445, 198], [415, 165], [375, 125], [335, 92],  // Curva Grande -> Variante della Roggia
  [305, 88],  [265, 65],  [215, 58],  [175, 78],  // Lesmo 1 & Lesmo 2
  [158, 118], [142, 158], [118, 178], [92, 174],  // Serraglio Straight -> Variante Ascari
  [74, 192],  [70, 218],  [82, 232],  [120, 235]  // Back Straight -> Curva Parabolica -> Finish
];

function sampleTrackPoint(tNorm: number): [number, number] {
  const scaled = tNorm * (MONZA_WAYPOINTS.length - 1);
  const i0 = Math.floor(scaled);
  const i1 = Math.min(MONZA_WAYPOINTS.length - 1, i0 + 1);
  const frac = scaled - i0;
  const [x0, y0] = MONZA_WAYPOINTS[i0];
  const [x1, y1] = MONZA_WAYPOINTS[i1];
  return [x0 + (x1 - x0) * frac, y0 + (y1 - y0) * frac];
}

export function generateMonzaTelemetryStream(
  profile: SimSetupProfile,
  rival: F1Team2026,
  totalSteps = 240
): TelemetryFramePoint[] {
  const frames: TelemetryFramePoint[] = [];
  const ourLapSec = SETUP_PROFILES[profile].lapTimeSec;
  let soc = 68.0;

  for (let i = 0; i < totalSteps; i++) {
    const u = i / totalSteps;
    const distanceM = Math.round(u * 5793);
    const [trackX, trackY] = sampleTrackPoint(u);

    let zoneName = "Main Pit Straight";
    let baseSpeed = 352;
    let throttlePct = 100;
    let brakePct = 0;
    let mgukKw = 310;
    let frontFlap = 18;
    let rearFlap = 28;
    let aeroState: TelemetryFramePoint["aeroState"] = "Straight (Low Drag)";
    let secuCycles = 18;

    if (u < 0.16) {
      zoneName = "Main Pit Straight (Straight Mode)";
      baseSpeed = 318 + (u / 0.16) * 40;
      throttlePct = 100;
      brakePct = 0;
      mgukKw = 340;
      frontFlap = 18;
      rearFlap = 28;
      secuCycles = i % 10 === 0 ? 52 : 14;
    } else if (u < 0.22) {
      zoneName = "Turn 1 Variante del Rettifilo (350 kW Regen)";
      const k = (u - 0.16) / 0.06;
      baseSpeed = 358 - Math.sin(k * Math.PI * 0.85) * 265;
      throttlePct = 0;
      brakePct = 98;
      mgukKw = -350;
      frontFlap = 2;
      rearFlap = 4;
      aeroState = "Corner (High Downforce)";
      secuCycles = i % 10 === 0 ? 64 : 22;
    } else if (u < 0.36) {
      zoneName = "Curva Grande (Full Deployment)";
      const k = (u - 0.22) / 0.14;
      baseSpeed = 145 + k * 195;
      throttlePct = 100;
      brakePct = 0;
      mgukKw = 350;
      frontFlap = 16;
      rearFlap = 25;
      secuCycles = i % 10 === 0 ? 54 : 16;
    } else if (u < 0.43) {
      zoneName = "Variante della Roggia Braking Zone";
      const k = (u - 0.36) / 0.07;
      baseSpeed = 340 - Math.sin(k * Math.PI * 0.85) * 215;
      throttlePct = 0;
      brakePct = 92;
      mgukKw = -345;
      frontFlap = 2;
      rearFlap = 4;
      aeroState = "Corner (High Downforce)";
      secuCycles = i % 10 === 0 ? 62 : 21;
    } else if (u < 0.58) {
      zoneName = "Lesmo 1 & Lesmo 2 (High Downforce)";
      const k = (u - 0.43) / 0.15;
      baseSpeed = 185 + Math.sin(k * Math.PI * 2) * 42 + k * 45;
      throttlePct = 78;
      brakePct = 18;
      mgukKw = 120;
      frontFlap = 3;
      rearFlap = 6;
      aeroState = "Corner (High Downforce)";
      secuCycles = i % 10 === 0 ? 56 : 18;
    } else if (u < 0.72) {
      zoneName = "Serraglio Straight (Overtake Override)";
      const k = (u - 0.58) / 0.14;
      baseSpeed = 245 + k * 96;
      throttlePct = 100;
      brakePct = 0;
      mgukKw = 350;
      frontFlap = 18;
      rearFlap = 28;
      secuCycles = i % 10 === 0 ? 53 : 15;
    } else if (u < 0.80) {
      zoneName = "Variante Ascari Chicane";
      const k = (u - 0.72) / 0.08;
      baseSpeed = 341 - Math.sin(k * Math.PI * 0.9) * 165;
      throttlePct = 25;
      brakePct = 76;
      mgukKw = -290;
      frontFlap = 2;
      rearFlap = 5;
      aeroState = "Corner (High Downforce)";
      secuCycles = i % 10 === 0 ? 61 : 20;
    } else if (u < 0.91) {
      zoneName = "Back Straight into Parabolica";
      const k = (u - 0.80) / 0.11;
      baseSpeed = 220 + k * 124;
      throttlePct = 100;
      brakePct = 0;
      mgukKw = 335;
      frontFlap = 18;
      rearFlap = 28;
      secuCycles = i % 10 === 0 ? 52 : 15;
    } else {
      zoneName = "Curva Parabolica (Alboreto)";
      const k = (u - 0.91) / 0.09;
      baseSpeed = 344 - Math.sin(k * Math.PI * 0.75) * 135;
      throttlePct = 65;
      brakePct = 35;
      mgukKw = -140;
      frontFlap = 2;
      rearFlap = 4;
      aeroState = "Corner (High Downforce)";
      secuCycles = i % 10 === 0 ? 58 : 19;
    }

    // Apply profile adjustments
    let speedKmh = baseSpeed;
    if (profile === "eco_harvest") {
      speedKmh = baseSpeed * 0.972;
      if (mgukKw > 0) mgukKw *= 0.78;
    } else if (profile === "faulty_lockout") {
      speedKmh = baseSpeed * 0.935;
      frontFlap = 0;
      rearFlap = 0;
      aeroState = "Fail-Safe Locked";
      secuCycles += 14;
    }

    // Update battery state of charge smoothly
    soc = Math.max(18, Math.min(96, soc - (mgukKw / 350) * 0.34));

    const rivalSpeedKmh =
      baseSpeed *
      rival.speedScale *
      (brakePct > 20 ? rival.brakingGripScale : 1.0);

    const gapToRivalSec = (ourLapSec - rival.monzaQualiSec) * u;

    frames.push({
      idx: i,
      distanceM,
      trackX,
      trackY,
      zoneName,
      speedKmh: Math.round(speedKmh * 10) / 10,
      rivalSpeedKmh: Math.round(rivalSpeedKmh * 10) / 10,
      throttlePct,
      brakePct,
      mgukKw: Math.round(mgukKw),
      batterySocPct: Math.round(soc * 10) / 10,
      frontFlapDeg: frontFlap,
      rearFlapDeg: rearFlap,
      aeroState,
      secuCycles,
      gapToRivalSec: Math.round(gapToRivalSec * 1000) / 1000
    });
  }

  return frames;
}
