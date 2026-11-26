#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace secu::wcet {

enum class CacheClass {
  AlwaysHit,   // AH: guaranteed L1 I-cache hit on every execution
  FirstMiss,   // FM: misses on cold loop entry, hits on subsequent loop iterations
  AlwaysMiss   // AM: conservative cold/conflicting fetch (pays flash wait-state penalty)
};

inline const char* to_string(CacheClass cc) noexcept {
  switch (cc) {
    case CacheClass::AlwaysHit:
      return "AH";
    case CacheClass::FirstMiss:
      return "FM";
    case CacheClass::AlwaysMiss:
      return "AM";
  }
  return "AM";
}

struct BasicBlockInfo {
  uint32_t id{0U};
  std::string label;
  uint32_t instruction_count{0U};
  uint32_t code_bytes{0U};
  uint32_t cache_lines_touched{1U};
  uint32_t base_cycles{0U};
  uint32_t first_iter_cycles{0U};
  uint32_t steady_iter_cycles{0U};
  CacheClass cache_class{CacheClass::AlwaysMiss};
  int32_t loop_id{-1};
  bool is_loop_header{false};
  std::string mutex_group;
  std::string mutex_tag;
};

struct CfgEdgeInfo {
  uint32_t src{0U};
  uint32_t dst{0U};
  bool is_backedge{false};
};

struct LoopBoundInfo {
  uint32_t loop_id{0U};
  uint32_t header_block_id{0U};
  uint32_t max_iterations{0U};
  bool bounded{false};
  std::string bound_source;  // "SCEV", "annotation", or "UNBOUNDED"
  std::vector<uint32_t> member_blocks;
};

struct FunctionCfgReport {
  std::string mangled_name;
  std::string demangled_name;
  uint32_t entry_block_id{0U};
  std::vector<uint32_t> exit_block_ids;
  bool wcet_provable{true};
  std::vector<BasicBlockInfo> blocks;
  std::vector<CfgEdgeInfo> edges;
  std::vector<LoopBoundInfo> loops;
};

}  // namespace secu::wcet
