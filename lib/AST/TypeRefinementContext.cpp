//===--- TypeRefinementContext.cpp - Swift Refinement Context -------------===//
//
// This source file is part of the Swift.org open source project
//
// Copyright (c) 2014 - 2017 Apple Inc. and the Swift project authors
// Licensed under Apache License v2.0 with Runtime Library Exception
//
// See https://swift.org/LICENSE.txt for license information
// See https://swift.org/CONTRIBUTORS.txt for the list of Swift project authors
//
//===----------------------------------------------------------------------===//
//
// This file implements the TypeRefinementContext class.
//
//===----------------------------------------------------------------------===//

#include "swift/AST/ASTContext.h"
#include "swift/AST/Decl.h"
#include "swift/AST/Module.h"
#include "swift/AST/Stmt.h"
#include "swift/AST/Expr.h"
#include "swift/AST/SourceFile.h"
#include "swift/AST/TypeCheckRequests.h"
#include "swift/AST/TypeRefinementContext.h"
#include "swift/Basic/Assertions.h"
#include "swift/Basic/SourceManager.h"

using namespace swift;

TypeRefinementContext::TypeRefinementContext(
    ASTContext &Ctx, IntroNode Node, TypeRefinementContext *Parent,
    SourceRange SrcRange, const AvailabilityRange &Info,
    const AvailabilityRange &ExplicitInfo)
    : Node(Node), SrcRange(SrcRange), AvailabilityInfo(Info),
      ExplicitAvailabilityInfo(ExplicitInfo) {
  if (Parent) {
    assert(SrcRange.isValid());
    Parent->addChild(this);
    assert(Info.isContainedIn(Parent->getAvailabilityInfo()));
  }
  Ctx.addDestructorCleanup(Children);
}

TypeRefinementContext *
TypeRefinementContext::createForSourceFile(SourceFile *SF,
                                           const AvailabilityRange &Info) {
  assert(SF);

  ASTContext &Ctx = SF->getASTContext();

  SourceRange range;
  TypeRefinementContext *parentContext = nullptr;
  AvailabilityRange availabilityRange = Info;
  switch (SF->Kind) {
  case SourceFileKind::MacroExpansion:
  case SourceFileKind::DefaultArgument: {
    // Look up the parent context in the enclosing file that this file's
    // root context should be nested under.
    if (auto parentTRC =
            SF->getEnclosingSourceFile()->getTypeRefinementContext()) {
      auto charRange = Ctx.SourceMgr.getRangeForBuffer(SF->getBufferID());
      range = SourceRange(charRange.getStart(), charRange.getEnd());
      auto originalNode = SF->getNodeInEnclosingSourceFile();
      parentContext =
          parentTRC->findMostRefinedSubContext(originalNode.getStartLoc(), Ctx);
      if (parentContext)
        availabilityRange = parentContext->getAvailabilityInfo();
    }
    break;
  }
  case SourceFileKind::Library:
  case SourceFileKind::Main:
  case SourceFileKind::Interface:
    break;
  case SourceFileKind::SIL:
    llvm_unreachable("unexpected SourceFileKind");
  }

  return new (Ctx)
      TypeRefinementContext(Ctx, SF, parentContext, range, availabilityRange,
                            AvailabilityRange::alwaysAvailable());
}

TypeRefinementContext *TypeRefinementContext::createForDecl(
    ASTContext &Ctx, Decl *D, TypeRefinementContext *Parent,
    const AvailabilityRange &Info, const AvailabilityRange &ExplicitInfo,
    SourceRange SrcRange) {
  assert(D);
  assert(Parent);
  return new (Ctx)
      TypeRefinementContext(Ctx, D, Parent, SrcRange, Info, ExplicitInfo);
}

TypeRefinementContext *TypeRefinementContext::createForDeclImplicit(
    ASTContext &Ctx, Decl *D, TypeRefinementContext *Parent,
    const AvailabilityRange &Info, SourceRange SrcRange) {
  assert(D);
  assert(Parent);
  return new (Ctx) TypeRefinementContext(
      Ctx, IntroNode(D, Reason::DeclImplicit), Parent, SrcRange, Info,
      AvailabilityRange::alwaysAvailable());
}

TypeRefinementContext *
TypeRefinementContext::createForIfStmtThen(ASTContext &Ctx, IfStmt *S,
                                           TypeRefinementContext *Parent,
                                           const AvailabilityRange &Info) {
  assert(S);
  assert(Parent);
  return new (Ctx)
      TypeRefinementContext(Ctx, IntroNode(S, /*IsThen=*/true), Parent,
                            S->getThenStmt()->getSourceRange(),
                            Info, /* ExplicitInfo */Info);
}

TypeRefinementContext *
TypeRefinementContext::createForIfStmtElse(ASTContext &Ctx, IfStmt *S,
                                           TypeRefinementContext *Parent,
                                           const AvailabilityRange &Info) {
  assert(S);
  assert(Parent);
  return new (Ctx)
      TypeRefinementContext(Ctx, IntroNode(S, /*IsThen=*/false), Parent,
                            S->getElseStmt()->getSourceRange(),
                            Info, /* ExplicitInfo */Info);
}

TypeRefinementContext *TypeRefinementContext::createForConditionFollowingQuery(
    ASTContext &Ctx, PoundAvailableInfo *PAI,
    const StmtConditionElement &LastElement, TypeRefinementContext *Parent,
    const AvailabilityRange &Info) {
  assert(PAI);
  assert(Parent);
  SourceRange Range(PAI->getEndLoc(), LastElement.getEndLoc());
  return new (Ctx) TypeRefinementContext(Ctx, PAI, Parent, Range,
                                         Info, /* ExplicitInfo */Info);
}

TypeRefinementContext *TypeRefinementContext::createForGuardStmtFallthrough(
    ASTContext &Ctx, GuardStmt *RS, BraceStmt *ContainingBraceStmt,
    TypeRefinementContext *Parent, const AvailabilityRange &Info) {
  assert(RS);
  assert(ContainingBraceStmt);
  assert(Parent);
  SourceRange Range(RS->getEndLoc(), ContainingBraceStmt->getEndLoc());
  return new (Ctx) TypeRefinementContext(Ctx,
                                         IntroNode(RS, /*IsFallthrough=*/true),
                                         Parent, Range, Info, /* ExplicitInfo */Info);
}

TypeRefinementContext *
TypeRefinementContext::createForGuardStmtElse(ASTContext &Ctx, GuardStmt *RS,
                                              TypeRefinementContext *Parent,
                                              const AvailabilityRange &Info) {
  assert(RS);
  assert(Parent);
  return new (Ctx)
      TypeRefinementContext(Ctx, IntroNode(RS, /*IsFallthrough=*/false), Parent,
                            RS->getBody()->getSourceRange(), Info, /* ExplicitInfo */Info);
}

TypeRefinementContext *
TypeRefinementContext::createForWhileStmtBody(ASTContext &Ctx, WhileStmt *S,
                                              TypeRefinementContext *Parent,
                                              const AvailabilityRange &Info) {
  assert(S);
  assert(Parent);
  return new (Ctx) TypeRefinementContext(
      Ctx, S, Parent, S->getBody()->getSourceRange(), Info, /* ExplicitInfo */Info);
}

/// Determine whether the child location is somewhere within the parent
/// range.
static bool rangeContainsTokenLocWithGeneratedSource(
    SourceManager &sourceMgr, SourceRange parentRange, SourceLoc childLoc) {
  if (sourceMgr.rangeContainsTokenLoc(parentRange, childLoc))
    return true;

  auto childBuffer = sourceMgr.findBufferContainingLoc(childLoc);
  while (!sourceMgr.rangeContainsTokenLoc(
      sourceMgr.getRangeForBuffer(childBuffer), parentRange.Start)) {
    auto *info = sourceMgr.getGeneratedSourceInfo(childBuffer);
    if (!info)
      return false;

    childLoc = info->originalSourceRange.getStart();
    if (childLoc.isInvalid())
      return false;

    childBuffer = sourceMgr.findBufferContainingLoc(childLoc);
  }

  return sourceMgr.rangeContainsTokenLoc(parentRange, childLoc);
}

TypeRefinementContext *
TypeRefinementContext::findMostRefinedSubContext(SourceLoc Loc,
                                                 ASTContext &Ctx) {
  assert(Loc.isValid());

  if (SrcRange.isValid() &&
      !rangeContainsTokenLocWithGeneratedSource(Ctx.SourceMgr, SrcRange, Loc))
    return nullptr;

  auto expandedChildren = evaluateOrDefault(
      Ctx.evaluator, ExpandChildTypeRefinementContextsRequest{this}, {});

  // For the moment, we perform a linear search here, but we can and should
  // do something more efficient.
  for (TypeRefinementContext *Child : expandedChildren) {
    if (auto *Found = Child->findMostRefinedSubContext(Loc, Ctx)) {
      return Found;
    }
  }

  // Loc is in this context's range but not in any child's, so this context
  // must be the inner-most context.
  return this;
}

void TypeRefinementContext::dump(SourceManager &SrcMgr) const {
  dump(llvm::errs(), SrcMgr);
}

void TypeRefinementContext::dump(raw_ostream &OS, SourceManager &SrcMgr) const {
  print(OS, SrcMgr, 0);
  OS << '\n';
}

SourceLoc TypeRefinementContext::getIntroductionLoc() const {
  switch (getReason()) {
  case Reason::Decl:
  case Reason::DeclImplicit:
    return Node.getAsDecl()->getLoc();

  case Reason::IfStmtThenBranch:
  case Reason::IfStmtElseBranch:
    return Node.getAsIfStmt()->getIfLoc();

  case Reason::ConditionFollowingAvailabilityQuery:
    return Node.getAsPoundAvailableInfo()->getStartLoc();

  case Reason::GuardStmtFallthrough:
  case Reason::GuardStmtElseBranch:
    return Node.getAsGuardStmt()->getGuardLoc();


  case Reason::WhileStmtBody:
    return Node.getAsWhileStmt()->getStartLoc();

  case Reason::Root:
    return SourceLoc();
  }

  llvm_unreachable("Unhandled Reason in switch.");
}

static SourceRange
getAvailabilityConditionVersionSourceRange(const PoundAvailableInfo *PAI,
                                           PlatformKind Platform,
                                           const llvm::VersionTuple &Version) {
  SourceRange Range;
  for (auto *S : PAI->getQueries()) {
    if (auto *V = dyn_cast<PlatformVersionConstraintAvailabilitySpec>(S)) {
      if (V->getPlatform() == Platform && V->getVersion() == Version) {
        // More than one: return invalid range, no unique choice.
        if (Range.isValid())
          return SourceRange();
        else
          Range = V->getVersionSrcRange();
      }
    }
  }
  return Range;
}

static SourceRange
getAvailabilityConditionVersionSourceRange(
    const MutableArrayRef<StmtConditionElement> &Conds,
    PlatformKind Platform,
    const llvm::VersionTuple &Version) {
  SourceRange Range;
  for (auto const& C : Conds) {
    if (C.getKind() == StmtConditionElement::CK_Availability) {
      SourceRange R = getAvailabilityConditionVersionSourceRange(
        C.getAvailability(), Platform, Version);
      // More than one: return invalid range.
      if (Range.isValid())
        return SourceRange();
      else
        Range = R;
    }
  }
  return Range;
}

static SourceRange
getAvailabilityConditionVersionSourceRange(const DeclAttributes &DeclAttrs,
                                           PlatformKind Platform,
                                           const llvm::VersionTuple &Version) {
  SourceRange Range;
  for (auto *Attr : DeclAttrs) {
    if (auto *AA = dyn_cast<AvailableAttr>(Attr)) {
      if (AA->Introduced.has_value() &&
          AA->Introduced.value() == Version &&
          AA->Platform == Platform) {

        // More than one: return invalid range.
        if (Range.isValid())
          return SourceRange();
        else
          Range = AA->IntroducedRange;
      }
    }
  }
  return Range;
}

SourceRange
TypeRefinementContext::getAvailabilityConditionVersionSourceRange(
    PlatformKind Platform,
    const llvm::VersionTuple &Version) const {
  switch (getReason()) {
  case Reason::Decl:
    return ::getAvailabilityConditionVersionSourceRange(
      Node.getAsDecl()->getAttrs(), Platform, Version);

  case Reason::IfStmtThenBranch:
  case Reason::IfStmtElseBranch:
    return ::getAvailabilityConditionVersionSourceRange(
      Node.getAsIfStmt()->getCond(), Platform, Version);

  case Reason::ConditionFollowingAvailabilityQuery:
    return ::getAvailabilityConditionVersionSourceRange(
      Node.getAsPoundAvailableInfo(), Platform, Version);

  case Reason::GuardStmtFallthrough:
  case Reason::GuardStmtElseBranch:
    return ::getAvailabilityConditionVersionSourceRange(
      Node.getAsGuardStmt()->getCond(), Platform, Version);

  case Reason::WhileStmtBody:
    return ::getAvailabilityConditionVersionSourceRange(
      Node.getAsWhileStmt()->getCond(), Platform, Version);

  case Reason::Root:
  case Reason::DeclImplicit:
    return SourceRange();
  }

  llvm_unreachable("Unhandled Reason in switch.");
}

static std::string
stringForAvailability(const AvailabilityRange &availability) {
  if (availability.isAlwaysAvailable())
    return "all";
  if (availability.isKnownUnreachable())
    return "none";

  return availability.getVersionString();
}

void TypeRefinementContext::print(raw_ostream &OS, SourceManager &SrcMgr,
                                  unsigned Indent) const {
  OS.indent(Indent);
  OS << "(" << getReasonName(getReason());

  OS << " version=" << stringForAvailability(AvailabilityInfo);

  if (getReason() == Reason::Decl || getReason() == Reason::DeclImplicit) {
    Decl *D = Node.getAsDecl();
    OS << " decl=";
    if (auto VD = dyn_cast<ValueDecl>(D)) {
      OS << VD->getName();
    } else if (auto *ED = dyn_cast<ExtensionDecl>(D)) {
      OS << "extension." << ED->getExtendedType().getString();
    } else if (isa<TopLevelCodeDecl>(D)) {
      OS << "<top-level-code>";
    } else if (auto PBD = dyn_cast<PatternBindingDecl>(D)) {
      if (auto VD = PBD->getAnchoringVarDecl(0)) {
        OS << VD->getName();
      }
    }
  }

  auto R = getSourceRange();
  if (R.isValid()) {
    OS << " src_range=";
    R.print(OS, SrcMgr, /*PrintText=*/false);
  }

  if (!ExplicitAvailabilityInfo.isAlwaysAvailable())
    OS << " explicit_version="
       << stringForAvailability(ExplicitAvailabilityInfo);

  for (TypeRefinementContext *Child : Children) {
    OS << '\n';
    Child->print(OS, SrcMgr, Indent + 2);
  }
  OS.indent(Indent);
  OS << ")";
}

TypeRefinementContext::Reason TypeRefinementContext::getReason() const {
  return Node.getReason();
}

StringRef TypeRefinementContext::getReasonName(Reason R) {
  switch (R) {
  case Reason::Root:
    return "root";

  case Reason::Decl:
    return "decl";

  case Reason::DeclImplicit:
    return "decl_implicit";

  case Reason::IfStmtThenBranch:
    return "if_then";

  case Reason::IfStmtElseBranch:
    return "if_else";

  case Reason::ConditionFollowingAvailabilityQuery:
    return "condition_following_availability";

  case Reason::GuardStmtFallthrough:
    return "guard_fallthrough";

  case Reason::GuardStmtElseBranch:
    return "guard_else";

  case Reason::WhileStmtBody:
    return "while_body";
  }

  llvm_unreachable("Unhandled Reason in switch.");
}

void swift::simple_display(
    llvm::raw_ostream &out, const TypeRefinementContext *trc) {
  out << "TRC @" << trc;
}

std::optional<std::vector<TypeRefinementContext *>>
ExpandChildTypeRefinementContextsRequest::getCachedResult() const {
  auto *TRC = std::get<0>(getStorage());
  if (TRC->getNeedsExpansion())
    return std::nullopt;
  return TRC->Children;
}

void ExpandChildTypeRefinementContextsRequest::cacheResult(
    std::vector<TypeRefinementContext *> children) const {
  auto *TRC = std::get<0>(getStorage());
  TRC->Children = children;
  TRC->setNeedsExpansion(false);
}
