#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

namespace secu::solver {

struct BlockExecutionResult {
  uint32_t id{0U};
  std::string label;
  std::string cache_class;
  uint32_t base_cycles{0U};
  uint32_t first_iter_cycles{0U};
  uint32_t steady_iter_cycles{0U};
  uint64_t exec_count{0U};
  uint64_t total_cycles_contributed{0U};
  bool on_wcep{false};  // True if part of the Worst-Case Execution Path ("Hot Red Path")
  std::string mutex_group;
  std::string mutex_tag;
};

struct EdgeExecutionResult {
  uint32_t src{0U};
  uint32_t dst{0U};
  bool is_backedge{false};
  uint64_t flow_count{0U};
  bool on_wcep{false};
};

struct FunctionIpetResult {
  std::string mangled_name;
  std::string demangled_name;
  bool provable{false};
  std::string solver_status;
  uint64_t wcet_cycles{0U};
  uint64_t unpruned_wcet_cycles{0U};
  uint64_t infeasible_path_savings_cycles{0U};
  std::vector<BlockExecutionResult> blocks;
  std::vector<EdgeExecutionResult> edges;
  nlohmann::json loops_json;
};

struct RtaTaskSpec {
  std::string task_id;
  std::string task_name;
  std::string function_match;
  uint32_t priority{1U};     // 1 = highest priority (Rate Monotonic)
  double period_us{1000.0};
  double deadline_us{1000.0};
};

struct RtaTaskResult {
  std::string task_id;
  std::string task_name;
  std::string demangled_function;
  uint32_t priority{1U};
  uint64_t wcet_cycles{0U};
  double wcet_us{0.0};
  double period_us{0.0};
  double deadline_us{0.0};
  double worst_response_us{0.0};
  double utilization_percent{0.0};
  double slack_us{0.0};
  bool schedulable{false};
};

struct SystemCertificationReport {
  double cpu_freq_mhz{400.0};
  double total_utilization_percent{0.0};
  double liu_layland_bound_percent{0.0};
  bool all_deadlines_met{false};
  std::vector<RtaTaskResult> rta_tasks;
  std::vector<FunctionIpetResult> functions;
};

FunctionIpetResult solveFunctionIpet(const nlohmann::json& fn_cfg_json,
                                     bool enable_mutex_pruning = true);

SystemCertificationReport certifySecuTaskSet(
    const std::vector<FunctionIpetResult>& solved_functions,
    const std::vector<RtaTaskSpec>& task_specs,
    double cpu_freq_mhz = 400.0);

nlohmann::json toJsonReport(const SystemCertificationReport& cert,
                            const nlohmann::json& misra_report,
                            const std::string& analysis_mode);

}  // namespace secu::solver
