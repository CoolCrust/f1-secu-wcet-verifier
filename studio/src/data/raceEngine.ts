import { GRID_2026_TEAMS } from "./grid2026";
import { FirmwareMode, TrackDefinition, WeatherMode } from "./tracks2026";

export type PitStrategyId = "one_stop_mh" | "two_stop_sms" | "overcut_hs" | "wet_adaptive";
export type TireCompound = "Soft" | "Medium" | "Hard" | "Inter" | "Wet";

export interface PitStrategySpec {
  id: PitStrategyId;
  label: string;
  description: string;
}

export const PIT_STRATEGIES: PitStrategySpec[] = [
  {
    id: "one_stop_mh",
    label: "1-Stop (Medium → Hard)",
    description: "Balanced pit loss with a long, consistent Hard-compound second stint."
  },
  {
    id: "two_stop_sms",
    label: "2-Stop Attack (Soft → Medium → Soft)",
    description: "Aggressive undercut pace on fresh rubber; strongest on high-wear tracks like Spa and Silverstone."
  },
  {
    id: "overcut_hs",
    label: "Overcut (Hard → Soft)",
    description: "Run deep into the race on Hard tires, then bolt on Softs with low fuel for a late charge."
  },
  {
    id: "wet_adaptive",
    label: "Smart Weather Adaptive",
    description: "Automatically boxes for Intermediates or Full Wets the moment track wetness crosses grip thresholds."
  }
];

export interface RaceCarStanding {
  id: string;
  isOurs: boolean;
  pos: number;
  team: string;
  driver: string;
  tire: TireCompound;
  pits: number;
  gapToLeaderSec: number;
  lastLapSec: number;
  status: "RUNNING" | "PITTING" | "LIMP_MODE" | "DNF";
}

export interface RaceLapSnapshot {
  lap: number;
  wetnessPct: number;
  ourPos: number;
  ourGapSec: number;
  ourLastLapSec: number;
  ourTire: TireCompound;
  ourTireHealthPct: number;
  ourPits: number;
  ourStatus: "RUNNING" | "PITTING" | "LIMP_MODE" | "DNF";
  faultAlert: string | null;
  standings: RaceCarStanding[];
}

export interface RaceEvent {
  lap: number;
  kind: "start" | "pit" | "weather" | "overtake" | "fault" | "finish";
  message: string;
}

export interface FullRaceResult {
  snapshots: RaceLapSnapshot[];
  events: RaceEvent[];
  startPos: number;
  finishPos: number;
  totalFaults: number;
  finishStatus: "FINISHED" | "LIMP_MODE" | "DNF";
}

interface SimCarState {
  id: string;
  isOurs: boolean;
  team: string;
  driver: string;
  basePaceRatio: number;
  tire: TireCompound;
  tireAge: number;
  totalTimeSec: number;
  lastLapSec: number;
  pits: number;
  pitLaps: number[];
  nextCompounds: TireCompound[];
  status: "RUNNING" | "PITTING" | "LIMP_MODE" | "DNF";
}

const COMPOUND_PACE_OFFSET: Record<TireCompound, number> = {
  Soft: -0.62,
  Medium: 0.0,
  Hard: 0.48,
  Inter: 2.8,
  Wet: 5.9
};

const COMPOUND_WEAR_PER_LAP: Record<TireCompound, number> = {
  Soft: 4.8,
  Medium: 3.1,
  Hard: 1.9,
  Inter: 2.7,
  Wet: 2.1
};

function getTrackWetness(weather: WeatherMode, lap: number, totalLaps: number): number {
  const frac = lap / totalLaps;
  if (weather === "dry") return 0;
  if (weather === "heavy_rain") {
    return Math.round(78 + Math.sin(frac * Math.PI * 3) * 14);
  }
  // Changing weather: dry start, passing rain shower between 36% and 74% race distance
  if (frac < 0.34) return 0;
  if (frac < 0.54) return Math.round(((frac - 0.34) / 0.2) * 82);
  if (frac < 0.74) return Math.round(82 - ((frac - 0.54) / 0.2) * 58);
  return Math.max(0, Math.round(24 - ((frac - 0.74) / 0.26) * 24));
}

function tireWeatherPenalty(tire: TireCompound, wetnessPct: number): number {
  const isSlick = tire === "Soft" || tire === "Medium" || tire === "Hard";
  if (wetnessPct < 15) {
    if (tire === "Inter") return 3.6;
    if (tire === "Wet") return 7.4;
    return 0;
  }
  if (wetnessPct < 60) {
    if (isSlick) return 3.8 + (wetnessPct - 15) * 0.14;
    if (tire === "Inter") return 0.2;
    return 2.6;
  }
  // Heavy standing water
  if (isSlick) return 14.5;
  if (tire === "Inter") return 2.9;
  return 0.0;
}

export function simulateFullRace(
  track: TrackDefinition,
  startPos: number,
  strategy: PitStrategyId,
  weather: WeatherMode,
  firmwareMode: FirmwareMode
): FullRaceResult {
  const totalLaps = track.laps;
  const clampedStart = Math.max(1, Math.min(22, startPos));

  // Build 21 rival entries (all 11 constructors: lead drivers + 10 teammates)
  const rivals: SimCarState[] = [];
  GRID_2026_TEAMS.forEach((t, idx) => {
    const leadRatio = t.monzaQualiSec / 81.786;
    rivals.push({
      id: `${t.id}_1`,
      isOurs: false,
      team: t.team,
      driver: t.leadDriver,
      basePaceRatio: leadRatio,
      tire: weather === "heavy_rain" ? "Wet" : "Medium",
      tireAge: 0,
      totalTimeSec: 0,
      lastLapSec: track.baseLapSec * leadRatio,
      pits: 0,
      pitLaps: [Math.round(totalLaps * (0.42 + (idx % 3) * 0.05))],
      nextCompounds: [weather === "heavy_rain" ? "Wet" : "Hard"],
      status: "RUNNING"
    });
    if (idx < 10) {
      rivals.push({
        id: `${t.id}_2`,
        isOurs: false,
        team: t.team,
        driver: `${t.team} Car #2`,
        basePaceRatio: leadRatio + 0.0045,
        tire: weather === "heavy_rain" ? "Wet" : "Hard",
        tireAge: 0,
        totalTimeSec: 0,
        lastLapSec: track.baseLapSec * (leadRatio + 0.0045),
        pits: 0,
        pitLaps: [Math.round(totalLaps * (0.50 + (idx % 3) * 0.04))],
        nextCompounds: [weather === "heavy_rain" ? "Wet" : "Medium"],
        status: "RUNNING"
      });
    }
  });

  // Sort rivals by qualifying pace and insert Our SECU Car at chosen startPos
  rivals.sort((a, b) => a.basePaceRatio - b.basePaceRatio);

  let ourStartTire: TireCompound = weather === "heavy_rain" ? "Wet" : "Medium";
  let ourPitLaps: number[] = [Math.round(totalLaps * 0.45)];
  let ourNextCompounds: TireCompound[] = ["Hard"];

  if (strategy === "two_stop_sms") {
    ourStartTire = weather === "heavy_rain" ? "Wet" : "Soft";
    ourPitLaps = [Math.round(totalLaps * 0.28), Math.round(totalLaps * 0.64)];
    ourNextCompounds = ["Medium", "Soft"];
  } else if (strategy === "overcut_hs") {
    ourStartTire = weather === "heavy_rain" ? "Wet" : "Hard";
    ourPitLaps = [Math.round(totalLaps * 0.62)];
    ourNextCompounds = ["Soft"];
  } else if (strategy === "wet_adaptive") {
    ourStartTire = weather === "heavy_rain" ? "Wet" : "Medium";
    ourPitLaps = weather === "dry" ? [Math.round(totalLaps * 0.46)] : [];
    ourNextCompounds = ["Hard"];
  }

  const ourCar: SimCarState = {
    id: "our_secu",
    isOurs: true,
    team: "SECU Prototype (Our Car)",
    driver: firmwareMode === "clean" ? "SECU Certified #26" : "SECU Faulty #26",
    basePaceRatio: firmwareMode === "clean" ? 1.0054 : 1.045,
    tire: ourStartTire,
    tireAge: 0,
    totalTimeSec: 0,
    lastLapSec: track.baseLapSec,
    pits: 0,
    pitLaps: ourPitLaps,
    nextCompounds: ourNextCompounds,
    status: "RUNNING"
  };

  const grid: SimCarState[] = [...rivals];
  grid.splice(clampedStart - 1, 0, ourCar);
  grid.forEach((car, gridIdx) => {
    // Initial staggered grid offset (~0.32s per starting slot)
    car.totalTimeSec = gridIdx * 0.32;
  });

  const events: RaceEvent[] = [
    {
      lap: 1,
      kind: "start",
      message: `Lights out at ${track.shortName} (${totalLaps} laps). Starting P${clampedStart} on ${ourCar.tire} tires (${firmwareMode === "clean" ? "Certified SECU" : "Faulty SECU"}).`
    }
  ];

  const snapshots: RaceLapSnapshot[] = [];
  let prevOurPos = clampedStart;
  let totalFaults = 0;
  let rainAnnounced = false;
  let dryAnnounced = false;

  const faultLap1 = Math.max(4, Math.round(totalLaps * 0.19));
  const faultLap2 = Math.max(8, Math.round(totalLaps * 0.48));
  const faultLap3 = Math.max(12, Math.round(totalLaps * 0.74));

  for (let lap = 1; lap <= totalLaps; lap++) {
    const wetness = getTrackWetness(weather, lap, totalLaps);
    const fuelBonusSec = -((lap / totalLaps) * 1.45);
    let lapFaultAlert: string | null = null;

    if (weather === "changing" && wetness >= 28 && !rainAnnounced) {
      rainAnnounced = true;
      events.push({
        lap,
        kind: "weather",
        message: `Rain shower hits ${track.shortName} (${wetness}% track wetness) — Wet-Weather Aero Fallback engaged.`
      });
    } else if (weather === "changing" && rainAnnounced && wetness < 14 && !dryAnnounced && lap > totalLaps * 0.65) {
      dryAnnounced = true;
      events.push({
        lap,
        kind: "weather",
        message: `Track drying out at ${track.shortName} (${wetness}% wetness) — crossover back to slicks.`
      });
    }

    for (let cIdx = 0; cIdx < grid.length; cIdx++) {
      const car = grid[cIdx];
      if (car.status === "DNF") {
        car.totalTimeSec += track.baseLapSec * 1.5;
        continue;
      }

      car.status = "RUNNING";
      car.tireAge += 1;

      // Check pit stop triggers
      let shouldPit = false;
      let nextTire: TireCompound | null = null;

      if (car.isOurs) {
        if (strategy === "wet_adaptive") {
          const isSlick = car.tire === "Soft" || car.tire === "Medium" || car.tire === "Hard";
          if (wetness >= 65 && car.tire !== "Wet") {
            shouldPit = true;
            nextTire = "Wet";
          } else if (wetness >= 24 && wetness < 65 && isSlick) {
            shouldPit = true;
            nextTire = "Inter";
          } else if (wetness < 14 && !isSlick) {
            shouldPit = true;
            nextTire = lap > totalLaps * 0.7 ? "Soft" : "Medium";
          } else if (car.pitLaps.includes(lap)) {
            shouldPit = true;
            nextTire = car.nextCompounds.shift() ?? "Hard";
          }
        } else if (car.pitLaps.includes(lap)) {
          shouldPit = true;
          nextTire = car.nextCompounds.shift() ?? "Hard";
        }
      } else {
        // Rival AI pit logic for rain or scheduled stop
        const isSlick = car.tire === "Soft" || car.tire === "Medium" || car.tire === "Hard";
        if (wetness >= 68 && car.tire !== "Wet" && lap % 2 === cIdx % 2) {
          shouldPit = true;
          nextTire = "Wet";
        } else if (wetness >= 28 && wetness < 68 && isSlick && lap % 2 === cIdx % 2) {
          shouldPit = true;
          nextTire = "Inter";
        } else if (wetness < 12 && !isSlick && lap > totalLaps * 0.68) {
          shouldPit = true;
          nextTire = "Medium";
        } else if (car.pitLaps.includes(lap)) {
          shouldPit = true;
          nextTire = car.nextCompounds.shift() ?? "Hard";
        }
      }

      const wearRate = COMPOUND_WEAR_PER_LAP[car.tire] * track.tireWearFactor;
      const wearPct = Math.min(100, car.tireAge * wearRate);
      const degPenalty = wearPct > 65 ? ((wearPct - 65) / 35) * 2.6 : (wearPct / 65) * 0.65;
      const weatherPen = tireWeatherPenalty(car.tire, wetness);

      // Deterministic slight lap variation
      const jitter = Math.sin(lap * 1.7 + cIdx * 0.9) * 0.14;
      let lapTime =
        track.baseLapSec * car.basePaceRatio +
        COMPOUND_PACE_OFFSET[car.tire] +
        degPenalty +
        weatherPen +
        fuelBonusSec +
        jitter;

      // Traffic / dirty air penalty scaled by track overtaking difficulty (Monaco is brutal!)
      if (car.isOurs && prevOurPos > 1) {
        const trafficFactor = (1 - track.overtakingEase) * 0.85;
        lapTime += trafficFactor;
      }

      if (shouldPit && nextTire) {
        car.status = "PITTING";
        car.pits += 1;
        car.tire = nextTire;
        car.tireAge = 0;
        lapTime += track.pitLossSec;
        if (car.isOurs) {
          events.push({
            lap,
            kind: "pit",
            message: `Box, box! Pit stop #${car.pits} completed (+${track.pitLossSec.toFixed(1)}s) — fitted fresh ${nextTire} tires.`
          });
        }
      }

      // Faulty Firmware Race Incidents (linked directly to static analysis violations)
      if (car.isOurs && firmwareMode === "faulty") {
        if (lap === faultLap1) {
          totalFaults += 1;
          lapTime += 4.2;
          lapFaultAlert = "SECU-WCET-001: Unbounded CAN Poll Watchdog Reset (+4.2s)";
          events.push({ lap, kind: "fault", message: `FAULT — ${lapFaultAlert}` });
        } else if (lap === faultLap2) {
          totalFaults += 1;
          lapTime += 3.1;
          car.status = "LIMP_MODE";
          lapFaultAlert = "MISRA-CPP-11.6.1: Active Aero Sensor Divergence -> Fail-Safe Drag Lockout";
          events.push({ lap, kind: "fault", message: `FAULT — ${lapFaultAlert}` });
        } else if (lap === faultLap3) {
          totalFaults += 1;
          if (weather === "heavy_rain") {
            car.status = "DNF";
            lapFaultAlert = "MISRA-CPP-21.6.1: ISR Heap Allocation Stall in Wet Braking — RETIRED (DNF)";
            events.push({ lap, kind: "fault", message: `CRITICAL — ${lapFaultAlert}` });
          } else {
            lapTime += 3.9;
            car.status = "LIMP_MODE";
            lapFaultAlert = "MISRA-CPP-21.6.1: Dynamic Heap Jitter in Brake-by-Wire ISR (+3.9s)";
            events.push({ lap, kind: "fault", message: `FAULT — ${lapFaultAlert}` });
          }
        }
      }

      car.lastLapSec = lapTime;
      car.totalTimeSec += lapTime;
    }

    // Rank all 22 cars by cumulative race time
    const sorted = [...grid].sort((a, b) => {
      if (a.status === "DNF" && b.status !== "DNF") return 1;
      if (b.status === "DNF" && a.status !== "DNF") return -1;
      return a.totalTimeSec - b.totalTimeSec;
    });

    const leaderTime = sorted[0].totalTimeSec;
    const standings: RaceCarStanding[] = sorted.map((car, idx) => ({
      id: car.id,
      isOurs: car.isOurs,
      pos: idx + 1,
      team: car.team,
      driver: car.driver,
      tire: car.tire,
      pits: car.pits,
      gapToLeaderSec: Math.max(0, car.totalTimeSec - leaderTime),
      lastLapSec: car.lastLapSec,
      status: car.status
    }));

    const ourRow = standings.find((s) => s.isOurs)!;
    if (ourRow.pos < prevOurPos && prevOurPos - ourRow.pos >= 2 && ourRow.status === "RUNNING") {
      events.push({
        lap,
        kind: "overtake",
        message: `Strong pace — climbed from P${prevOurPos} up to P${ourRow.pos} (${ourRow.tire} tires).`
      });
    }
    prevOurPos = ourRow.pos;

    const ourWear = Math.min(
      100,
      ourCar.tireAge * COMPOUND_WEAR_PER_LAP[ourCar.tire] * track.tireWearFactor
    );

    snapshots.push({
      lap,
      wetnessPct: wetness,
      ourPos: ourRow.pos,
      ourGapSec: ourRow.gapToLeaderSec,
      ourLastLapSec: ourRow.lastLapSec,
      ourTire: ourCar.tire,
      ourTireHealthPct: Math.max(0, Math.round(100 - ourWear)),
      ourPits: ourCar.pits,
      ourStatus: ourRow.status,
      faultAlert: lapFaultAlert,
      standings
    });
  }

  const finalSnap = snapshots[snapshots.length - 1];
  events.push({
    lap: totalLaps,
    kind: "finish",
    message:
      finalSnap.ourStatus === "DNF"
        ? `Checkered flag — Car retired (DNF) due to faulty firmware stall.`
        : `Checkered flag at ${track.shortName}! Finished P${finalSnap.ourPos} (Started P${clampedStart}, ${finalSnap.ourPits} pit stop${finalSnap.ourPits === 1 ? "" : "s"}).`
  });

  return {
    snapshots,
    events,
    startPos: clampedStart,
    finishPos: finalSnap.ourPos,
    totalFaults,
    finishStatus:
      finalSnap.ourStatus === "DNF"
        ? "DNF"
        : firmwareMode === "faulty"
        ? "LIMP_MODE"
        : "FINISHED"
  };
}
