#include <gtest/gtest.h>

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>

#include <nlohmann/json.hpp>

namespace fs = std::filesystem;

namespace {

int runGatekeeper(const std::vector<std::string>& source_files,
                  const fs::path& json_out,
                  const std::string& extra_args = "") {
  std::string cmd = std::string(GATEKEEPER_BIN_PATH) + " -p " + BUILD_DIR_PATH +
                    " --json-out=" + json_out.string();
  if (!extra_args.empty()) {
    cmd += " " + extra_args;
  }
  for (const auto& src : source_files) {
    cmd += " " + src;
  }
  cmd += " >/dev/null 2>&1";
  const int rc = std::system(cmd.c_str());
  return WEXITSTATUS(rc);
}

bool hasRule(const nlohmann::json& report, const std::string& rule_id) {
  for (const auto& item : report["violations"]) {
    if (item["rule_id"] == rule_id) {
      return true;
    }
  }
  return false;
}

}  // namespace

TEST(MisraGatekeeperTest, CleanFirmwarePassesWithZeroViolations) {
  const fs::path report_path = fs::temp_directory_path() / "secu_clean_misra.json";
  const std::string src_dir = SOURCE_DIR_PATH;

  const int exit_code = runGatekeeper(
      {src_dir + "/firmware/src/mguk_bbw_arbiter.cpp",
       src_dir + "/firmware/src/active_aero_sync.cpp",
       src_dir + "/firmware/src/telemetry_framer.cpp"},
      report_path);

  EXPECT_EQ(exit_code, 0);
  std::ifstream in(report_path);
  const nlohmann::json report = nlohmann::json::parse(in);
  EXPECT_TRUE(report["passed"].get<bool>());
  EXPECT_EQ(report["violation_count"].get<std::size_t>(), 0U);
}

TEST(MisraGatekeeperTest, CatchesFaultyFirmwareVariants) {
  const std::string src_dir = SOURCE_DIR_PATH;

  // 1. Unbounded hardware polling loop
  {
    const fs::path p = fs::temp_directory_path() / "fault_loop.json";
    const int rc = runGatekeeper({src_dir + "/firmware/faulty/faulty_unbounded_loop.cpp"}, p);
    EXPECT_EQ(rc, 1);
    std::ifstream in(p);
    const nlohmann::json r = nlohmann::json::parse(in);
    EXPECT_TRUE(hasRule(r, "SECU-WCET-001"));
  }

  // 2. Dynamic heap allocation in 1 kHz ISR
  {
    const fs::path p = fs::temp_directory_path() / "fault_heap.json";
    const int rc = runGatekeeper({src_dir + "/firmware/faulty/faulty_heap_isr.cpp"}, p);
    EXPECT_EQ(rc, 1);
    std::ifstream in(p);
    const nlohmann::json r = nlohmann::json::parse(in);
    EXPECT_TRUE(hasRule(r, "MISRA-CPP-21.6.1"));
  }

  // 3. Uninitialized variable, implicit narrowing, goto, and throw
  {
    const fs::path p = fs::temp_directory_path() / "fault_ub.json";
    const int rc = runGatekeeper({src_dir + "/firmware/faulty/faulty_narrowing_and_ub.cpp"}, p);
    EXPECT_EQ(rc, 1);
    std::ifstream in(p);
    const nlohmann::json r = nlohmann::json::parse(in);
    EXPECT_TRUE(hasRule(r, "MISRA-CPP-11.6.1"));
    EXPECT_TRUE(hasRule(r, "MISRA-CPP-8.0.1"));
    EXPECT_TRUE(hasRule(r, "MISRA-CPP-9.6.1"));
    EXPECT_TRUE(hasRule(r, "MISRA-CPP-18.1.1"));
  }
}

TEST(MisraGatekeeperTest, CatchesMutualRecursionCycle) {
  const fs::path tmp_cpp = fs::temp_directory_path() / "mutual_recursion_probe.cpp";
  const fs::path report_path = fs::temp_directory_path() / "mutual_recursion.json";
  {
    std::ofstream out(tmp_cpp);
    out << "int beta(int x);\n"
        << "int alpha(int x) { return x > 0 ? beta(x - 1) : 0; }\n"
        << "int beta(int x) { return alpha(x - 1); }\n";
  }

  const int rc = runGatekeeper({tmp_cpp.string()}, report_path, "--extra-arg=-std=c++20");
  EXPECT_EQ(rc, 1);
  std::ifstream in(report_path);
  const nlohmann::json r = nlohmann::json::parse(in);
  EXPECT_TRUE(hasRule(r, "MISRA-CPP-17.2.1"));
}
