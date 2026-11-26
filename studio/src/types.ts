export interface MisraViolation {
  file: string;
  line: number;
  column: number;
  rule_id: string;
  category: string;
  message: string;
}

export interface MisraReport {
  passed: boolean;
  violation_count: number;
  analyzed_files: string[];
  violations: MisraViolation[];
}

export interface RtaTask {
  task_id: string;
  task_name: string;
  demangled_function: string;
  priority: number;
  wcet_cycles: number;
  wcet_us: number;
  period_us: number;
  deadline_us: number;
  worst_response_us: number;
  utilization_percent: number;
  slack_us: number;
  schedulable: boolean;
}

export interface BasicBlockNode {
  id: number;
  label: string;
  cache_class: "AH" | "FM" | "AM";
  base_cycles: number;
  first_iter_cycles: number;
  steady_iter_cycles: number;
  exec_count: number;
  total_cycles_contributed: number;
  on_wcep: boolean;
  mutex_group: string;
  mutex_tag: string;
}

export interface CfgEdge {
  src: number;
  dst: number;
  is_backedge: boolean;
  flow_count: number;
  on_wcep: boolean;
}

export interface LoopInfo {
  loop_id: number;
  header_block_id: number;
  max_iterations: number;
  bounded: boolean;
  bound_source: string;
}

export interface FunctionAnalysis {
  mangled_name: string;
  demangled_name: string;
  provable: boolean;
  solver_status: string;
  wcet_cycles: number;
  unpruned_wcet_cycles: number;
  infeasible_path_savings_cycles: number;
  blocks: BasicBlockNode[];
  edges: CfgEdge[];
  loops: LoopInfo[];
}

export interface AnalysisReport {
  analysis_mode: string;
  cpu_freq_mhz: number;
  overall_certified: boolean;
  misra_gatekeeper: MisraReport;
  rta_summary: {
    total_utilization_percent: number;
    liu_layland_bound_percent: number;
    all_deadlines_met: boolean;
  };
  rta_tasks: RtaTask[];
  functions: FunctionAnalysis[];
}

export interface QemuTickSample {
  tick: number;
  segment: string;
  task1_cycles: number;
  task2_cycles: number;
  task3_cycles: number;
  total_tick_cycles: number;
  mguk_power_kw: number;
  lap_harvest_kj: number;
  aero_mode: number;
  fault_bits: number;
}

export interface IrBasicBlock {
  id: number;
  label: string;
  lines: string[];
}

export interface ArmDisasmEntry {
  symbol: string;
  address: string;
  instruction_count: number;
  estimated_thumb_cycles?: number;
  lines: string[];
}

export interface DisassemblyBundle {
  toolchain: Record<string, string>;
  artifact_hashes_sha256: Record<string, string>;
  ir_by_mangled: Record<string, IrBasicBlock[]>;
  arm_disasm: Record<string, ArmDisasmEntry>;
}

export interface McuUartFrame {
  source_port: string;
  is_emulated: boolean;
  mcu_model: string;
  clock_mhz: number;
  tick: number;
  t1_dwt_cycles: number;
  t2_dwt_cycles: number;
  t3_dwt_cycles: number;
  total_dwt_cycles: number;
  icache_hits: number;
  icache_misses: number;
  flash_stall_cycles: number;
  pipeline_dual_issue_saved: number;
  watchdog_fault: boolean;
  raw_uart_line: string;
}

export interface ExportBundleResult {
  bundle_sha256: string;
  json_path: string;
  html_path: string;
  timestamp_unix: number;
  mode: string;
}
