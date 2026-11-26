#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "ipet_solver.hpp"

namespace fs = std::filesystem;

int main(int argc, char** argv) {
  std::string mode = "clean";
  std::string report_out = "analysis_report.json";

  for (int i = 1; i < argc; ++i) {
    const std::string arg = argv[i];
    if (arg.rfind("--mode=", 0U) == 0U) {
      mode = arg.substr(7U);
    } else if (arg.rfind("--out=", 0U) == 0U) {
      report_out = arg.substr(6U);
    }
  }

  const std::string src_dir = SECU_SOURCE_DIR;
  const std::string build_dir = SECU_BUILD_DIR;
  const std::string gatekeeper_bin = SECU_GATEKEEPER_BIN;
  const std::string wcet_plugin = SECU_WCET_PLUGIN;

  std::vector<std::string> target_sources = {
      src_dir + "/firmware/src/mguk_bbw_arbiter.cpp",
      src_dir + "/firmware/src/active_aero_sync.cpp",
      src_dir + "/firmware/src/telemetry_framer.cpp",
  };

  if (mode == "faulty") {
    target_sources.push_back(src_dir + "/firmware/faulty/faulty_unbounded_loop.cpp");
    target_sources.push_back(src_dir + "/firmware/faulty/faulty_heap_isr.cpp");
    target_sources.push_back(src_dir + "/firmware/faulty/faulty_narrowing_and_ub.cpp");
  }

  // Stage 1: MISRA C++:2023 Safety Gatekeeper
  const fs::path tmp = fs::temp_directory_path();
  const fs::path misra_json_path = tmp / ("secu_misra_" + mode + ".json");
  std::string gk_cmd =
      gatekeeper_bin + " -p " + build_dir + " --json-out=" + misra_json_path.string();
  for (const auto& s : target_sources) {
    gk_cmd += " " + s;
  }
  gk_cmd += " >/dev/null 2>&1";
  (void)std::system(gk_cmd.c_str());

  nlohmann::json misra_report;
  {
    std::ifstream in(misra_json_path);
    if (in.good()) {
      misra_report = nlohmann::json::parse(in);
    } else {
      misra_report = {{"passed", false}, {"violation_count", 1}, {"violations", nlohmann::json::array()}};
    }
  }

  // Stage 2 & 3: Lower each unit to LLVM IR -> run WCETPass -> solve IPET via HiGHS
  std::vector<secu::solver::FunctionIpetResult> solved_functions;
  for (std::size_t idx = 0U; idx < target_sources.size(); ++idx) {
    const std::string tag = "secu_unit_" + mode + "_" + std::to_string(idx);
    const fs::path ll_path = tmp / (tag + ".ll");
    const fs::path cfg_json_path = tmp / (tag + "_cfg.json");

    const std::string emit_cmd =
        "clang++-22 -std=c++20 -O1 -Xclang -disable-llvm-passes -fno-exceptions -fno-rtti -I" +
        src_dir + "/firmware/include -S -emit-llvm " + target_sources[idx] +
        " -o " + ll_path.string() + " >/dev/null 2>&1";
    const int emit_rc = std::system(emit_cmd.c_str());
    if (WEXITSTATUS(emit_rc) != 0) {
      continue;
    }

    const std::string opt_cmd =
        "SECU_WCET_CFG_OUT=" + cfg_json_path.string() +
        " opt-22 -load-pass-plugin=" + wcet_plugin +
        " -passes='module(function(mem2reg),cgscc(inline),function(mem2reg,loop-simplify,lcssa),secu-wcet)'"
        " -disable-output " + ll_path.string() + " >/dev/null 2>&1";
    const int opt_rc = std::system(opt_cmd.c_str());
    if (WEXITSTATUS(opt_rc) != 0) {
      continue;
    }

    std::ifstream cfg_in(cfg_json_path);
    if (!cfg_in.good()) {
      continue;
    }
    const nlohmann::json cfg_doc = nlohmann::json::parse(cfg_in);
    for (const auto& fn_j : cfg_doc["functions"]) {
      solved_functions.push_back(secu::solver::solveFunctionIpet(fn_j, true));
    }
  }

  // Stage 4: Rate Monotonic RTA Certifier for the 2026 F1 SECU Periodic Task Set
  std::vector<secu::solver::RtaTaskSpec> task_specs = {
      {"TASK_1_MGUK", "350kW MGU-K & BBW Arbiter (1 kHz)", "MgukBbwArbiter::step", 1U, 1000.0, 1000.0},
      {"TASK_2_AERO", "Active Aero Sync & Flap Guard (500 Hz)", "ActiveAeroController::step", 2U, 2000.0, 2000.0},
      {"TASK_3_TELEM", "FIA Telemetry & CRC16 Framer (100 Hz)", "TelemetryFramer::pack_and_enqueue", 3U, 10000.0, 10000.0},
  };
  if (mode == "faulty") {
    task_specs.push_back(
        {"TASK_4_CAN_POLL", "Faulty Unbounded CAN Mailbox Poll", "wait_for_can_mailbox_ready", 4U, 10000.0, 10000.0});
  }

  const secu::solver::SystemCertificationReport cert =
      secu::solver::certifySecuTaskSet(solved_functions, task_specs, 400.0);

  const nlohmann::json final_report =
      secu::solver::toJsonReport(cert, misra_report, mode);

  std::ofstream out(report_out);
  out << final_report.dump(2) << "\n";

  std::cout << "=== 2026 F1 SECU STATIC VERIFICATION REPORT (" << mode << ") ===\n";
  std::cout << "MISRA C++:2023 Gatekeeper : "
            << (misra_report.value("passed", false) ? "PASS" : "FAIL")
            << " (" << misra_report.value("violation_count", 0) << " violations)\n";
  for (const auto& t : cert.rta_tasks) {
    std::cout << "  [" << t.task_id << "] WCET=" << t.wcet_cycles
              << " cycles (" << t.wcet_us << " us) | WCRT=" << t.worst_response_us
              << " us / " << t.deadline_us << " us -> "
              << (t.schedulable ? "SCHEDULABLE" : "DEADLINE_FAIL") << "\n";
  }
  std::cout << "Total CPU Utilization: " << cert.total_utilization_percent
            << "% (Liu-Layland RM Bound: " << cert.liu_layland_bound_percent << "%)\n";
  std::cout << "Report exported to: " << report_out << "\n";

  return final_report.value("overall_certified", false) ? 0 : 1;
}
