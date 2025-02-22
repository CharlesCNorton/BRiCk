/*
 * Copyright (c) 2020-2024 BlueRock Security, Inc.
 * This software is distributed under the terms of the BedRock Open-Source License.
 * See the LICENSE-BedRock file in the repository root for details.
 */
#pragma once
#include "Trace.hpp"
#include <clang/AST/Decl.h>
#include <clang/AST/DeclCXX.h>
#include <list>
#include <map>
#include <set>
#include <utility>

namespace clang {
class CompilerInstance;
};

class Module {
public:
	struct Flags {
		bool in_template;
		bool
			in_specialization; // explicit specialization or implicit instantiation

		Flags set_template() const {
			return Flags{true, in_specialization};
		}
		Flags set_specialization() const {
			return Flags{in_template, true};
		}

		bool none() const {
			return !in_template && !in_specialization;
		}
	};

	void add_definition(const clang::NamedDecl&, Flags);
	void add_declaration(const clang::NamedDecl&, Flags);
	void add_assert(const clang::StaticAssertDecl&);

	using AssertList = std::list<const clang::StaticAssertDecl*>;
	using DeclList = std::list<const clang::NamedDecl*>;

	const AssertList& asserts() const {
		return asserts_;
	}

	const DeclList& declarations() const {
		return declarations_;
	}

	const DeclList& definitions() const {
		return definitions_;
	}

	const DeclList& template_declarations() const {
		return template_declarations_;
	}

	const DeclList& template_definitions() const {
		return template_definitions_;
	}

	Module() = delete;
	Module(Trace::Mask trace) : trace_(trace & Trace::ModuleBuilder) {}

private:
	const bool trace_;

	DeclList declarations_;
	DeclList definitions_;

	DeclList template_declarations_;
	DeclList template_definitions_;

	AssertList asserts_;

	void add_decl(llvm::StringRef, DeclList&, DeclList&,
				  const clang::NamedDecl&, Flags);
};

class Filter;
class SpecCollector;

void build_module(clang::TranslationUnitDecl* tu, ::Module& mod, Filter& filter,
				  SpecCollector& specs, clang::CompilerInstance*,
				  bool elaborate, bool templates);
