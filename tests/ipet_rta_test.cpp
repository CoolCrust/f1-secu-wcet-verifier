#include <gtest/gtest.h>

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>

#include <nlohmann/json.hpp>

#include "ipet_solver.hpp"

namespace fs = std::filesystem;

TEST(IpetSolverTest, InfeasiblePathPruningReducesActiveAeroWcet) {
  // Synthetic diamond CFG with two mutually exclusive branches (Straight vs Emergency)
  nlohmann::json cfg = {
      {"mangled_name", "_ZN4secu20ActiveAeroController4stepEv"},
      {"demangled_name", "secu::ActiveAeroController::step()"},
      {"entry_block_id", 0},
      {"exit_block_ids", {3}},
      {"wcet_provable", true},
      {"loops", nlohmann::json::array()},
      {"blocks", {
          {{"id", 0}, {"label", "entry"}, {"cache_class", "AM"},
           {"base_cycles", 10}, {"first_iter_cycles", 16}, {"steady_iter_cycles", 16},
           {"mutex_group", ""}, {"mutex_tag", ""}},
          {{"id", 1}, {"label", "straight_branch"}, {"cache_class", "AH"},
           {"base_cycles", 40}, {"first_iter_cycles", 40}, {"steady_iter_cycles", 40},
           {"mutex_group", "aero_mode"}, {"mutex_tag", "straight"}},
          {{"id", 2}, {"label", "emergency_branch"}, {"cache_class", "AH"},
           {"base_cycles", 25}, {"first_iter_cycles", 25}, {"steady_iter_cycles", 25},
           {"mutex_group", "aero_mode"}, {"mutex_tag", "emergency"}},
          {{"id", 3}, {"label", "exit"}, {"cache_class", "AH"},
           {"base_cycles", 10}, {"first_iter_cycles", 10}, {"steady_iter_cycles", 10},
           {"mutex_group", ""}, {"mutex_tag", ""}},
      }},
      {"edges", {
          {{"src", 0}, {"dst", 1}, {"is_backedge", false}},
          {{"src", 0}, {"dst", 2}, {"is_backedge", false}},
          {{"src", 1}, {"dst", 3}, {"is_backedge", false}},
          {{"src", 2}, {"dst", 3}, {"is_backedge", false}},
      }},
  };

  const secu::solver::FunctionIpetResult res = secu::solver::solveFunctionIpet(cfg, true);
  ASSERT_TRUE(res.provable);
  // Longest feasible path: entry (16) + straight (40) + exit (10) = 66 cycles
  EXPECT_EQ(res.wcet_cycles, 66U);
  EXPECT_EQ(res.blocks[1].exec_count, 1U);
  EXPECT_EQ(res.blocks[2].exec_count, 0U);
  EXPECT_TRUE(res.blocks[1].on_wcep);
  EXPECT_FALSE(res.blocks[2].on_wcep);
}

TEST(RtaCertifierTest, DetectsDeadlineOverrunUnderHeavyPreemption) {
  secu::solver::FunctionIpetResult t1;
  t1.demangled_name = "MgukBbwArbiter::step";
  t1.provable = true;
  t1.wcet_cycles = 240000U;  // 600 us at 400 MHz (60% CPU in 1000 us period)

  secu::solver::FunctionIpetResult t2;
  t2.demangled_name = "ActiveAeroController::step";
  t2.provable = true;
  t2.wcet_cycles = 360000U;  // 900 us at 400 MHz -> suffers 2 preemptions from T1 -> 2100 us > 2000 us

  const std::vector<secu::solver::RtaTaskSpec> specs = {
      {"T1", "MGU-K", "MgukBbwArbiter::step", 1U, 1000.0, 1000.0},
      {"T2", "Aero", "ActiveAeroController::step", 2U, 2000.0, 2000.0},
  };

  const auto cert = secu::solver::certifySecuTaskSet({t1, t2}, specs, 400.0);
  EXPECT_TRUE(cert.rta_tasks[0].schedulable);
  EXPECT_FALSE(cert.rta_tasks[1].schedulable);
  EXPECT_FALSE(cert.all_deadlines_met);
}

TEST(EndToEndVerifierTest, CleanPipelineCertifiesAndExportsJson) {
  const fs::path out_json = fs::temp_directory_path() / "e2e_clean_report.json";
  const std::string cmd = std::string(VERIFY_CLI_BIN_PATH) +
                          " --mode=clean --out=" + out_json.string() + " >/dev/null 2>&1";
  const int rc = std::system(cmd.c_str());
  EXPECT_EQ(WEXITSTATUS(rc), 0);

  std::ifstream in(out_json);
  const nlohmann::json rep = nlohmann::json::parse(in);
  EXPECT_TRUE(rep["overall_certified"].get<bool>());
  EXPECT_TRUE(rep["rta_summary"]["all_deadlines_met"].get<bool>());
  EXPECT_EQ(rep["rta_tasks"].size(), 3U);
}
