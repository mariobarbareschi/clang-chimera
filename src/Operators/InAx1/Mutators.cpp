//===- Mutators.cpp ---------------------------------------------*- C++ -*-===//
//
//  Copyright (C) 2015, 2016  Federico Iannucci (fed.iannucci@gmail.com)
//
//  This file is part of Clang-Chimera.
//
//  Clang-Chimera is free software: you can redistribute it and/or modify
//  it under the terms of the GNU Affero General Public License as published by
//  the Free Software Foundation, either version 3 of the License, or
//  (at your option) any later version.
//
//  Clang-Chimera is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU Affero General Public License for more details.
//
//  You should have received a copy of the GNU Affero General Public License
//  along with Clang-Chimera. If not, see <http://www.gnu.org/licenses/>.
//
//===----------------------------------------------------------------------===//
/// \file Mutators.cpp
/// \author Federico Iannucci
/// \brief This file contains sample mutators
//===----------------------------------------------------------------------===//

#include "Operators/InAx1/Mutators.h"
#include "llvm/Support/ErrorHandling.h"

#include "Log.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/ErrorHandling.h"
#include <iostream>

#define DEBUG_TYPE "mutator_iideaa"

using namespace clang;
using namespace clang::ast_matchers;

#define XHS_INTERNAL_MATCHER(id)                                               \
  ignoringParenImpCasts(expr().bind(                                           \
      id)) //< x hand side matcher without parenthesis and implicit casts

// Retrieve hs without implicit cast and parenthesis,
// also bypassing explicit casting, this is done arriving to the explicit cast
// and then going on the child (has), where the above rule is applied again
#define XHS_MATCHER(type, id)                                                  \
  allOf(hasType(qualType(hasCanonicalType(asString(type)))),                   \
        anyOf(XHS_INTERNAL_MATCHER(id),                                        \
              ignoringParenImpCasts(                                           \
                  castExpr(has(expr(XHS_INTERNAL_MATCHER(id)))))))

// static ::std::string mapOpCode(::clang::BinaryOperator::Opcode code) {
//   ::std::string retString = "";
//   switch (code) {
//   case BO_Add:
//   case BO_AddAssign:
//     retString = "ADD";
//     break;
//   case BO_Sub:
//   case BO_SubAssign:
//     retString = "SUB";
//     break;
//   case BO_Mul:
//   case BO_MulAssign:
//     retString = "MUL";
//     break;
//   case BO_Div:
//   case BO_DivAssign:
//     retString = "DIV";
//     break;
//   default:
//     llvm_unreachable("OpCode unsupported");
//   }
//   return retString;
// }

// static ::std::string castFlapFloat(const ::std::string &xhs,
//                                    const ::std::string &opType,
//                                    const ::std::string &precId) {
//   return "(" + opType + ")(::fap::FloatingPointType(" + "(" + opType + ") " +
//          xhs + ", " + precId + "))";
// }

/// @brief Apply the casting logic on a hand side
/// @param rw Rewriter
/// @param xhs Hand side to cast
/// @param type Cast type
/// @param precId FLAP specific parameter
// static void castFlapFloat(Rewriter &rw, const Expr *xhs,
//                           const ::std::string &type,
//                           const ::std::string &precId) {
//   SourceRange range = xhs->getSourceRange();
//   rw.InsertTextBefore(range.getBegin(), "(" + type +
//                                             ")(::fap::FloatingPointType(" +
//                                             "(" + type + ") ");
//   rw.InsertTextAfterToken(range.getEnd(), ", " + precId + "))");
// }
/// \brief It is used to retrieve the node, it hides the binding string.
///        In Mutator.h there is code template snippet that should be fine for
///        all the majority of the cases.
bool chimera::inax1::MutatorInAx1::getMatchedNode(
    const chimera::mutator::NodeType &node,
    clang::ast_type_traits::DynTypedNode &dynNode) {
  const BinaryOperator *Op = node.Nodes.getNodeAs<BinaryOperator>("inax1_op");
  assert(Op && "BinaryOperator is nullptr");
  if (Op != nullptr) {
    dynNode = ast_type_traits::DynTypedNode::create(*Op);
    return true;
  } else
    return false;
}

/// \brief This method returns the statement matcher to match the binary
/// operation
::clang::ast_matchers::StatementMatcher
chimera::inax1::MutatorInAx1::getStatementMatcher() {
  // It has to match a binary operation with a specific operator name (>). In
  // order to retrieve the match, it is necessary to bind a string in this case
  // "op".
  // But we want to avoid matches in for loops, so in this phase the mather has
  // to gather information about the surroundings, i.e. if the binary operation
  // is inside a for loop, this is done checking the ancestor. Such condition is
  // OPTIONAL, indeed it is used a little trick using anything.
  return stmt(
    binaryOperator(hasOperatorName("+"),
      hasRHS(XHS_MATCHER("int", "rhs"))
    ).bind("inax1_op"),
    
    binaryOperator(hasOperatorName("+"),
      hasLHS(XHS_MATCHER("int", "lhs"))
    ).bind("inax1_op")
  );
}

/// \brief This method implements the fine grained matching rules, indeed it is
///        not possible in an easy way to specify that the node matched it has
///        not to be part of the condition expression of a for statement
bool chimera::inax1::MutatorInAx1::match(
    const ::chimera::mutator::NodeType &node) {
  // First operation: Retrieve the node
  const BinaryOperator *bop =
      node.Nodes.getNodeAs<BinaryOperator>("inax1_op");
  assert(bop && "BinaryOperator is nullptr");

  // // In order to see if the operation is not part of the condition expression,
  // // it is simply checked if the operation position is not in the range of such
  // // expression
  // SourceRange bopRange = bop->getSourceRange();
  // // IF a construct has been matched
  // // If stmt
  // const ForStmt *forStmt = node.Nodes.getNodeAs<ForStmt>("forStmt");
  // // Check if there is forStmt
  // if (forStmt != nullptr) {
  //   // Check if it is inside the ExpressionCondition range
  //   if (bopRange.getBegin().getRawEncoding() >=
  //           forStmt->getCond()->getSourceRange().getBegin().getRawEncoding() &&
  //       bopRange.getEnd().getRawEncoding() >=
  //           forStmt->getCond()->getSourceRange().getEnd().getRawEncoding()) {
  //     // The match is invalid, return false
  //     return false;
  //   }
  // }

  // At this point the match is still valid, return true
  return true;
}

::clang::Rewriter &chimera::inax1::MutatorInAx1::mutate(
    const ::chimera::mutator::NodeType &node,
    ::chimera::mutator::MutatorType type, ::clang::Rewriter &rw) {

    // Common operations
    const FunctionDecl *funDecl =
        node.Nodes.getNodeAs<FunctionDecl>("functionDecl");
    // Set the operation number
    unsigned int bopNum = this->operationCounter++;
    // Local rewriter to holds the original code
    Rewriter oriRw(*(node.SourceManager), node.Context->getLangOpts());

    // Retrieve binary operation, left and right hand side
    const BinaryOperator *bop = node.Nodes.getNodeAs<BinaryOperator>("inax1_op");
  
    const Expr *internalLhs = node.Nodes.getNodeAs<Expr>("lhs");
    const Expr *internalRhs = node.Nodes.getNodeAs<Expr>("rhs");
    const Expr *lhs = bop->getLHS()->IgnoreCasts();
    const Expr *rhs = bop->getRHS()->IgnoreCasts();

    assert (bop && "BinaryOperator is nullptr"); 
    assert(internalLhs && "LHS is nullptr");
    assert(internalRhs && "RHS is nullptr");

    ::std::string nabId = "nab_" + ::std::to_string(bopNum);
    // TODO: Add operation type
    // Add to the additional compile commands
    //  this->additionalCompileCommands.push_back("-D" + nabId);

    ::std::string lhsString = rw.getRewrittenText(lhs->getSourceRange());
    ::std::string rhsString = rw.getRewrittenText(rhs->getSourceRange());

    ////////////////////////////////////////////////////////////////////////////////////////////
    /// Debug
    DEBUG(::llvm::dbgs() << "****************************************************"
                            "****\nDump binary operation:\n");
    DEBUG(::llvm::dbgs() << "Operation: "
                        << rw.getRewrittenText(bop->getSourceRange()) << " ==> ["
                        << bop->getOpcodeStr() << "]\n");
    DEBUG(::llvm::dbgs() << "LHS: " << lhsString << "\n");
    DEBUG(::llvm::dbgs() << "RHS: " << rhsString << "\n");
    ////////////////////////////////////////////////////////////////////////////////////////////

    // Create a global var before the function
    rw.InsertTextBefore(funDecl->getSourceRange().getBegin(), "int " + nabId + " = 0;\n");
    //rw.InsertTextBefore(funDecl->getSourceRange().getBegin(), "::fap::FloatPrecTy " + nabId + "(8,23);\n");
    
    bool isLhsBinaryOp = ::llvm::isa<BinaryOperator>(internalLhs);
    bool isRhsBinaryOp = ::llvm::isa<BinaryOperator>(internalRhs);
    ::std::string retVar = "NULL";

  // Replace all the text of the binary operator, substituting the operation
  // (which bind lhs and rhs) with the replacement
    ::std::string bopReplacement = "inax1_sum(" + nabId + "," + lhsString + "," + rhsString + ")";
    rw.ReplaceText(bop->getSourceRange(), bopReplacement);
    //rw.InsertTextBefore(funDecl->getSourceRange().getEnd(), bopReplacement);

      // Get return variable name, if exists
      const BinaryOperator *assignOp =
          node.Nodes.getNodeAs<BinaryOperator>("externalAssignOp");
      if (assignOp != nullptr) {
        // Some assign operation has been matched, narrow down to the really
        // interesting
        // The bop MUST be its RHS
        if (assignOp->getRHS()->IgnoreCasts()->IgnoreParenImpCasts() == bop) {
          ///////////////////////////////////////////////////////////////////////////////
          /// DEBUG
          DEBUG(::llvm::dbgs() << "External assignment operation: "
                              << rw.getRewrittenText(assignOp->getSourceRange())
                              << "\n");
          ///////////////////////////////////////////////////////////////////////////////
          // Check if it is a DeclRef expression
          if (::llvm::isa<DeclRefExpr>(assignOp->getLHS())) {
            retVar = ((const DeclRefExpr *)(assignOp->getLHS()))
                        ->getNameInfo()
                        .getName()
                        .getAsString();
          }
        }
      }
    

    // Store mutations info:
    MutatorInAx1::MutationInfo mutationInfo;
    // * Operation Identifier
    mutationInfo.nabId = nabId;
    // * Line location
    FullSourceLoc loc(bop->getSourceRange().getBegin(), *(node.SourceManager));
    mutationInfo.line = loc.getSpellingLineNumber();
    // * Return type
    mutationInfo.opRetTy = "opRetType";
    // * Operation type
    mutationInfo.opTy = bop->getOpcode();
    // * Information about operands:
    // ** LHS
    ::std::string oriLHS = rw.getRewrittenText(internalLhs->getSourceRange());
    ::std::replace(oriLHS.begin(), oriLHS.end(), '\n', ' ');
    oriLHS.erase(remove_if(oriLHS.begin(), oriLHS.end(), ::isspace),
                oriLHS.end());
    mutationInfo.op1 = oriLHS;
    mutationInfo.op1OpTy = NoOp;
    if (isLhsBinaryOp) {
      mutationInfo.op1OpTy = ((const BinaryOperator *)internalLhs)->getOpcode();
    }
    // ** RHS
    ::std::string oriRHS = rw.getRewrittenText(internalRhs->getSourceRange());
    ::std::replace(oriRHS.begin(), oriRHS.end(), '\n', ' ');
    oriRHS.erase(remove_if(oriRHS.begin(), oriRHS.end(), ::isspace),
                oriRHS.end());
    mutationInfo.op2 = oriRHS;
    mutationInfo.op2OpTy = NoOp;
    if (isRhsBinaryOp) {
      mutationInfo.op2OpTy = ((const BinaryOperator *)internalRhs)->getOpcode();
    }
    // ** Return variable, if exists
    mutationInfo.retOp = retVar;

    this->mutationsInfo.push_back(mutationInfo);

    DEBUG(::llvm::dbgs() << rw.getRewrittenText(bop->getSourceRange()) << "\n");
    return rw;


}

void chimera::inax1::MutatorInAx1::onCreatedMutant(
    const ::std::string &mDir) {
  // Create a specific report inside the mutant directory
  ::std::error_code error;
  ::llvm::raw_fd_ostream report(mDir + "inax1_report.csv", error,
                                ::llvm::sys::fs::OpenFlags::F_Text);
  // Resolve operand/operation information, substituting the binary operator
  // with the code of the I type operation
  // This operation, due to the unknown order of processing, has to be performed
  // here
  // Make a copy to always read the old operand string
  // FIXME: Check also operation type -> store binaryOperator pointer to
  // compare?
  ::std::vector<MutationInfo> cMutationsInfo = this->mutationsInfo;
  for (auto &mI : cMutationsInfo) {
    if (mI.op1OpTy != NoOp) {
      // Operand 1 is a binary operation
      // Search in all info
      for (const auto &mII : this->mutationsInfo) {
        // Check that isn't the same mutationInfo
        if (mII.nabId != mI.nabId) {
          // Search both operand inside mI.op1, if they are both found AND
          // the operation between them is mII.opTy there is a match.
          // Search from the end of op1 and begin of op2 the OpcodeStr of mII
          auto op1inOp = ::std::search(mI.op1.begin(), mI.op1.end(),
                                       mII.op1.begin(), mII.op1.end());
          auto op2inOp = ::std::search(mI.op1.begin(), mI.op1.end(),
                                       mII.op2.begin(), mII.op2.end());
          if (op1inOp != mI.op1.end() && op2inOp != mI.op1.end() &&
              ::std::find(op1inOp + mII.op1.size() - 1, op2inOp,
                          BinaryOperator::getOpcodeStr(mII.opTy).data()[0]) !=
                  mI.op1.end()) {
            DEBUG(::llvm::dbgs() << "Operand/operation: " << mI.op1
                                 << " IS Operation: " << mII.nabId << "\n");
            mI.op1 = mII.nabId; // found the new label
            break;
          }
        }
      }
    }
    if (mI.op2OpTy != NoOp) {
      // Operand 2 is a binary operation
      // Search in all info
      for (const auto &mII : this->mutationsInfo) {
        // Check that isn't the same mutationInfo
        if (mII.nabId != mI.nabId) {
          // Search both operand inside mI.op1, if they are both found AND
          // the operation between them is mII.opTy there is a match.
          // Search from the end of op1 and begin of op2 the OpcodeStr of mII
          auto op1inOp = ::std::search(mI.op2.begin(), mI.op2.end(),
                                       mII.op1.begin(), mII.op1.end());
          auto op2inOp = ::std::search(mI.op2.begin(), mI.op2.end(),
                                       mII.op2.begin(), mII.op2.end());
          if (op1inOp != mI.op2.end() && op2inOp != mI.op2.end() &&
              ::std::find(op1inOp + mII.op1.size() - 1, op2inOp,
                          BinaryOperator::getOpcodeStr(mII.opTy).data()[0]) !=
                  mI.op1.end()) {
            DEBUG(::llvm::dbgs() << "Operand/operation: " << mI.op2
                                 << " IS Operation: " << mII.nabId << "\n");
            mI.op2 = mII.nabId; // found the new label
            break;
          }
        }
      }
    }
  }

  // Now resolve the retVar, that is where an operation produce a retVar that is
  // used as input in a
  // following operation, the two are dependant. So the input var of the latter
  // operation can be substituted
  // with the operationId of the first.
  // The entries are ordered as location of occurrence, starting from the end it
  // is necessary to see if
  // an operand that is not a binary operation occurrs as retVar of previous
  // operation
  for (auto rIt = cMutationsInfo.rbegin(), rEnd = cMutationsInfo.rend();
       rIt != rEnd; ++rIt) {
    // Operand 1
    if (rIt->op1OpTy == NoOp) {
      auto &localOp = rIt->op1;
      // loop on the remaining operation
      for (auto rIt2 = rIt + 1; rIt2 != rEnd; rIt2++) {
        // Check if operand 1 is a retVar for anyone of them
        if (rIt2->retOp != "NULL" && localOp == rIt2->retOp) {
          DEBUG(::llvm::dbgs() << "Operand: " << localOp
                               << " IS Operation: " << rIt2->nabId << "\n");
          localOp = rIt2->nabId; // new label
          break;
        }
      }
    }
    if (rIt->op2OpTy == NoOp) {
      // Operand 2
      auto &localOp = rIt->op2;
      // loop on the remaining operation
      for (auto rIt2 = rIt + 1; rIt2 != rEnd; rIt2++) {
        // Check if operand 1 is a retVar for anyone of them
        if (rIt2->retOp != "NULL" && localOp == rIt2->retOp) {
          DEBUG(::llvm::dbgs() << "Operand: " << localOp
                               << " IS Operation: " << rIt2->nabId << "\n");
          localOp = rIt2->nabId; // new label
          break;
        }
      }
    }
  }

  // for (const auto& mutationInfo : this->mutationsInfo) {
  for (const auto &mutationInfo : cMutationsInfo) {
    report << mutationInfo.nabId << "," << mutationInfo.line << ","
           << mutationInfo.opRetTy << "," << "OPTYPE" << ","
           << "\"" << mutationInfo.op1 << "\","
           << "\"" << mutationInfo.op2 << "\","
           << "\"" << mutationInfo.retOp << "\"\n";
  }
  report.close();
}