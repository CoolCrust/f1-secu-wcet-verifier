#pragma once

// Portable static analysis annotations:
// - Under Clang/LLVM, emits @llvm.var.annotation / @llvm.ptr.annotation intrinsics
//   consumed by WCETPass (Phase 3) and IPET infeasible-path pruning (Phase 4).
// - Under bare-metal GCC (Phase 5), expands cleanly to nothing without -Wattributes.
#if defined(__clang__)
#define WCET_MAX_ITER(n) [[clang::annotate("wcet_max_iter=" #n)]]
#define WCET_PATH_GROUP(group, tag) [[clang::annotate("wcet_mutex:" group ":" tag)]]
#else
#define WCET_MAX_ITER(n)
#define WCET_PATH_GROUP(group, tag)
#endif
