#include "misra_gatekeeper.hpp"

#include <algorithm>
#include <fstream>
#include <iostream>
#include <set>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <nlohmann/json.hpp>

#include "clang/AST/ASTContext.h"
#include "clang/AST/Attr.h"
#include "clang/AST/Expr.h"
#include "clang/AST/ParentMapContext.h"
#include "clang/ASTMatchers/ASTMatchFinder.h"
#include "clang/ASTMatchers/ASTMatchers.h"
#include "clang/Basic/SourceManager.h"
#include "clang/Frontend/FrontendActions.h"
#include "clang/Tooling/CommonOptionsParser.h"
#include "clang/Tooling/Tooling.h"
#include "llvm/Support/CommandLine.h"

using namespace clang;
using namespace clang::ast_matchers;
using namespace clang::tooling;

namespace {

llvm::cl::OptionCategory kGatekeeperCategory("secu-misra-gatekeeper options");
llvm::cl::opt<std::string> kJsonOutOpt(
    "json-out",
    llvm::cl::desc("Optional path to write JSON MISRA audit report"),
    llvm::cl::value_desc("filename"),
    llvm::cl::cat(kGatekeeperCategory));

bool isUserLocation(const SourceManager& sm, SourceLocation loc) {
  if (loc.isInvalid()) {
    return false;
  }
  const SourceLocation spelling = sm.getSpellingLoc(loc);
  return sm.isInMainFile(spelling) && !sm.isInSystemHeader(spelling);
}

class MisraRuleCallback : public MatchFinder::MatchCallback {
 public:
  explicit MisraRuleCallback(secu::misra::AuditReport& report) : report_(report) {}


  void run(const MatchFinder::MatchResult& result) override {
    const SourceManager& sm = *result.SourceManager;
    ASTContext& ctx = *result.Context;

    if (const auto* new_expr = result.Nodes.getNodeAs<CXXNewExpr>("heap_new")) {
      record(sm, new_expr->getBeginLoc(), "MISRA-CPP-21.6.1", "Dynamic Memory",
             "Dynamic allocation via 'new' is forbidden in real-time SECU firmware.");
    }

    if (const auto* del_expr = result.Nodes.getNodeAs<CXXDeleteExpr>("heap_delete")) {
      record(sm, del_expr->getBeginLoc(), "MISRA-CPP-21.6.1", "Dynamic Memory",
             "Dynamic deallocation via 'delete' is forbidden in real-time SECU firmware.");
    }

    if (const auto* heap_call = result.Nodes.getNodeAs<CallExpr>("heap_c_call")) {
      std::string callee_name = "allocator";
      if (const FunctionDecl* fn = heap_call->getDirectCallee()) {
        callee_name = fn->getNameAsString();
      }
      record(sm, heap_call->getBeginLoc(), "MISRA-CPP-21.6.1", "Dynamic Memory",
             "Dynamic heap call '" + callee_name + "()' violates deterministic memory rules.");
    }

    if (const auto* var_decl = result.Nodes.getNodeAs<VarDecl>("heap_container_var")) {
      record(sm, var_decl->getLocation(), "MISRA-CPP-21.6.1", "Dynamic Memory",
             "Heap-backed STL container '" + var_decl->getNameAsString() +
                 "' is forbidden; use fixed-capacity std::array or secu::RingBuffer.");
    }

    if (const auto* throw_expr = result.Nodes.getNodeAs<CXXThrowExpr>("exception_throw")) {
      record(sm, throw_expr->getBeginLoc(), "MISRA-CPP-18.1.1", "Exceptions",
             "Throwing exceptions ('throw') creates unbounded stack-unwinding latency.");
    }

    if (const auto* try_stmt = result.Nodes.getNodeAs<CXXTryStmt>("exception_try")) {
      record(sm, try_stmt->getBeginLoc(), "MISRA-CPP-18.1.1", "Exceptions",
             "Exception handling ('try/catch') is prohibited in freestanding SECU code.");
    }

    if (const auto* goto_stmt = result.Nodes.getNodeAs<GotoStmt>("goto_stmt")) {
      record(sm, goto_stmt->getBeginLoc(), "MISRA-CPP-9.6.1", "Control Flow",
             "Unstructured 'goto' jumps hinder CFG reducibility and WCET path analysis.");
    }

    if (const auto* uninit_var = result.Nodes.getNodeAs<VarDecl>("uninit_local")) {
      const QualType qt = uninit_var->getType().getCanonicalType();
      if (!uninit_var->hasInit() &&
          (qt->isBuiltinType() || qt->isPointerType() || qt->isEnumeralType())) {
        record(sm, uninit_var->getLocation(), "MISRA-CPP-11.6.1", "Initialization",
               "Local variable '" + uninit_var->getNameAsString() +
                   "' is declared without deterministic initialization.");
      }
    }

    if (const auto* cast_expr = result.Nodes.getNodeAs<ImplicitCastExpr>("implicit_cast")) {
      checkNarrowingCast(sm, ctx, cast_expr);
    }

    if (const auto* while_stmt = result.Nodes.getNodeAs<WhileStmt>("while_loop")) {
      checkWhileLoopBound(sm, ctx, while_stmt);
    }

    if (const auto* call_expr = result.Nodes.getNodeAs<CallExpr>("direct_call")) {
      const auto* caller = result.Nodes.getNodeAs<FunctionDecl>("caller_fn");
      const FunctionDecl* callee = call_expr->getDirectCallee();
      if (caller != nullptr && callee != nullptr &&
          isUserLocation(sm, call_expr->getBeginLoc())) {
        const std::string caller_name = caller->getQualifiedNameAsString();
        const std::string callee_name = callee->getQualifiedNameAsString();
        call_graph_[caller_name].insert(callee_name);
        if (call_locations_.find(caller_name) == call_locations_.end()) {
          call_locations_[caller_name] = call_expr->getBeginLoc();
        }
      }
    }
  }

  void finalizeCallGraph(const SourceManager& sm) {
    std::unordered_set<std::string> visited;
    std::unordered_set<std::string> on_stack;

    for (const auto& [fn, _] : call_graph_) {
      if (visited.find(fn) == visited.end()) {
        detectCycleDfs(sm, fn, visited, on_stack);
      }
    }
    call_graph_.clear();
    call_locations_.clear();
  }

 private:
  void detectCycleDfs(const SourceManager& sm,
                      const std::string& node,
                      std::unordered_set<std::string>& visited,
                      std::unordered_set<std::string>& on_stack) {
    visited.insert(node);
    on_stack.insert(node);

    const auto it = call_graph_.find(node);
    if (it != call_graph_.end()) {
      for (const std::string& callee : it->second) {
        if (on_stack.find(callee) != on_stack.end()) {
          const SourceLocation loc = call_locations_[node];
          record(sm, loc, "MISRA-CPP-17.2.1", "Recursion",
                 "Recursive call cycle detected involving '" + node + "' -> '" + callee +
                     "'; stack depth is unbounded.");
        } else if (visited.find(callee) == visited.end()) {
          detectCycleDfs(sm, callee, visited, on_stack);
        }
      }
    }

    on_stack.erase(node);
  }

  void checkNarrowingCast(const SourceManager& sm,
                          ASTContext& ctx,
                          const ImplicitCastExpr* cast) {
    if (!isUserLocation(sm, cast->getBeginLoc())) {
      return;
    }
    // Ignore implicit casts that sit directly inside an explicit static_cast<T>(...)
    const DynTypedNodeList parents = ctx.getParents(*cast);
    for (const DynTypedNode& parent : parents) {
      if (parent.get<ExplicitCastExpr>() != nullptr) {
        return;
      }
    }

    const QualType src_ty = cast->getSubExpr()->getType().getCanonicalType();
    const QualType dst_ty = cast->getType().getCanonicalType();

    if (cast->getCastKind() == CK_FloatingToIntegral) {
      record(sm, cast->getBeginLoc(), "MISRA-CPP-8.0.1", "Type Safety",
             "Implicit narrowing conversion from floating-point '" + src_ty.getAsString() +
                 "' to integral '" + dst_ty.getAsString() + "'.");
      return;
    }

    if (cast->getCastKind() == CK_FloatingCast &&
        ctx.getTypeSize(src_ty) > ctx.getTypeSize(dst_ty)) {
      record(sm, cast->getBeginLoc(), "MISRA-CPP-8.0.1", "Type Safety",
             "Implicit floating-point precision narrowing from '" + src_ty.getAsString() +
                 "' to '" + dst_ty.getAsString() + "'.");
    }
  }

  void checkWhileLoopBound(const SourceManager& sm,
                           ASTContext& ctx,
                           const WhileStmt* while_stmt) {
    if (!isUserLocation(sm, while_stmt->getBeginLoc())) {
      return;
    }
    bool has_wcet_annotation = false;
    const DynTypedNodeList parents = ctx.getParents(*while_stmt);
    for (const DynTypedNode& parent : parents) {
      if (const auto* attr_stmt = parent.get<AttributedStmt>()) {
        for (const Attr* attr : attr_stmt->getAttrs()) {
          if (const auto* ann = dyn_cast<AnnotateAttr>(attr)) {
            if (ann->getAnnotation().starts_with("wcet_max_iter=")) {
              has_wcet_annotation = true;
            }
          }
        }
      }
    }

    if (!has_wcet_annotation) {
      record(sm, while_stmt->getBeginLoc(), "SECU-WCET-001", "WCET Determinism",
             "Data-dependent 'while' loop lacks a [[clang::annotate(\"wcet_max_iter=N\")]] "
             "bound; static WCET cannot be guaranteed.");
    }
  }

  void record(const SourceManager& sm,
              SourceLocation loc,
              const std::string& rule_id,
              const std::string& category,
              const std::string& msg) {
    if (!isUserLocation(sm, loc)) {
      return;
    }
    const SourceLocation spelling = sm.getSpellingLoc(loc);
    const std::string file = sm.getFilename(spelling).str();
    const unsigned line = sm.getSpellingLineNumber(spelling);
    const unsigned col = sm.getSpellingColumnNumber(spelling);

    // Deduplicate identical diagnostics emitted across template instantiations
    for (const auto& existing : report_.violations) {
      if (existing.file == file && existing.line == line &&
          existing.column == col && existing.rule_id == rule_id) {
        return;
      }
    }

    report_.violations.push_back(
        secu::misra::Violation{file, line, col, rule_id, category, msg});
  }

  secu::misra::AuditReport& report_;
  std::unordered_map<std::string, std::unordered_set<std::string>> call_graph_;
  std::unordered_map<std::string, SourceLocation> call_locations_;
};

class GatekeeperConsumer : public ASTConsumer {
 public:
  explicit GatekeeperConsumer(secu::misra::AuditReport& report) : callback_(report) {
    matcher_.addMatcher(cxxNewExpr().bind("heap_new"), &callback_);
    matcher_.addMatcher(cxxDeleteExpr().bind("heap_delete"), &callback_);
    matcher_.addMatcher(
        callExpr(callee(functionDecl(hasAnyName(
            "malloc", "calloc", "realloc", "free", "aligned_alloc"))))
            .bind("heap_c_call"),
        &callback_);
    matcher_.addMatcher(
        varDecl(hasType(hasUnqualifiedDesugaredType(recordType(hasDeclaration(
                    classTemplateSpecializationDecl(hasAnyName(
                        "std::vector", "std::basic_string", "std::map",
                        "std::unordered_map", "std::list", "std::deque")))))))
            .bind("heap_container_var"),
        &callback_);
    matcher_.addMatcher(cxxThrowExpr().bind("exception_throw"), &callback_);
    matcher_.addMatcher(cxxTryStmt().bind("exception_try"), &callback_);
    matcher_.addMatcher(gotoStmt().bind("goto_stmt"), &callback_);
    matcher_.addMatcher(
        varDecl(hasLocalStorage(), unless(parmVarDecl()), unless(isImplicit()))
            .bind("uninit_local"),
        &callback_);
    matcher_.addMatcher(implicitCastExpr().bind("implicit_cast"), &callback_);
    matcher_.addMatcher(whileStmt().bind("while_loop"), &callback_);
    matcher_.addMatcher(
        callExpr(hasAncestor(functionDecl().bind("caller_fn"))).bind("direct_call"),
        &callback_);
  }

  void HandleTranslationUnit(ASTContext& ctx) override {
    matcher_.matchAST(ctx);
    callback_.finalizeCallGraph(ctx.getSourceManager());
  }

 private:
  MisraRuleCallback callback_;
  MatchFinder matcher_;
};

class GatekeeperAction : public ASTFrontendAction {
 public:
  explicit GatekeeperAction(secu::misra::AuditReport& report) : report_(report) {}

  std::unique_ptr<ASTConsumer> CreateASTConsumer(CompilerInstance& /*ci*/,
                                                 StringRef in_file) override {
    report_.analyzed_files.push_back(in_file.str());
    return std::make_unique<GatekeeperConsumer>(report_);
  }

 private:
  secu::misra::AuditReport& report_;
};

class GatekeeperActionFactory : public FrontendActionFactory {
 public:
  explicit GatekeeperActionFactory(secu::misra::AuditReport& report) : report_(report) {}

  std::unique_ptr<FrontendAction> create() override {
    return std::make_unique<GatekeeperAction>(report_);
  }

 private:
  secu::misra::AuditReport& report_;
};

}  // namespace

int main(int argc, const char** argv) {
  auto expected_parser = CommonOptionsParser::create(argc, argv, kGatekeeperCategory);
  if (!expected_parser) {
    llvm::errs() << expected_parser.takeError();
    return 2;
  }

  CommonOptionsParser& options_parser = expected_parser.get();
  ClangTool tool(options_parser.getCompilations(), options_parser.getSourcePathList());

  secu::misra::AuditReport report;
  GatekeeperActionFactory factory(report);
  const int tool_status = tool.run(&factory);

  for (const auto& v : report.violations) {
    std::cerr << "[VIOLATION] " << v.file << ":" << v.line << ":" << v.column
              << " [" << v.rule_id << "] (" << v.category << ") " << v.message << "\n";
  }

  if (!kJsonOutOpt.empty()) {
    nlohmann::json j;
    j["passed"] = report.passed() && (tool_status == 0);
    j["violation_count"] = report.violations.size();
    j["analyzed_files"] = report.analyzed_files;
    j["violations"] = nlohmann::json::array();
    for (const auto& v : report.violations) {
      j["violations"].push_back({
          {"file", v.file},
          {"line", v.line},
          {"column", v.column},
          {"rule_id", v.rule_id},
          {"category", v.category},
          {"message", v.message},
      });
    }
    std::ofstream out_file(kJsonOutOpt.getValue());
    out_file << j.dump(2) << "\n";
  }

  if (tool_status != 0) {
    return 2;
  }
  return report.passed() ? 0 : 1;
}
