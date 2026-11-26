#include <gtest/gtest.h>

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>

#include <nlohmann/json.hpp>

namespace fs = std::filesystem;

namespace {

nlohmann::json runWcetPassOnCpp(const std::string& cpp_file, const std::string& tag) {
  const fs::path tmp = fs::temp_directory_path();
  const fs::path ll_path = tmp / (tag + ".ll");
  const fs::path json_path = tmp / (tag + ".json");

  const std::string emit_cmd =
      "clang++-22 -std=c++20 -O1 -Xclang -disable-llvm-passes -fno-exceptions -fno-rtti -I" +
      std::string(SOURCE_DIR_PATH) + "/firmware/include -S -emit-llvm " +
      cpp_file + " -o " + ll_path.string();
  const int emit_rc = std::system(emit_cmd.c_str());
  EXPECT_EQ(WEXITSTATUS(emit_rc), 0) << "Failed IR emission: " << emit_cmd;

  const std::string opt_cmd =
      "SECU_WCET_CFG_OUT=" + json_path.string() +
      " opt-22 -load-pass-plugin=" + std::string(WCET_PASS_PLUGIN_PATH) +
      " -passes='module(function(mem2reg),cgscc(inline),function(mem2reg,loop-simplify,lcssa),secu-wcet)'"
      " -disable-output " +
      ll_path.string();
  const int opt_rc = std::system(opt_cmd.c_str());
  EXPECT_EQ(WEXITSTATUS(opt_rc), 0) << "Failed opt pass: " << opt_cmd;

  std::ifstream in(json_path);
  return nlohmann::json::parse(in);
}

}  // namespace

TEST(WcetPassTest, TelemetryFramerProvesScevTripCountAndCacheClasses) {
  const std::string src =
      std::string(SOURCE_DIR_PATH) + "/firmware/src/telemetry_framer.cpp";
  const nlohmann::json report = runWcetPassOnCpp(src, "telemetry_wcet");

  ASSERT_FALSE(report["functions"].empty());
  bool found_framer = false;
  for (const auto& fn : report["functions"]) {
    const std::string name = fn["demangled_name"].get<std::string>();
    if (name.find("TelemetryFramer::pack_and_enqueue") != std::string::npos) {
      found_framer = true;
      EXPECT_TRUE(fn["wcet_provable"].get<bool>());
      ASSERT_FALSE(fn["loops"].empty());
      bool found_24_loop = false;
      for (const auto& lp : fn["loops"]) {
        if (lp["max_iterations"].get<uint32_t>() == 24U) {
          found_24_loop = true;
          EXPECT_EQ(lp["bound_source"].get<std::string>(), "SCEV");
          EXPECT_TRUE(lp["bounded"].get<bool>());
        }
      }
      EXPECT_TRUE(found_24_loop);

      bool has_hit_or_fm = false;
      for (const auto& blk : fn["blocks"]) {
        const std::string cc = blk["cache_class"].get<std::string>();
        if (cc == "AH" || cc == "FM") {
          has_hit_or_fm = true;
        }
      }
      EXPECT_TRUE(has_hit_or_fm);
    }
  }
  EXPECT_TRUE(found_framer);
}

TEST(WcetPassTest, ActiveAeroExtractsBoundedLoopAndMutexGroups) {
  const std::string src =
      std::string(SOURCE_DIR_PATH) + "/firmware/src/active_aero_sync.cpp";
  const nlohmann::json report = runWcetPassOnCpp(src, "aero_wcet");

  ASSERT_EQ(report["functions"].size(), 1U);
  const auto& fn = report["functions"][0];
  EXPECT_TRUE(fn["wcet_provable"].get<bool>());
  ASSERT_EQ(fn["loops"].size(), 1U);
  EXPECT_TRUE(fn["loops"][0]["bounded"].get<bool>());
  EXPECT_LE(fn["loops"][0]["max_iterations"].get<uint32_t>(), 7U);
}

TEST(WcetPassTest, MgukArbiterIsProvableWithScevLoops) {
  const std::string src =
      std::string(SOURCE_DIR_PATH) + "/firmware/src/mguk_bbw_arbiter.cpp";
  const nlohmann::json report = runWcetPassOnCpp(src, "mguk_wcet");

  ASSERT_EQ(report["functions"].size(), 1U);
  const auto& fn = report["functions"][0];
  EXPECT_TRUE(fn["wcet_provable"].get<bool>());
  EXPECT_GE(fn["blocks"].size(), 5U);
}

TEST(WcetPassTest, DetectsUnboundedLoopInFaultyFirmware) {
  const std::string src =
      std::string(SOURCE_DIR_PATH) + "/firmware/faulty/faulty_unbounded_loop.cpp";
  const nlohmann::json report = runWcetPassOnCpp(src, "faulty_loop_wcet");

  ASSERT_EQ(report["functions"].size(), 1U);
  const auto& fn = report["functions"][0];
  EXPECT_FALSE(fn["wcet_provable"].get<bool>());
  ASSERT_EQ(fn["loops"].size(), 1U);
  EXPECT_FALSE(fn["loops"][0]["bounded"].get<bool>());
  EXPECT_EQ(fn["loops"][0]["bound_source"].get<std::string>(), "UNBOUNDED");
}
