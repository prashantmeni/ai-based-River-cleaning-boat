import {
  DIAG_CMD,
  type NavState,
  type Telemetry,
} from "../services/communication";
import { store, type SourceMode } from "../services/communication/store";

export type DemoScenario = "mission" | "fault" | "comms_loss";

export const DEMO_SCENARIOS: { id: DemoScenario; label: string; blurb: string }[] = [
  {
    id: "mission",
    label: "Normal mission",
    blurb: "Search → detect → approach → collect loop with GPS and sensors.",
  },
  {
    id: "fault",
    label: "Fault & obstacle",
    blurb: "Obstacle avoidance and a latched sensor fault mid-mission.",
  },
  {
    id: "comms_loss",
    label: "CAM link loss",
    blurb: "ESP-NOW link drops after 12 s; recover with E-STOP clear.",
  },
];

const TICK_MS = 500;
/** Placeholder coordinates, clearly synthetic (SIMULATION label in UI). */
const DEMO_LAT = 17.385;
const DEMO_LON = 78.4867;
const CLASS_POOL = [1, 2, 3];

type Phase = { name: NavState; ticksLeft: number };

/**
 * Synthetic telemetry generator. Produces EXACT Telemetry objects through the
 * same store as live data (source = "demo"), so every page renders real code
 * paths with obviously-labelled simulated data. Never mixes into live source.
 */
export class DemoEngine {
  private scenario: DemoScenario = "mission";
  private timer: ReturnType<typeof setInterval> | null = null;
  private t: Telemetry;
  private phase: Phase = { name: "IDLE", ticksLeft: 6 };
  private elapsedTicks = 0;
  private driveTicksLeft = 0;
  private driveCmd: number = 0;
  private manualPwm = 0;
  private obstacleInjected = false;
  private sensorFaultInjected = false;
  private commsDown = false;

  constructor() {
    this.t = DemoEngine.freshTelemetry();
  }

  static freshTelemetry(): Telemetry {
    return {
      type: "telemetry",
      protocol_version: 1,
      device_id: "riverclean_main",
      state: "IDLE",
      comms_ok: true,
      estop: false,
      autonomous: false,
      detection: false,
      confidence: 0,
      class_id: 0,
      center_x: 48,
      left_cm: 140,
      right_cm: 138,
      gps_fix: true,
      latitude: DEMO_LAT,
      longitude: DEMO_LON,
      satellites: 11,
      gps_age_ms: 400,
      collection_count: 0,
      conveyor: "OFF",
      fault_flags: 0,
      uptime_ms: 65_000,
      left_pwm: 0,
      right_pwm: 0,
      left_dir: 0,
      right_dir: 0,
      rx_bytes: 40_000,
      rx_errors: 0,
      perr: 0,
      drops: 0,
      dbg: "",
      bottle_count: 0,
      bag_count: 0,
      debris_count: 0,
      tds_ppm: 128,
      tds_mv: 730,
    };
  }

  isRunning(): boolean {
    return this.timer !== null;
  }

  start(scenario: DemoScenario): void {
    this.stop();
    this.scenario = scenario;
    this.t = DemoEngine.freshTelemetry();
    this.phase = { name: "IDLE", ticksLeft: 6 };
    this.elapsedTicks = 0;
    this.driveTicksLeft = 0;
    this.driveCmd = 0;
    this.manualPwm = 0;
    this.obstacleInjected = false;
    this.sensorFaultInjected = false;
    this.commsDown = false;
    store.setSource("demo", Date.now());
    store.clearSeries();
    store.pushTelemetry(this.t, "demo", Date.now());
    this.timer = setInterval(() => this.tick(), TICK_MS);
  }

  stop(): void {
    if (this.timer) clearInterval(this.timer);
    this.timer = null;
    store.setSource("live", Date.now());
  }

  getScenario(): DemoScenario {
    return this.scenario;
  }

  /** Advance exactly one 500 ms tick — used by tests for determinism. */
  step(): void {
    if (this.timer) this.tick();
  }

  /**
   * Simulate a firmware command. Mirrors nav safety rules: no drive while
   * latched, manual drive only in non-autonomous modes, E-STOP latch/clear.
   */
  handleCommand(cmd: number, value: number, durationMs: number): { ok: boolean; reason?: string } {
    const t = this.t;
    if (cmd === DIAG_CMD.STOP) {
      if (value === 1) {
        t.estop = true;
        this.setPhase("EMERGENCY_STOP", 9999);
        this.stopMotors();
        t.conveyor = "OFF";
        return { ok: true };
      }
      if (value === 2) {
        t.estop = false;
        t.fault_flags = 0;
        t.comms_ok = !this.commsDown;
        if (this.commsDown) this.commsDown = false;
        if (t.state === "FAULT" || t.state === "EMERGENCY_STOP") {
          this.setPhase("IDLE", 4);
        }
        return { ok: true };
      }
      this.stopMotors();
      t.conveyor = "OFF";
      return { ok: true };
    }
    if (t.estop || t.state === "EMERGENCY_STOP" || t.state === "FAULT") {
      return { ok: false, reason: "state latched" };
    }
    switch (cmd) {
      case DIAG_CMD.FORWARD:
      case DIAG_CMD.TURN_LEFT:
      case DIAG_CMD.TURN_RIGHT: {
        if (t.autonomous) return { ok: false, reason: "autonomous mode" };
        this.manualPwm = Math.max(0, Math.min(255, value));
        this.driveCmd = cmd;
        this.driveTicksLeft = Math.max(1, Math.round((durationMs || 1500) / TICK_MS));
        return { ok: true };
      }
      case DIAG_CMD.CONVEYOR_ON: {
        if (t.autonomous) return { ok: false, reason: "autonomous mode" };
        t.conveyor = "RUNNING";
        return { ok: true };
      }
      case DIAG_CMD.CONVEYOR_OFF: {
        t.conveyor = "OFF";
        return { ok: true };
      }
      case DIAG_CMD.AUTONOMOUS_ON: {
        t.autonomous = true;
        this.setPhase("SEARCH", 8);
        return { ok: true };
      }
      case DIAG_CMD.AUTONOMOUS_OFF: {
        t.autonomous = false;
        this.stopMotors();
        t.conveyor = "OFF";
        this.setPhase("IDLE", 4);
        return { ok: true };
      }
      default:
        return { ok: false, reason: "unknown command" };
    }
  }

  private stopMotors(): void {
    this.t.left_pwm = 0;
    this.t.right_pwm = 0;
    this.t.left_dir = 0;
    this.t.right_dir = 0;
    this.driveTicksLeft = 0;
  }

  private setPhase(name: NavState, ticks: number): void {
    this.phase = { name, ticksLeft: ticks };
    this.t.state = name;
  }

  private tick(): void {
    const t = this.t;
    this.elapsedTicks += 1;
    t.uptime_ms += TICK_MS;
    t.rx_bytes += this.commsDown ? 0 : 220;

    if (this.scenario === "comms_loss") {
      if (this.elapsedTicks === 24 && !this.commsDown) {
        this.commsDown = true;
        t.comms_ok = false;
        t.fault_flags |= 1; // FAULT_COMMS_LOST
        t.state = "FAULT";
        this.phase = { name: "FAULT", ticksLeft: 9999 };
      }
    }

    if (this.scenario === "fault") {
      if (this.elapsedTicks === 14 && !this.obstacleInjected) {
        this.obstacleInjected = true;
        t.fault_flags |= 2; // FAULT_OBSTACLE
        this.setPhase("OBSTACLE_AVOID", 6);
      }
      if (this.elapsedTicks === 40 && !this.sensorFaultInjected) {
        this.sensorFaultInjected = true;
        t.fault_flags |= 4; // FAULT_SENSOR_BAD
      }
    }

    if (t.estop) {
      this.stopMotors();
      t.conveyor = "OFF";
    } else if (t.state === "FAULT") {
      this.stopMotors();
      t.conveyor = "OFF";
    } else if (t.autonomous) {
      this.runAutonomousMission();
    } else {
      this.runManualDrive();
    }

    this.sensors();
    store.pushTelemetry({ ...t }, "demo", Date.now());
  }

  private runManualDrive(): void {
    const t = this.t;
    if (this.driveTicksLeft > 0) {
      this.driveTicksLeft -= 1;
      t.left_pwm = this.manualPwm;
      t.right_pwm = this.manualPwm;
      t.left_dir = this.driveCmd === DIAG_CMD.TURN_LEFT ? -1 : 1;
      t.right_dir = this.driveCmd === DIAG_CMD.TURN_RIGHT ? -1 : 1;
      if (this.driveTicksLeft === 0) this.stopMotors();
    } else {
      this.stopMotors();
    }
  }

  private runAutonomousMission(): void {
    const t = this.t;
    this.phase.ticksLeft -= 1;
    if (this.phase.ticksLeft <= 0) this.advancePhase();

    switch (t.state) {
      case "SEARCH":
        t.left_pwm = 70;
        t.right_pwm = 70;
        t.left_dir = 1;
        t.right_dir = 1;
        t.detection = false;
        break;
      case "TARGET_DETECTED":
        t.detection = true;
        t.left_pwm = 55;
        t.right_pwm = 55;
        break;
      case "ALIGN":
        t.detection = true;
        t.center_x = Math.max(46, t.center_x - 4);
        t.left_pwm = t.center_x > 48 ? 45 : 65;
        t.right_pwm = t.center_x > 48 ? 65 : 45;
        break;
      case "APPROACH":
        t.detection = true;
        t.left_pwm = 90;
        t.right_pwm = 90;
        t.left_cm = Math.max(28, t.left_cm - 18);
        t.right_cm = Math.max(28, t.right_cm - 18);
        break;
      case "COLLECT":
        t.detection = true;
        t.conveyor = "RUNNING";
        t.left_pwm = 50;
        t.right_pwm = 50;
        break;
      case "VERIFY_COLLECT":
        t.conveyor = "OFF";
        t.left_pwm = 0;
        t.right_pwm = 0;
        break;
      case "OBSTACLE_AVOID":
        t.left_pwm = this.phase.ticksLeft % 2 === 0 ? 80 : 0;
        t.right_pwm = 0;
        t.left_cm = 18;
        t.right_cm = 18;
        break;
      case "IDLE":
      default:
        this.stopMotors();
        break;
    }
  }

  private advancePhase(): void {
    const t = this.t;
    switch (t.state) {
      case "IDLE":
        this.setPhase("SEARCH", 8);
        break;
      case "SEARCH":
        this.setPhase("TARGET_DETECTED", 4);
        t.detection = true;
        t.class_id = CLASS_POOL[Math.floor(Math.random() * CLASS_POOL.length)];
        t.confidence = 72 + Math.floor(Math.random() * 24);
        t.center_x = 62;
        break;
      case "TARGET_DETECTED":
        this.setPhase("ALIGN", 5);
        break;
      case "ALIGN":
        this.setPhase("APPROACH", 8);
        t.center_x = 48;
        break;
      case "APPROACH":
        this.setPhase("COLLECT", 10);
        break;
      case "COLLECT":
        this.setPhase("VERIFY_COLLECT", 6);
        break;
      case "VERIFY_COLLECT":
        t.collection_count += 1;
        if (t.class_id === 1) t.bottle_count = (t.bottle_count ?? 0) + 1;
        else if (t.class_id === 2) t.bag_count = (t.bag_count ?? 0) + 1;
        else if (t.class_id === 3) t.debris_count = (t.debris_count ?? 0) + 1;
        t.left_cm = 140;
        t.right_cm = 138;
        this.setPhase("SEARCH", 8);
        t.detection = false;
        break;
      case "OBSTACLE_AVOID":
        if (this.obstacleInjected) {
          t.fault_flags &= ~2;
          this.obstacleInjected = false;
        }
        this.setPhase("APPROACH", 6);
        break;
      default:
        this.setPhase("SEARCH", 8);
        break;
    }
  }

  private sensors(): void {
    const t = this.t;
    if (!t.detection) {
      t.confidence = 0;
      t.class_id = 0;
      t.center_x = 48;
    }
    t.gps_age_ms = 350 + ((this.elapsedTicks * 137) % 500);
    t.satellites = 9 + ((this.elapsedTicks >> 3) % 4);
    if (t.state === "IDLE" || t.state === "BOOT" || t.state === "SELF_TEST") {
      if (Math.abs(t.left_cm - 140) > 1) {
        t.left_cm = 140;
        t.right_cm = 138;
      }
    } else if (t.state === "SEARCH" || t.state === "TARGET_DETECTED") {
      t.left_cm = 95 + ((this.elapsedTicks * 7) % 30);
      t.right_cm = 92 + ((this.elapsedTicks * 11) % 33);
    }
    t.latitude = DEMO_LAT + Math.sin(this.elapsedTicks / 40) * 0.00012;
    t.longitude = DEMO_LON + Math.cos(this.elapsedTicks / 37) * 0.00012;
    if (this.commsDown) {
      t.comms_ok = false;
    } else if (this.scenario !== "fault" || !this.sensorFaultInjected) {
      t.comms_ok = true;
    }
  }
}

export const demoEngine = new DemoEngine();
export const DEMO_SOURCE: SourceMode = "demo";
