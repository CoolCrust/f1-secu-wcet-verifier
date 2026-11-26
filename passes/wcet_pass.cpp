#include "wcet_pass.hpp"

#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <map>
#include <set>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include <nlohmann/json.hpp>

#include "llvm/Analysis/LoopInfo.h"
#include "llvm/Analysis/ScalarEvolution.h"
#include "llvm/Demangle/Demangle.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/IntrinsicInst.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/PassManager.h"
#include "llvm/Passes/PassBuilder.h"
#if __has_include("llvm/Plugins/PassPlugin.h")
#include "llvm/Plugins/PassPlugin.h"
#else
#include "llvm/Passes/PassPlugin.h"
#endif
#include "llvm/Support/raw_ostream.h"

using namespace llvm;

namespace {

// ARM Cortex-M7 L1 Instruction Cache Geometry: 4 KB, 2-way set associative, 32-byte lines
constexpr uint32_t kCacheLineBytes = 32U;
constexpr uint32_t kCacheWays = 2U;
constexpr uint32_t kCacheSets = 64U;
constexpr uint32_t kFlashMissPenaltyCycles = 6U;

struct InstrCost {
  uint32_t cycles{1U};
  uint32_t bytes{2U};
};

InstrCost estimateThumb2Instruction(const Instruction& inst) {
  if (isa<PHINode>(inst) || inst.isDebugOrPseudoInst()) {
    return InstrCost{0U, 0U};
  }
  if (const auto* call = dyn_cast<CallBase>(&inst)) {
    if (const Function* callee = call->getCalledFunction()) {
      if (callee->isIntrinsic()) {
        // Overflow/saturating intrinsics lower to 1-2 Thumb-2 instructions
        return InstrCost{1U, 4U};
      }
    }
    return InstrCost{3U, 4U};
  }
  if (isa<LoadInst>(inst) || isa<StoreInst>(inst)) {
    return InstrCost{2U, 4U};
  }
  const unsigned op = inst.getOpcode();
  if (op == Instruction::SDiv || op == Instruction::UDiv) {
    return InstrCost{6U, 4U};
  }
  if (op == Instruction::Mul) {
    return InstrCost{2U, 4U};
  }
  if (isa<BranchInst>(inst) || isa<SwitchInst>(inst) || isa<ReturnInst>(inst)) {
    return InstrCost{1U, 2U};
  }
  return InstrCost{1U, 2U};
}

// Abstract cache state per cache set: maps memory line tag -> LRU age in [0, kCacheWays - 1]
using SetState = std::map<uint32_t, uint32_t>;
using CacheState = std::vector<SetState>;

CacheState makeEmptyCacheState() {
  return CacheState(kCacheSets);
}

void accessCacheLine(CacheState& state, uint32_t line_addr, bool is_must) {
  const uint32_t set_idx = line_addr % kCacheSets;
  SetState& set = state[set_idx];
  const auto it = set.find(line_addr);
  const uint32_t prev_age = (it != set.end()) ? it->second : kCacheWays;

  for (auto entry_it = set.begin(); entry_it != set.end();) {
    if (entry_it->first == line_addr) {
      ++entry_it;
      continue;
    }
    if (entry_it->second < prev_age) {
      entry_it->second += 1U;
    }
    if (entry_it->second >= kCacheWays) {
      entry_it = set.erase(entry_it);
    } else {
      ++entry_it;
    }
  }
  (void)is_must;
  set[line_addr] = 0U;
}

CacheState joinMustStates(const std::vector<CacheState>& preds) {
  if (preds.empty()) {
    return makeEmptyCacheState();
  }
  CacheState result = preds[0];
  for (std::size_t p = 1U; p < preds.size(); ++p) {
    for (uint32_t s = 0U; s < kCacheSets; ++s) {
      SetState merged;
      for (const auto& [tag, age_a] : result[s]) {
        const auto it = preds[p][s].find(tag);
        if (it != preds[p][s].end()) {
          const uint32_t worst_age = std::max(age_a, it->second);
          if (worst_age < kCacheWays) {
            merged[tag] = worst_age;
          }
        }
      }
      result[s] = std::move(merged);
    }
  }
  return result;
}

CacheState joinMayStates(const std::vector<CacheState>& preds) {
  CacheState result = makeEmptyCacheState();
  for (const CacheState& pred : preds) {
    for (uint32_t s = 0U; s < kCacheSets; ++s) {
      for (const auto& [tag, age] : pred[s]) {
        if (age >= kCacheWays) {
          continue;
        }
        const auto it = result[s].find(tag);
        if (it == result[s].end()) {
          result[s][tag] = age;
        } else {
          result[s][tag] = std::min(it->second, age);
        }
      }
    }
  }
  return result;
}

bool isLineInCache(const CacheState& state, uint32_t line_addr) {
  const uint32_t set_idx = line_addr % kCacheSets;
  const auto it = state[set_idx].find(line_addr);
  return (it != state[set_idx].end()) && (it->second < kCacheWays);
}

void extractAnnotationsFromInstruction(const Instruction& inst,
                                       uint32_t& annotated_max_iter,
                                       std::string& mutex_group,
                                       std::string& mutex_tag) {
  if (const MDNode* md = inst.getMetadata("annotation")) {
    for (const MDOperand& op : md->operands()) {
      if (const auto* mds = dyn_cast<MDString>(op.get())) {
        const StringRef str = mds->getString();
        if (str.starts_with("wcet_max_iter=")) {
          uint32_t parsed = 0U;
          if (!str.drop_front(14).getAsInteger(10, parsed) && parsed > 0U) {
            annotated_max_iter = parsed;
          }
        } else if (str.starts_with("wcet_mutex:")) {
          const StringRef rest = str.drop_front(11);
          const std::size_t sep = rest.find(':');
          if (sep != StringRef::npos) {
            mutex_group = rest.substr(0, sep).str();
            mutex_tag = rest.substr(sep + 1).str();
          }
        }
      }
    }
  }

  if (const auto* call = dyn_cast<CallBase>(&inst)) {
    if (const Function* callee = call->getCalledFunction()) {
      if (callee->getName().starts_with("llvm.var.annotation") ||
          callee->getName().starts_with("llvm.ptr.annotation")) {
        if (call->arg_size() >= 2U) {
          if (const auto* gv =
                  dyn_cast<GlobalVariable>(call->getArgOperand(1U)->stripPointerCasts())) {
            if (gv->hasInitializer()) {
              if (const auto* cda = dyn_cast<ConstantDataArray>(gv->getInitializer())) {
                StringRef ann = cda->getAsCString();
                if (ann.starts_with("wcet_max_iter=")) {
                  uint32_t parsed = 0U;
                  if (!ann.drop_front(14).getAsInteger(10, parsed) && parsed > 0U) {
                    annotated_max_iter = parsed;
                  }
                } else if (ann.starts_with("wcet_mutex:")) {
                  const StringRef rest = ann.drop_front(11);
                  const std::size_t sep = rest.find(':');
                  if (sep != StringRef::npos) {
                    mutex_group = rest.substr(0, sep).str();
                    mutex_tag = rest.substr(sep + 1).str();
                  }
                }
              }
            }
          }
        }
      }
    }
  }
}

secu::wcet::FunctionCfgReport analyzeFunction(Function& F,
                                             FunctionAnalysisManager& FAM,
                                             uint32_t function_base_addr) {
  secu::wcet::FunctionCfgReport report;
  report.mangled_name = F.getName().str();
  report.demangled_name = llvm::demangle(report.mangled_name);
  report.entry_block_id = 0U;

  auto& LI = FAM.getResult<LoopAnalysis>(F);
  auto& SE = FAM.getResult<ScalarEvolutionAnalysis>(F);

  std::unordered_map<const BasicBlock*, uint32_t> bb_to_id;
  std::vector<const BasicBlock*> id_to_bb;
  uint32_t next_id = 0U;
  for (const BasicBlock& BB : F) {
    bb_to_id[&BB] = next_id++;
    id_to_bb.push_back(&BB);
  }

  const std::size_t num_blocks = id_to_bb.size();
  report.blocks.resize(num_blocks);

  // Assign deterministic flash addresses to basic blocks to model I-cache sets
  uint32_t current_addr = function_base_addr;
  std::vector<std::vector<uint32_t>> block_cache_lines(num_blocks);

  for (std::size_t idx = 0U; idx < num_blocks; ++idx) {
    const BasicBlock* BB = id_to_bb[idx];
    secu::wcet::BasicBlockInfo info;
    info.id = static_cast<uint32_t>(idx);
    info.label = BB->hasName() ? BB->getName().str() : ("bb_" + std::to_string(idx));

    uint32_t total_cycles = 0U;
    uint32_t total_bytes = 0U;
    uint32_t inst_count = 0U;
    uint32_t unused_iter = 0U;

    for (const Instruction& inst : *BB) {
      extractAnnotationsFromInstruction(inst, unused_iter, info.mutex_group, info.mutex_tag);
      const InstrCost cost = estimateThumb2Instruction(inst);
      if (cost.bytes > 0U) {
        ++inst_count;
        total_cycles += cost.cycles;
        total_bytes += cost.bytes;
      }
    }

    if (total_bytes == 0U) {
      total_bytes = 2U;
    }
    if (total_cycles == 0U) {
      total_cycles = 1U;
    }

    const uint32_t start_line = current_addr / kCacheLineBytes;
    const uint32_t end_line = (current_addr + total_bytes - 1U) / kCacheLineBytes;
    for (uint32_t line = start_line; line <= end_line; ++line) {
      block_cache_lines[idx].push_back(line);
    }
    current_addr += (total_bytes + 3U) & ~3U;

    info.instruction_count = inst_count;
    info.code_bytes = total_bytes;
    info.cache_lines_touched = static_cast<uint32_t>(block_cache_lines[idx].size());
    info.base_cycles = total_cycles;
    report.blocks[idx] = std::move(info);

    if (succ_empty(BB)) {
      report.exit_block_ids.push_back(static_cast<uint32_t>(idx));
    }
  }

  // Extract loops, SCEV trip counts, and backedges
  std::set<std::pair<uint32_t, uint32_t>> backedge_set;
  uint32_t loop_idx = 0U;
  for (Loop* L : LI.getLoopsInPreorder()) {
    secu::wcet::LoopBoundInfo linfo;
    linfo.loop_id = loop_idx++;
    const BasicBlock* header = L->getHeader();
    linfo.header_block_id = bb_to_id[header];
    report.blocks[linfo.header_block_id].is_loop_header = true;

    for (BasicBlock* member : L->getBlocks()) {
      const uint32_t mid = bb_to_id[member];
      linfo.member_blocks.push_back(mid);
      if (report.blocks[mid].loop_id < 0) {
        report.blocks[mid].loop_id = static_cast<int32_t>(linfo.loop_id);
      }
      for (const BasicBlock* succ : successors(member)) {
        if (succ == header) {
          backedge_set.insert({mid, linfo.header_block_id});
        }
      }
    }

    const unsigned scev_max = SE.getSmallConstantMaxTripCount(L);
    if (scev_max > 0U) {
      linfo.max_iterations = (scev_max > 1U) ? (scev_max - 1U) : 1U;
      linfo.bounded = true;
      linfo.bound_source = "SCEV";
    } else {
      uint32_t ann_iter = 0U;
      std::string dummy_g;
      std::string dummy_t;
      for (BasicBlock* member : L->getBlocks()) {
        for (const Instruction& inst : *member) {
          extractAnnotationsFromInstruction(inst, ann_iter, dummy_g, dummy_t);
        }
      }
      if (BasicBlock* preheader = L->getLoopPreheader()) {
        for (const Instruction& inst : *preheader) {
          extractAnnotationsFromInstruction(inst, ann_iter, dummy_g, dummy_t);
        }
      }
      if (ann_iter > 0U) {
        linfo.max_iterations = ann_iter;
        linfo.bounded = true;
        linfo.bound_source = "annotation";
      } else {
        linfo.max_iterations = 0U;
        linfo.bounded = false;
        linfo.bound_source = "UNBOUNDED";
        report.wcet_provable = false;
      }
    }

    report.loops.push_back(std::move(linfo));
  }

  // Build CFG edge list and predecessor adjacency
  std::vector<std::vector<uint32_t>> preds(num_blocks);
  for (std::size_t idx = 0U; idx < num_blocks; ++idx) {
    const BasicBlock* BB = id_to_bb[idx];
    for (const BasicBlock* succ : successors(BB)) {
      const uint32_t dst = bb_to_id[succ];
      const bool is_be = backedge_set.count({static_cast<uint32_t>(idx), dst}) > 0U;
      report.edges.push_back(secu::wcet::CfgEdgeInfo{static_cast<uint32_t>(idx), dst, is_be});
      preds[dst].push_back(static_cast<uint32_t>(idx));
    }
  }

  // Fixed-point Must and May abstract cache interpretation
  std::vector<CacheState> must_in(num_blocks, makeEmptyCacheState());
  std::vector<CacheState> must_out(num_blocks, makeEmptyCacheState());
  std::vector<CacheState> may_in(num_blocks, makeEmptyCacheState());
  std::vector<CacheState> may_out(num_blocks, makeEmptyCacheState());

  bool changed = true;
  uint32_t guard_iters = 0U;
  while (changed && guard_iters < 128U) {
    changed = false;
    ++guard_iters;
    for (std::size_t idx = 0U; idx < num_blocks; ++idx) {
      if (idx > 0U && !preds[idx].empty()) {
        std::vector<CacheState> pred_must;
        std::vector<CacheState> pred_may;
        for (uint32_t p : preds[idx]) {
          pred_must.push_back(must_out[p]);
          pred_may.push_back(may_out[p]);
        }
        must_in[idx] = joinMustStates(pred_must);
        may_in[idx] = joinMayStates(pred_may);
      }

      CacheState next_must = must_in[idx];
      CacheState next_may = may_in[idx];
      for (uint32_t line : block_cache_lines[idx]) {
        accessCacheLine(next_must, line, true);
        accessCacheLine(next_may, line, false);
      }

      if (next_must != must_out[idx] || next_may != may_out[idx]) {
        must_out[idx] = std::move(next_must);
        may_out[idx] = std::move(next_may);
        changed = true;
      }
    }
  }

  // Classify each BasicBlock as AH, FM, or AM and compute cycle weights
  for (std::size_t idx = 0U; idx < num_blocks; ++idx) {
    secu::wcet::BasicBlockInfo& binfo = report.blocks[idx];
    bool all_must_hit = true;
    bool all_may_miss = true;

    for (uint32_t line : block_cache_lines[idx]) {
      if (!isLineInCache(must_in[idx], line)) {
        all_must_hit = false;
      }
      if (isLineInCache(may_in[idx], line)) {
        all_may_miss = false;
      }
    }

    // Check loop persistence: if inside a bounded loop and all lines stay in Must-cache
    // across internal loop backedges, classify cold-entry misses as First-Miss (FM).
    bool persistent_in_loop = false;
    if (!all_must_hit && binfo.loop_id >= 0) {
      std::vector<CacheState> loop_backedge_must;
      for (uint32_t p : preds[idx]) {
        if (report.blocks[p].loop_id == binfo.loop_id) {
          loop_backedge_must.push_back(must_out[p]);
        }
      }
      if (!loop_backedge_must.empty()) {
        const CacheState loop_steady_in = joinMustStates(loop_backedge_must);
        persistent_in_loop = true;
        for (uint32_t line : block_cache_lines[idx]) {
          if (!isLineInCache(loop_steady_in, line)) {
            persistent_in_loop = false;
            break;
          }
        }
      }
    }

    const uint32_t miss_penalty = binfo.cache_lines_touched * kFlashMissPenaltyCycles;
    if (all_must_hit) {
      binfo.cache_class = secu::wcet::CacheClass::AlwaysHit;
      binfo.first_iter_cycles = binfo.base_cycles;
      binfo.steady_iter_cycles = binfo.base_cycles;
    } else if (persistent_in_loop) {
      binfo.cache_class = secu::wcet::CacheClass::FirstMiss;
      binfo.first_iter_cycles = binfo.base_cycles + miss_penalty;
      binfo.steady_iter_cycles = binfo.base_cycles;
    } else {
      (void)all_may_miss;
      binfo.cache_class = secu::wcet::CacheClass::AlwaysMiss;
      binfo.first_iter_cycles = binfo.base_cycles + miss_penalty;
      binfo.steady_iter_cycles = binfo.base_cycles + miss_penalty;
    }
  }

  return report;
}

class SecuWcetModulePass : public PassInfoMixin<SecuWcetModulePass> {
 public:
  PreservedAnalyses run(Module& M, ModuleAnalysisManager& MAM) {
    auto& FAM = MAM.getResult<FunctionAnalysisManagerModuleProxy>(M).getManager();

    nlohmann::json root;
    root["module_name"] = M.getName().str();
    root["cache_config"] = {
        {"size_bytes", kCacheSets * kCacheWays * kCacheLineBytes},
        {"ways", kCacheWays},
        {"sets", kCacheSets},
        {"line_bytes", kCacheLineBytes},
        {"miss_penalty_cycles", kFlashMissPenaltyCycles},
    };
    root["functions"] = nlohmann::json::array();

    uint32_t flash_cursor = 0x08001000U;
    for (Function& F : M) {
      if (F.isDeclaration()) {
        continue;
      }
      const std::string demangled = llvm::demangle(F.getName().str());
      const bool is_target =
          demangled.find("MgukBbwArbiter::step") != std::string::npos ||
          demangled.find("ActiveAeroController::step") != std::string::npos ||
          demangled.find("TelemetryFramer::pack_and_enqueue") != std::string::npos ||
          demangled.find("wait_for_can_mailbox_ready") != std::string::npos;

      if (!is_target) {
        continue;
      }

      const secu::wcet::FunctionCfgReport rep = analyzeFunction(F, FAM, flash_cursor);
      flash_cursor += 0x00001000U;

      nlohmann::json fj;
      fj["mangled_name"] = rep.mangled_name;
      fj["demangled_name"] = rep.demangled_name;
      fj["entry_block_id"] = rep.entry_block_id;
      fj["exit_block_ids"] = rep.exit_block_ids;
      fj["wcet_provable"] = rep.wcet_provable;

      fj["blocks"] = nlohmann::json::array();
      for (const auto& b : rep.blocks) {
        fj["blocks"].push_back({
            {"id", b.id},
            {"label", b.label},
            {"instruction_count", b.instruction_count},
            {"code_bytes", b.code_bytes},
            {"cache_lines_touched", b.cache_lines_touched},
            {"base_cycles", b.base_cycles},
            {"first_iter_cycles", b.first_iter_cycles},
            {"steady_iter_cycles", b.steady_iter_cycles},
            {"cache_class", secu::wcet::to_string(b.cache_class)},
            {"loop_id", b.loop_id},
            {"is_loop_header", b.is_loop_header},
            {"mutex_group", b.mutex_group},
            {"mutex_tag", b.mutex_tag},
        });
      }

      fj["edges"] = nlohmann::json::array();
      for (const auto& e : rep.edges) {
        fj["edges"].push_back({
            {"src", e.src},
            {"dst", e.dst},
            {"is_backedge", e.is_backedge},
        });
      }

      fj["loops"] = nlohmann::json::array();
      for (const auto& l : rep.loops) {
        fj["loops"].push_back({
            {"loop_id", l.loop_id},
            {"header_block_id", l.header_block_id},
            {"max_iterations", l.max_iterations},
            {"bounded", l.bounded},
            {"bound_source", l.bound_source},
            {"member_blocks", l.member_blocks},
        });
      }

      root["functions"].push_back(std::move(fj));
    }

    const char* env_out = std::getenv("SECU_WCET_CFG_OUT");
    const std::string out_path =
        (env_out != nullptr && env_out[0] != '\0') ? env_out : "wcet_cfg_report.json";
    std::ofstream out_file(out_path);
    out_file << root.dump(2) << "\n";

    return PreservedAnalyses::all();
  }
};

}  // namespace

extern "C" LLVM_ATTRIBUTE_WEAK ::llvm::PassPluginLibraryInfo llvmGetPassPluginInfo() {
  return {LLVM_PLUGIN_API_VERSION, "SecuWCETPass", LLVM_VERSION_STRING,
          [](PassBuilder& PB) {
            PB.registerPipelineParsingCallback(
                [](StringRef Name, ModulePassManager& MPM,
                   ArrayRef<PassBuilder::PipelineElement>) {
                  if (Name == "secu-wcet") {
                    MPM.addPass(SecuWcetModulePass());
                    return true;
                  }
                  return false;
                });
          }};
}
