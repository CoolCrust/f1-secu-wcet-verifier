#include "ipet_solver.hpp"

#include <algorithm>
#include <cmath>
#include <map>
#include <set>
#include <unordered_map>
#include <utility>
#include <vector>

#include "Highs.h"

namespace secu::solver {

namespace {

struct SparseRowBuilder {
  std::vector<HighsInt> starts;
  std::vector<HighsInt> indices;
  std::vector<double> values;
  std::vector<double> lower;
  std::vector<double> upper;

  void addRow(const std::vector<std::pair<HighsInt, double>>& raw_terms,
              double lb,
              double ub) {
    // Merge duplicate column indices and sort ascending for HiGHS CSR compliance
    std::map<HighsInt, double> merged;
    for (const auto& [col, coeff] : raw_terms) {
      merged[col] += coeff;
    }

    starts.push_back(static_cast<HighsInt>(indices.size()));
    for (const auto& [col, coeff] : merged) {
      if (std::abs(coeff) > 1e-12) {
        indices.push_back(col);
        values.push_back(coeff);
      }
    }
    lower.push_back(lb);
    upper.push_back(ub);
  }

  void finalize(HighsLp& lp) {
    starts.push_back(static_cast<HighsInt>(indices.size()));
    lp.num_row_ = static_cast<HighsInt>(lower.size());
    lp.row_lower_ = std::move(lower);
    lp.row_upper_ = std::move(upper);
    lp.a_matrix_.format_ = MatrixFormat::kRowwise;
    lp.a_matrix_.start_ = std::move(starts);
    lp.a_matrix_.index_ = std::move(indices);
    lp.a_matrix_.value_ = std::move(values);
  }
};

constexpr double kBigFlowBound = 100000.0;

}  // namespace

FunctionIpetResult solveFunctionIpet(const nlohmann::json& fn_cfg_json,
                                     bool enable_mutex_pruning) {
  FunctionIpetResult res;
  res.mangled_name = fn_cfg_json.value("mangled_name", "");
  res.demangled_name = fn_cfg_json.value("demangled_name", "");
  res.loops_json = fn_cfg_json.value("loops", nlohmann::json::array());

  const auto& blocks_j = fn_cfg_json["blocks"];
  const auto& edges_j = fn_cfg_json["edges"];
  const auto& loops_j = fn_cfg_json["loops"];
  const uint32_t entry_id = fn_cfg_json.value("entry_block_id", 0U);

  std::set<uint32_t> exit_ids;
  for (const auto& eid : fn_cfg_json["exit_block_ids"]) {
    exit_ids.insert(eid.get<uint32_t>());
  }

  const std::size_t n_blocks = blocks_j.size();
  const std::size_t n_edges = edges_j.size();

  res.blocks.resize(n_blocks);
  for (std::size_t i = 0U; i < n_blocks; ++i) {
    const auto& bj = blocks_j[i];
    BlockExecutionResult b;
    b.id = bj.value("id", static_cast<uint32_t>(i));
    b.label = bj.value("label", "");
    b.cache_class = bj.value("cache_class", "AM");
    b.base_cycles = bj.value("base_cycles", 1U);
    b.first_iter_cycles = bj.value("first_iter_cycles", b.base_cycles);
    b.steady_iter_cycles = bj.value("steady_iter_cycles", b.base_cycles);
    b.mutex_group = bj.value("mutex_group", "");
    b.mutex_tag = bj.value("mutex_tag", "");
    res.blocks[i] = std::move(b);
  }

  res.edges.resize(n_edges);
  std::vector<std::vector<std::size_t>> in_edges(n_blocks);
  std::vector<std::vector<std::size_t>> out_edges(n_blocks);
  for (std::size_t e = 0U; e < n_edges; ++e) {
    const auto& ej = edges_j[e];
    EdgeExecutionResult edge;
    edge.src = ej.value("src", 0U);
    edge.dst = ej.value("dst", 0U);
    edge.is_backedge = ej.value("is_backedge", false);
    if (edge.src < n_blocks) {
      out_edges[edge.src].push_back(e);
    }
    if (edge.dst < n_blocks) {
      in_edges[edge.dst].push_back(e);
    }
    res.edges[e] = edge;
  }

  // Reject immediately if LLVM pass flagged any loop as unbounded
  if (!fn_cfg_json.value("wcet_provable", false) || n_blocks == 0U || exit_ids.empty()) {
    res.provable = false;
    res.solver_status = "UNBOUNDED_OR_INVALID_CFG";
    return res;
  }

  // Column layout:
  // [0 .. n_blocks - 1]                        -> x_i (block execution count)
  // [n_blocks .. n_blocks + n_edges - 1]       -> f_e (edge traversal flow)
  // [n_blocks + n_edges .. total_cols - 1]     -> z_fm (binary cold-miss indicator for FM blocks)
  std::vector<HighsInt> fm_col_for_block(n_blocks, -1);
  HighsInt next_col = static_cast<HighsInt>(n_blocks + n_edges);
  for (std::size_t i = 0U; i < n_blocks; ++i) {
    if (res.blocks[i].cache_class == "FM" &&
        res.blocks[i].first_iter_cycles > res.blocks[i].steady_iter_cycles) {
      fm_col_for_block[i] = next_col++;
    }
  }

  const HighsInt total_cols = next_col;
  HighsLp lp;
  lp.num_col_ = total_cols;
  lp.sense_ = ObjSense::kMaximize;
  lp.offset_ = 0.0;
  lp.col_cost_.assign(static_cast<std::size_t>(total_cols), 0.0);
  lp.col_lower_.assign(static_cast<std::size_t>(total_cols), 0.0);
  lp.col_upper_.assign(static_cast<std::size_t>(total_cols), kBigFlowBound);
  lp.integrality_.assign(static_cast<std::size_t>(total_cols), HighsVarType::kInteger);

  for (std::size_t i = 0U; i < n_blocks; ++i) {
    lp.col_cost_[i] = static_cast<double>(res.blocks[i].steady_iter_cycles);
    if (fm_col_for_block[i] >= 0) {
      const auto z_col = static_cast<std::size_t>(fm_col_for_block[i]);
      const uint32_t surcharge =
          res.blocks[i].first_iter_cycles - res.blocks[i].steady_iter_cycles;
      lp.col_cost_[z_col] = static_cast<double>(surcharge);
      lp.col_upper_[z_col] = 1.0;
    }
  }

  SparseRowBuilder rows;

  // 1. First-Miss coupling: z_i - x_i <= 0 (z_i can only be 1 if block i executes >= 1 time)
  for (std::size_t i = 0U; i < n_blocks; ++i) {
    if (fm_col_for_block[i] >= 0) {
      rows.addRow(
          {{fm_col_for_block[i], 1.0}, {static_cast<HighsInt>(i), -1.0}},
          -kHighsInf,
          0.0);
    }
  }

  // 2. Entry block flow conservation: sum(Out(entry)) - sum(In(entry)) = 1, x_entry - sum(Out(entry)) = 0
  {
    std::vector<std::pair<HighsInt, double>> net_entry;
    for (std::size_t e : out_edges[entry_id]) {
      net_entry.emplace_back(static_cast<HighsInt>(n_blocks + e), 1.0);
    }
    for (std::size_t e : in_edges[entry_id]) {
      net_entry.emplace_back(static_cast<HighsInt>(n_blocks + e), -1.0);
    }
    rows.addRow(net_entry, 1.0, 1.0);

    std::vector<std::pair<HighsInt, double>> x_entry_eq;
    x_entry_eq.emplace_back(static_cast<HighsInt>(entry_id), 1.0);
    for (std::size_t e : out_edges[entry_id]) {
      x_entry_eq.emplace_back(static_cast<HighsInt>(n_blocks + e), -1.0);
    }
    rows.addRow(x_entry_eq, 0.0, 0.0);
  }

  // 3. Internal & Exit block flow conservation
  for (std::size_t i = 0U; i < n_blocks; ++i) {
    if (i == entry_id) {
      continue;
    }
    std::vector<std::pair<HighsInt, double>> in_eq;
    in_eq.emplace_back(static_cast<HighsInt>(i), 1.0);
    for (std::size_t e : in_edges[i]) {
      in_eq.emplace_back(static_cast<HighsInt>(n_blocks + e), -1.0);
    }
    rows.addRow(in_eq, 0.0, 0.0);

    if (exit_ids.count(static_cast<uint32_t>(i)) == 0U) {
      std::vector<std::pair<HighsInt, double>> out_eq;
      out_eq.emplace_back(static_cast<HighsInt>(i), 1.0);
      for (std::size_t e : out_edges[i]) {
        out_eq.emplace_back(static_cast<HighsInt>(n_blocks + e), -1.0);
      }
      rows.addRow(out_eq, 0.0, 0.0);
    }
  }

  // 4. Total exit conservation: sum_{i in Exits} x_i = 1
  {
    std::vector<std::pair<HighsInt, double>> exit_sum;
    for (uint32_t eid : exit_ids) {
      exit_sum.emplace_back(static_cast<HighsInt>(eid), 1.0);
    }
    rows.addRow(exit_sum, 1.0, 1.0);
  }

  // 5. Loop back-edge trip-count bounds: sum(f_back) - B * sum(f_entry) <= 0
  for (const auto& lj : loops_j) {
    const uint32_t header = lj.value("header_block_id", 0U);
    const uint32_t max_iter = lj.value("max_iterations", 1U);
    std::set<uint32_t> members;
    for (const auto& mb : lj["member_blocks"]) {
      members.insert(mb.get<uint32_t>());
    }

    std::vector<std::pair<HighsInt, double>> loop_row;
    for (std::size_t e : in_edges[header]) {
      const uint32_t src = res.edges[e].src;
      const auto edge_col = static_cast<HighsInt>(n_blocks + e);
      if (members.count(src) > 0U || res.edges[e].is_backedge) {
        loop_row.emplace_back(edge_col, 1.0);
      } else {
        loop_row.emplace_back(edge_col, -static_cast<double>(max_iter));
      }
    }
    rows.addRow(loop_row, -kHighsInf, 0.0);
  }

  // 6. Mutually exclusive infeasible-path constraints (wcet_mutex:<group>:<tag>)
  if (enable_mutex_pruning) {
    std::map<std::string, std::map<std::string, uint32_t>> group_tag_rep;
    for (std::size_t i = 0U; i < n_blocks; ++i) {
      if (!res.blocks[i].mutex_group.empty() && !res.blocks[i].mutex_tag.empty()) {
        group_tag_rep[res.blocks[i].mutex_group].emplace(
            res.blocks[i].mutex_tag, static_cast<uint32_t>(i));
      }
    }
    for (const auto& [group_name, tag_map] : group_tag_rep) {
      (void)group_name;
      if (tag_map.size() > 1U) {
        std::vector<std::pair<HighsInt, double>> mutex_row;
        for (const auto& [tag_name, rep_block] : tag_map) {
          (void)tag_name;
          mutex_row.emplace_back(static_cast<HighsInt>(rep_block), 1.0);
        }
        rows.addRow(mutex_row, -kHighsInf, 1.0);
      }
    }
  }

  rows.finalize(lp);

  Highs highs;
  highs.setOptionValue("output_flag", false);
  if (highs.passModel(lp) != HighsStatus::kOk || highs.run() != HighsStatus::kOk) {
    res.provable = false;
    res.solver_status = "HIGHS_SOLVER_ERROR";
    return res;
  }

  const HighsModelStatus model_status = highs.getModelStatus();
  if (model_status != HighsModelStatus::kOptimal) {
    res.provable = false;
    res.solver_status = highs.modelStatusToString(model_status);
    return res;
  }

  const std::vector<double>& col_val = highs.getSolution().col_value;
  res.provable = true;
  res.solver_status = "OPTIMAL";
  res.wcet_cycles = static_cast<uint64_t>(
      std::llround(highs.getInfo().objective_function_value));

  for (std::size_t i = 0U; i < n_blocks; ++i) {
    const uint64_t count = static_cast<uint64_t>(std::llround(col_val[i]));
    res.blocks[i].exec_count = count;
    res.blocks[i].on_wcep = (count > 0U);
    if (count > 0U) {
      const uint64_t first = res.blocks[i].first_iter_cycles;
      const uint64_t steady = res.blocks[i].steady_iter_cycles;
      res.blocks[i].total_cycles_contributed = first + (count - 1U) * steady;
    }
  }

  for (std::size_t e = 0U; e < n_edges; ++e) {
    const uint64_t flow =
        static_cast<uint64_t>(std::llround(col_val[n_blocks + e]));
    res.edges[e].flow_count = flow;
    res.edges[e].on_wcep = (flow > 0U);
  }

  if (enable_mutex_pruning) {
    const FunctionIpetResult unpruned = solveFunctionIpet(fn_cfg_json, false);
    if (unpruned.provable && unpruned.wcet_cycles >= res.wcet_cycles) {
      res.unpruned_wcet_cycles = unpruned.wcet_cycles;
      res.infeasible_path_savings_cycles = unpruned.wcet_cycles - res.wcet_cycles;
    } else {
      res.unpruned_wcet_cycles = res.wcet_cycles;
    }
  } else {
    res.unpruned_wcet_cycles = res.wcet_cycles;
  }

  return res;
}

SystemCertificationReport certifySecuTaskSet(
    const std::vector<FunctionIpetResult>& solved_functions,
    const std::vector<RtaTaskSpec>& task_specs,
    double cpu_freq_mhz) {
  SystemCertificationReport cert;
  cert.cpu_freq_mhz = cpu_freq_mhz;

  std::vector<RtaTaskSpec> ordered = task_specs;
  std::sort(ordered.begin(), ordered.end(),
            [](const RtaTaskSpec& a, const RtaTaskSpec& b) {
              return a.priority < b.priority;
            });

  double total_u = 0.0;
  bool all_ok = !ordered.empty();

  for (std::size_t idx = 0U; idx < ordered.size(); ++idx) {
    const RtaTaskSpec& spec = ordered[idx];
    RtaTaskResult tr;
    tr.task_id = spec.task_id;
    tr.task_name = spec.task_name;
    tr.priority = spec.priority;
    tr.period_us = spec.period_us;
    tr.deadline_us = spec.deadline_us;

    const FunctionIpetResult* matched_fn = nullptr;
    for (const auto& fn : solved_functions) {
      if (fn.demangled_name.find(spec.function_match) != std::string::npos) {
        matched_fn = &fn;
        break;
      }
    }

    if (matched_fn == nullptr || !matched_fn->provable) {
      tr.demangled_function =
          (matched_fn != nullptr) ? matched_fn->demangled_name : spec.function_match;
      tr.schedulable = false;
      all_ok = false;
      cert.rta_tasks.push_back(std::move(tr));
      continue;
    }

    tr.demangled_function = matched_fn->demangled_name;
    tr.wcet_cycles = matched_fn->wcet_cycles;
    tr.wcet_us = static_cast<double>(tr.wcet_cycles) / cpu_freq_mhz;
    tr.utilization_percent = (tr.wcet_us / tr.period_us) * 100.0;
    total_u += (tr.wcet_us / tr.period_us);

    // Rate Monotonic fixed-point response time recurrence:
    // R_i^(k+1) = C_i + sum_{j in hp(i)} ceil(R_i^(k) / T_j) * C_j
    double r_prev = tr.wcet_us;
    bool converged = false;
    for (uint32_t iter = 0U; iter < 256U; ++iter) {
      double interference = 0.0;
      for (std::size_t hp = 0U; hp < idx; ++hp) {
        const RtaTaskResult& higher = cert.rta_tasks[hp];
        const double preemptions = std::ceil(r_prev / higher.period_us);
        interference += preemptions * higher.wcet_us;
      }
      const double r_next = tr.wcet_us + interference;
      if (r_next > tr.deadline_us) {
        r_prev = r_next;
        converged = false;
        break;
      }
      if (std::abs(r_next - r_prev) < 1e-9) {
        r_prev = r_next;
        converged = true;
        break;
      }
      r_prev = r_next;
    }

    tr.worst_response_us = r_prev;
    tr.slack_us = tr.deadline_us - tr.worst_response_us;
    tr.schedulable = converged && (tr.worst_response_us <= tr.deadline_us);
    if (!tr.schedulable) {
      all_ok = false;
    }
    cert.rta_tasks.push_back(std::move(tr));
  }

  const double n = static_cast<double>(ordered.size());
  cert.total_utilization_percent = total_u * 100.0;
  cert.liu_layland_bound_percent =
      (n > 0.0) ? (n * (std::pow(2.0, 1.0 / n) - 1.0) * 100.0) : 0.0;
  cert.all_deadlines_met = all_ok;
  cert.functions = solved_functions;
  return cert;
}

nlohmann::json toJsonReport(const SystemCertificationReport& cert,
                            const nlohmann::json& misra_report,
                            const std::string& analysis_mode) {
  nlohmann::json root;
  root["analysis_mode"] = analysis_mode;
  root["cpu_freq_mhz"] = cert.cpu_freq_mhz;
  root["misra_gatekeeper"] = misra_report;

  const bool misra_ok = misra_report.value("passed", false);
  root["overall_certified"] = misra_ok && cert.all_deadlines_met;

  root["rta_summary"] = {
      {"total_utilization_percent", cert.total_utilization_percent},
      {"liu_layland_bound_percent", cert.liu_layland_bound_percent},
      {"all_deadlines_met", cert.all_deadlines_met},
  };

  root["rta_tasks"] = nlohmann::json::array();
  for (const auto& t : cert.rta_tasks) {
    root["rta_tasks"].push_back({
        {"task_id", t.task_id},
        {"task_name", t.task_name},
        {"demangled_function", t.demangled_function},
        {"priority", t.priority},
        {"wcet_cycles", t.wcet_cycles},
        {"wcet_us", t.wcet_us},
        {"period_us", t.period_us},
        {"deadline_us", t.deadline_us},
        {"worst_response_us", t.worst_response_us},
        {"utilization_percent", t.utilization_percent},
        {"slack_us", t.slack_us},
        {"schedulable", t.schedulable},
    });
  }

  root["functions"] = nlohmann::json::array();
  for (const auto& fn : cert.functions) {
    nlohmann::json fj;
    fj["mangled_name"] = fn.mangled_name;
    fj["demangled_name"] = fn.demangled_name;
    fj["provable"] = fn.provable;
    fj["solver_status"] = fn.solver_status;
    fj["wcet_cycles"] = fn.wcet_cycles;
    fj["unpruned_wcet_cycles"] = fn.unpruned_wcet_cycles;
    fj["infeasible_path_savings_cycles"] = fn.infeasible_path_savings_cycles;
    fj["loops"] = fn.loops_json;

    fj["blocks"] = nlohmann::json::array();
    for (const auto& b : fn.blocks) {
      fj["blocks"].push_back({
          {"id", b.id},
          {"label", b.label},
          {"cache_class", b.cache_class},
          {"base_cycles", b.base_cycles},
          {"first_iter_cycles", b.first_iter_cycles},
          {"steady_iter_cycles", b.steady_iter_cycles},
          {"exec_count", b.exec_count},
          {"total_cycles_contributed", b.total_cycles_contributed},
          {"on_wcep", b.on_wcep},
          {"mutex_group", b.mutex_group},
          {"mutex_tag", b.mutex_tag},
      });
    }

    fj["edges"] = nlohmann::json::array();
    for (const auto& e : fn.edges) {
      fj["edges"].push_back({
          {"src", e.src},
          {"dst", e.dst},
          {"is_backedge", e.is_backedge},
          {"flow_count", e.flow_count},
          {"on_wcep", e.on_wcep},
      });
    }

    root["functions"].push_back(std::move(fj));
  }

  return root;
}

}  // namespace secu::solver
