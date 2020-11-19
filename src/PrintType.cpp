/*
 * Copyright (C) BedRock Systems Inc. 2019-2020 Gregory Malecha
 *
 * SPDX-License-Identifier: LGPL-2.1 WITH BedRock Exception for use over network, see repository root for details.
 */
#include "ClangPrinter.hpp"
#include "CoqPrinter.hpp"
#include "Logging.hpp"
#include "TypeVisitorWithArgs.h"
#include "clang/AST/ASTContext.h"
#include "clang/AST/Decl.h"
#include "clang/AST/DeclCXX.h"
#include "clang/AST/DeclTemplate.h"
#include "clang/AST/Mangle.h"
#include "clang/AST/Type.h"
#include "clang/Basic/Version.inc"
#include <Formatter.hpp>

using namespace clang;
using namespace fmt;

void
printQualType(const QualType& qt, CoqPrinter& print, ClangPrinter& cprint) {
    if (auto p = qt.getTypePtrOrNull()) {
        if (qt.isLocalConstQualified()) {
            if (qt.isVolatileQualified()) {
                print.ctor("Qconst_volatile", false);
            } else {
                print.ctor("Qconst", false);
            }
            cprint.printType(p, print);
            print.end_ctor();
        } else {
            if (qt.isLocalVolatileQualified()) {
                print.ctor("Qmut_volatile", false);
                cprint.printType(p, print);
                print.end_ctor();
            } else {
                cprint.printType(p, print);
            }
        }
    } else {
        using namespace logging;
        fatal() << "unexpected null type in printQualType\n";
        die();
    }
}

void
printQualifier(const QualType& qt, CoqPrinter& print) {
    print.begin_record();
    print.record_field("q_const");
    print.boolean(qt.isConstQualified());
    print.output() << ";" << fmt::nbsp;
    print.record_field("q_volatile");
    print.boolean(qt.isVolatileQualified());
    print.end_record();
}

const std::string
bitsize(unsigned n) {
    switch (n) {
    case 8:
        return "W8";
    case 16:
        return "W16";
    case 32:
        return "W32";
    case 64:
        return "W64";
    case 128:
        return "W128";
    default:
        return "unknown_bit_size";
    }
}

class PrintType :
    public TypeVisitor<PrintType, void, CoqPrinter&, ClangPrinter&> {
private:
    PrintType() {}

public:
    static PrintType printer;

    void VisitType(const Type* type, CoqPrinter& print, ClangPrinter& cprint) {
        using namespace logging;
        fatal() << "[ERR] unsupported type (" << type->getTypeClassName()
                << "):";
#if CLANG_VERSION_MAJOR >= 11
        type->dump(fatal(), cprint.getContext());
#else
        type->dump(fatal());
#endif

        fatal() << "\n";
        die();
    }

    void VisitAttributedType(const AttributedType* type, CoqPrinter& print,
                             ClangPrinter& cprint) {
        cprint.printQualType(type->getModifiedType(), print);
    }

    void VisitDeducedType(const DeducedType* type, CoqPrinter& print,
                          ClangPrinter& cprint) {
        cprint.printQualType(type->getDeducedType(), print);
    }

    void VisitTemplateTypeParmType(const TemplateTypeParmType* type,
                                   CoqPrinter& print, ClangPrinter& cprint) {
        print.ctor("Ttemplate")
            << "\"" << type->getDecl()->getNameAsString() << "\"";
        print.end_ctor();
    }

    void VisitEnumType(const EnumType* type, CoqPrinter& print,
                       ClangPrinter& cprint) {
        print.ctor("@Talias", false);
        cprint.printGlobalName(type->getDecl(), print);
        print.output() << fmt::nbsp;
        cprint.printQualType(
            //getPromotionType returns the integer type that the enum promotes to
            type->getDecl()->getCanonicalDecl()->getPromotionType(), print);
        print.end_ctor();
    }
    void VisitRecordType(const RecordType* type, CoqPrinter& print,
                         ClangPrinter& cprint) {
        print.ctor("Tnamed", false);
        cprint.printGlobalName(type->getDecl(), print);
        print.end_ctor();
    }

    void VisitParenType(const ParenType* type, CoqPrinter& print,
                        ClangPrinter& cprint) {
        cprint.printQualType(type->getInnerType(), print);
    }

    void printTypeSugar(const BuiltinType* type, CoqPrinter& print,
                        ClangPrinter& cprint) {
        if (type->isSignedIntegerType()) {
            switch (auto sz = cprint.getTypeSize(type)) {
            case 8:
                print.output() << "T_int8";
                break;
            case 16:
                print.output() << "T_int16";
                break;
            case 32:
                print.output() << "T_int32";
                break;
            case 64:
                print.output() << "T_int64";
                break;
            case 128:
                print.output() << "T_int128";
                break;
            default:
                print.output() << "(Tint " << bitsize(sz) << " Signed)";
            }
        } else if (type->isUnsignedIntegerType()) {
            switch (auto sz = cprint.getTypeSize(type)) {
            case 8:
                print.output() << "T_uint8";
                break;
            case 16:
                print.output() << "T_uint16";
                break;
            case 32:
                print.output() << "T_uint32";
                break;
            case 64:
                print.output() << "T_uint64";
                break;
            case 128:
                print.output() << "T_uint128";
                break;
            default:
                print.output() << "(Tint " << bitsize(sz) << " Unsigned)";
            }
        }
    }

    void VisitBuiltinType(const BuiltinType* type, CoqPrinter& print,
                          ClangPrinter& cprint) {
        switch (type->getKind()) {
        case BuiltinType::Kind::Bool:
            print.output() << "Tbool";
            break;
        case BuiltinType::Kind::Void:
            print.output() << "Tvoid";
            break;
        case BuiltinType::Kind::NullPtr:
            print.output() << "Tnullptr";
            break;
        default:
            if (type->isAnyCharacterType()) {
                print.output()
                    << "(Tchar " << bitsize(cprint.getTypeSize(type)) << " "
                    << (type->isSignedInteger() ? "Signed" : "Unsigned") << ")";
            } else if (type->isFloatingPoint()) {
                print.output()
                    << "(Tfloat " << bitsize(cprint.getTypeSize(type)) << ")";
            } else if (type->isIntegerType()) {
                print.output()
                    << "(Tint " << bitsize(cprint.getTypeSize(type)) << " "
                    << (type->isSignedInteger() ? "Signed" : "Unsigned") << ")";
            } else if (type->isSizelessBuiltinType()) {
                print.output() << fmt::lparen << "Tarch None \""
                               << type->getNameAsCString(
                                      cprint.getContext().getPrintingPolicy())
                               << "\"" << fmt::rparen;
                break;
            } else {
                using namespace logging;
                fatal() << "[ERR] Unsupported builtin type (" << type->getKind()
                        << "): \""
                        << type->getNameAsCString(
                               cprint.getContext().getPrintingPolicy())
                        << "\"\n";
                die();
            }
        }
    }

    void VisitLValueReferenceType(const LValueReferenceType* type,
                                  CoqPrinter& print, ClangPrinter& cprint) {
        print.ctor("Tref", false);
        printQualType(type->getPointeeType(), print, cprint);
        print.end_ctor();
    }

    void VisitRValueReferenceType(const RValueReferenceType* type,
                                  CoqPrinter& print, ClangPrinter& cprint) {
        print.ctor("Trv_ref", false);
        printQualType(type->getPointeeType(), print, cprint);
        print.end_ctor();
    }

    void VisitPointerType(const PointerType* type, CoqPrinter& print,
                          ClangPrinter& cprint) {
        print.ctor("Tptr", false);
        printQualType(type->getPointeeType(), print, cprint);
        print.end_ctor();
    }

    void VisitTypedefType(const TypedefType* type, CoqPrinter& print,
                          ClangPrinter& cprint) {
        print.ctor("@Talias", false);
        cprint.printGlobalName(type->getDecl(), print);
        print.output() << fmt::nbsp;
        cprint.printQualType(
            type->getDecl()->getCanonicalDecl()->getUnderlyingType(), print);
        print.output() << fmt::rparen;
    }

    void VisitFunctionProtoType(const FunctionProtoType* type,
                                CoqPrinter& print, ClangPrinter& cprint) {
        print.ctor("@Tfunction");
        cprint.printCallingConv(type->getCallConv(), print);
        print.output() << fmt::nbsp;
        printQualType(type->getReturnType(), print, cprint);
        print.output() << fmt::nbsp;
        print.begin_list();
        for (auto i : type->param_types()) {
            printQualType(i, print, cprint);
            print.cons();
        }
        print.end_list();

        print.end_ctor();
    }

    void VisitElaboratedType(const ElaboratedType* type, CoqPrinter& print,
                             ClangPrinter& cprint) {
        printQualType(type->getNamedType(), print, cprint);
    }

    void VisitConstantArrayType(const ConstantArrayType* type,
                                CoqPrinter& print, ClangPrinter& cprint) {
        print.ctor("Tarray");
        printQualType(type->getElementType(), print, cprint);
        print.output() << fmt::nbsp << type->getSize().getLimitedValue()
                       << fmt::rparen;
    }

    void VisitSubstTemplateTypeParmType(const SubstTemplateTypeParmType* type,
                                        CoqPrinter& print,
                                        ClangPrinter& cprint) {
        printQualType(type->getReplacementType(), print, cprint);
    }

    void VisitIncompleteArrayType(const IncompleteArrayType* type,
                                  CoqPrinter& print, ClangPrinter& cprint) {
        // note(gmm): i might want to note the sugar.
        print.ctor("Qconst");
        print.ctor("Tptr", false);
        printQualType(type->getElementType(), print, cprint);
        print.output() << fmt::rparen << fmt::rparen;
    }

    void VisitDecayedType(const DecayedType* type, CoqPrinter& print,
                          ClangPrinter& cprint) {
        print.ctor("Qconst");
        print.ctor("Tptr", false);
        printQualType(type->getPointeeType(), print, cprint);
        print.output() << fmt::rparen << fmt::rparen;
    }

    void VisitTemplateSpecializationType(const TemplateSpecializationType* type,
                                         CoqPrinter& print,
                                         ClangPrinter& cprint) {
        if (type->isSugared()) {
            cprint.printQualType(type->desugar(), print);
        } else {
            VisitType(type, print, cprint);
        }
    }

    void VisitDecltypeType(const DecltypeType* type, CoqPrinter& print,
                           ClangPrinter& cprint) {
        cprint.printQualType(type->desugar(), print);
    }

    void VisitTypeOfExprType(const TypeOfExprType* type, CoqPrinter& print,
                             ClangPrinter& cprint) {
        cprint.printQualType(type->desugar(), print);
    }

    void VisitInjectedClassNameType(const InjectedClassNameType* type,
                                    CoqPrinter& print, ClangPrinter& cprint) {
        if (type->getDecl()) {
            print.ctor("Tnamed");
            cprint.printGlobalName(type->getDecl(), print);
            print.end_ctor();
        } else {
            logging::log() << "no underlying declaration for \n";
#if CLANG_VERSION_MAJOR >= 11
            type->dump(logging::log(), cprint.getContext());
#else
            type->dump(logging::log());
#endif

            cprint.printQualType(type->getInjectedSpecializationType(), print);
        }
    }

    void VisitMemberPointerType(const MemberPointerType* type,
                                CoqPrinter& print, ClangPrinter& cprint) {
        print.ctor("Tmember_pointer");
        cprint.printGlobalName(type->getClass()->getAsCXXRecordDecl(), print);
        print.output() << fmt::nbsp;
        cprint.printQualType(type->getPointeeType(), print);
        print.end_ctor();
    }
};

PrintType PrintType::printer;

void
ClangPrinter::printType(const clang::Type* type, CoqPrinter& print) {
    auto depth = print.output().get_depth();
    PrintType::printer.Visit(type, print, *this);
    assert(depth == print.output().get_depth());
}

void
ClangPrinter::printQualType(const QualType& qt, CoqPrinter& print) {
    auto depth = print.output().get_depth();
    ::printQualType(qt, print, *this);
    assert(depth == print.output().get_depth());
}

void
ClangPrinter::printQualifier(const QualType& qt, CoqPrinter& print) const {
    auto depth = print.output().get_depth();
    ::printQualifier(qt, print);
    assert(depth == print.output().get_depth());
}

void
ClangPrinter::printQualifier(bool is_const, bool is_volatile,
                             CoqPrinter& print) const {
    if (is_const) {
        if (is_volatile) {
            print.output() << "QCV";
        } else {
            print.output() << "QC";
        }
    } else {
        if (is_volatile) {
            print.output() << "QV";
        } else {
            print.output() << "QM";
        }
    }
}
