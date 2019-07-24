/*
 * Copyright (C) BedRock Systems Inc. 2019 Gregory Malecha
 *
 * SPDX-License-Identifier:AGPL-3.0-or-later
 *
 * This file is based on the tutorial here:
 * https://clang.llvm.org/docs/LibASTMatchersTutorial.html
 */
#include <optional>
#include "clang/AST/ASTConsumer.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Frontend/FrontendAction.h"

#include "clang/Tooling/Tooling.h"
#include "clang/Tooling/CommonOptionsParser.h"
// Declares clang::SyntaxOnlyAction.
#include "clang/Frontend/FrontendActions.h"
// Declares llvm::cl::extrahelp.
#include "llvm/Support/CommandLine.h"

#include "Logging.hpp"
#include "ToCoq.hpp"

using namespace clang;
using namespace clang::tooling;
using namespace llvm;

// Apply a custom category to all command-line options so that they are the
// only ones displayed.
static cl::OptionCategory Cpp2V("cpp2v options");

// CommonOptionsParser declares HelpMessage with a description of the common
// command-line options related to the compilation database and input files.
// It's nice to have this help message in all tools.
static cl::extrahelp CommonHelp(CommonOptionsParser::HelpMessage);

static cl::opt<std::string> SpecFile("spec", cl::desc("path to generate specifications"), cl::Optional, cl::cat(Cpp2V));

static cl::opt<std::string> VFileOutput("o", cl::desc("path to generate the module"), cl::Optional, cl::cat(Cpp2V));

static cl::opt<bool> Verbose("v", cl::desc("verbose"), cl::Optional, cl::cat(Cpp2V));

class ToCoqAction: public clang::ASTFrontendAction {
public:
	virtual std::unique_ptr<clang::ASTConsumer> CreateASTConsumer(
			clang::CompilerInstance &Compiler, llvm::StringRef InFile) {
#if 0
		Compiler.getInvocation().getLangOpts()->CommentOpts.BlockCommandNames.push_back("with");
		Compiler.getInvocation().getLangOpts()->CommentOpts.BlockCommandNames.push_back("internal");
    for (auto i : Compiler.getInvocation().getLangOpts()->CommentOpts.BlockCommandNames) {
			llvm::errs() << i << "\n";
		}
#endif
		auto result = new ToCoqConsumer(to_opt(VFileOutput), to_opt(SpecFile));
		return std::unique_ptr < clang::ASTConsumer > (result);
	}

  template<typename T> Optional<T> to_opt(const cl::opt<T>& val) {
    if (val.empty()) {
      return Optional<T>();
    } else {
      return Optional<T>(val.getValue());
    }
  }
};

int main(int argc, const char **argv) {
	CommonOptionsParser OptionsParser(argc, argv, Cpp2V);
	ClangTool Tool(OptionsParser.getCompilations(),
			OptionsParser.getSourcePathList());

	if (Verbose) {
		logging::set_level(logging::VERBOSE);
	}

	return Tool.run(newFrontendActionFactory<ToCoqAction>().get());
}
