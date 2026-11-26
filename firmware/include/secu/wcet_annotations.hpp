#pragma once

// Dual-stage static analysis annotations:
// - Emits AST attributes for Phase 2 Clang LibTooling checks AND @llvm.var.annotation
//   intrinsics for Phase 3 LLVM IR loop-bound / mutex-group extraction.
// - Expands to nothing under bare-metal GCC (Phase 5).
#if defined(__clang__)
#define WCET_CONCAT_IMPL(a, b) a##b
#define WCET_CONCAT(a, b) WCET_CONCAT_IMPL(a, b)
#define WCET_MAX_ITER(n)                                                                 \
  [[clang::annotate("wcet_max_iter=" #n)]] [[maybe_unused]] int WCET_CONCAT(_wcet_it_,   \
                                                                            __LINE__) =  \
      0;                                                                                 \
  [[clang::annotate("wcet_max_iter=" #n)]]
#define WCET_PATH_GROUP(group, tag)                                                      \
  [[clang::annotate("wcet_mutex:" group ":" tag)]] [[maybe_unused]] int WCET_CONCAT(     \
      _wcet_grp_, __LINE__) = 0
#else
#define WCET_MAX_ITER(n)
#define WCET_PATH_GROUP(group, tag)
#endif
