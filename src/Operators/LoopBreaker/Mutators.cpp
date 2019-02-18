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
/// \author Andrea Aletto
/// \brief Loopbreaker mutator implementation
//===----------------------------------------------------------------------===//

#include "Operators/LoopBreaker/Mutators.h"
#include "llvm/Support/ErrorHandling.h"

#include "Log.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/ErrorHandling.h"
#include <iostream>

#define DEBUG_TYPE "mutator_loopbreaker"

using namespace clang;
using namespace clang::ast_matchers;
using namespace chimera;
using namespace chimera::mutator;
using namespace clang::ast_type_traits;

#define PARENT_NODE_TYPE(res_matcher, child)                                   \
  (res_matcher.Context->getParents(*child))[0].getNodeKind().asStringRef()

#define GET_PARENT_NODE(res_matcher, child, casting_type)                      \
  ((res_matcher.Context->getParents(*child))[0]).get<casting_type>()

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


/// \brief It is used to retrieve the node, it hides the binding string.
///        In Mutator.h there is code template snippet that should be fine for
///        all the majority of the cases.
bool chimera::loopbreaker::MutatorLoopBreaker::getMatchedNode(const NodeType &node, DynTypedNode &dynNode) {
  const ForStmt *forStmt = node.Nodes.getNodeAs<ForStmt>("outer_for_stmt");
  assert(forStmt && "ForStmt is nullptr");
  if (forStmt != nullptr) {
    dynNode = DynTypedNode::create(*forStmt);
    return true;
  } else
    return false;
}

/// \brief  This method implements the coarse grained matching rules,
///         returning the statement matcher to match the inner for loop
StatementMatcher chimera::loopbreaker::MutatorLoopBreaker::getStatementMatcher() {
  return stmt(
    forStmt(
      unless(hasAncestor(forStmt()))
    ).bind("outer_for_stmt")
  );
}

/// \brief  Coarse grain is enough to identify the inner for loop
bool chimera::loopbreaker::MutatorLoopBreaker::match(const NodeType &node) {
    return true;
}

Rewriter &chimera::loopbreaker::MutatorLoopBreaker::mutate(const NodeType &node, MutatorType type, Rewriter &rw) {

    // Retrieve a pointer to function declaration (or template function declaration) to insert global variables before it
    const FunctionDecl *funDecl = node.Nodes.getNodeAs<FunctionDecl>("functionDecl");
    const FunctionTemplateDecl *templDecl = (FunctionTemplateDecl*)(GET_PARENT_NODE(node, funDecl, FunctionTemplateDecl));

    // const ForStmt *innerFor = node.Nodes.getNodeAs<ForStmt>("outer_for_stmt");

    // if(innerFor) {
    //   DEBUG(::llvm::dbgs()  << "\n****************************************************\n");
    //   const Expr* cond = innerFor->getCond();
    //   std::string condString = rw.getRewrittenText(cond->getSourceRange());
    //   DEBUG(::llvm::dbgs()  << condString );
    //   DEBUG(::llvm::dbgs()  << "\n****************************************************\n");
    // }

    // Set the operation number
    unsigned int bopNum = this->operationCounter++;
    // Local rewriter to hold the original code
    Rewriter oriRw(*(node.SourceManager), node.Context->getLangOpts());

    // Retrieve binary operation, left and right hand side
    ForStmt *forStmt   = (ForStmt*) node.Nodes.getNodeAs<ForStmt>("outer_for_stmt");
    Expr *cond     = (Expr*) forStmt->getCond();

    // Create a global var before the function
    ::std::string baseId = "base_" + ::std::to_string(bopNum++);

    if(templDecl != NULL) 
      rw.InsertTextBefore(templDecl->getSourceRange().getBegin(), "int " + baseId + " = 8;\n");
    else                  
      rw.InsertTextBefore(funDecl->getSourceRange().getBegin(), "int " + baseId + " = 8;\n");

    // Assert that binary operator and Xhs are not null
    assert (forStmt && "Outer ForStatement is nullptr"); 

    // Retrieve the condition
    ::std::string condString = rw.getRewrittenText(cond->getSourceRange());
    ::std::string condVariableString = condString.substr(0, condString.find("<"));

    // Start collecting information for the report (everything but the return variable):
    MutatorLoopBreaker::MutationInfo mutationInfo;

    // * Operation Identifier
    mutationInfo.baseId = baseId;

    // Save info into the report
    this->mutationsInfo.push_back(mutationInfo);

    // * Line location
    FullSourceLoc loc(forStmt->getSourceRange().getBegin(), *(node.SourceManager));
    mutationInfo.line = loc.getSpellingLineNumber();

    // Form the replacing string
    ::std::string condReplacement = condVariableString + " < " + baseId; 
      
    //////////////////////////////////////////////////////////////////////////////////////////
    // Debug
    DEBUG(::llvm::dbgs()  << "****************************************************"
                            "****\nDump for loop:\n");
    DEBUG(::llvm::dbgs()  << "Statement: "
                          << rw.getRewrittenText(forStmt->getSourceRange()) << "]\n");
    DEBUG(::llvm::dbgs()  << "Condition: "            << condString << "\n");
    DEBUG(::llvm::dbgs()  << "Condition Variable: "   << condVariableString << "\n");
    DEBUG(::llvm::dbgs()  << "Mutate condition in: "  << condReplacement << "\n");
    DEBUG(::llvm::dbgs()  << "****************************************************\n\n");

    ////////////////////////////////////////////////////////////////////////////////////////// 

    // Replace all the text of the binary operator with a function call
    rw.ReplaceText(cond->getSourceRange(), condReplacement); 

    // Stop if the current node (bop) has no parents
    assert( ((ForStmt*)forStmt->getBody()) && "No more for statement children.");

    forStmt = (ForStmt*)forStmt->getBody()->IgnoreContainers();
    cond = (Expr*) forStmt->getCond();

    ::std::string innerCondString = rw.getRewrittenText(cond->getSourceRange());
    ::std::string innerCondVariableString = innerCondString.substr(0, innerCondString.find("<"));
  
    condReplacement = innerCondVariableString + " < " + baseId + " - " + condVariableString; 
    
  
    //////////////////////////////////////////////////////////////////////////////////////////
    // Debug
    DEBUG(::llvm::dbgs()  << "****************************************************"
                            "****\nDump inner for loop:\n");
    DEBUG(::llvm::dbgs()  << "Statement: "
                          << rw.getRewrittenText(forStmt->getSourceRange()) << "]\n");
    DEBUG(::llvm::dbgs()  << "Condition: "            << innerCondString << "\n");
    DEBUG(::llvm::dbgs()  << "Condition Variable: "   << innerCondVariableString << "\n");
    DEBUG(::llvm::dbgs()  << "Mutate condition in: "  << condReplacement << "\n");
    DEBUG(::llvm::dbgs()  << "****************************************************\n\n");

    ////////////////////////////////////////////////////////////////////////////////////////// 

    // Replace all the text of the binary operator with a function call
    rw.ReplaceText(cond->getSourceRange(), condReplacement); 







    // //Info for the report
    // bool isLhsBinaryOp = ::llvm::isa<BinaryOperator>(internalLhs);
    // bool isRhsBinaryOp = ::llvm::isa<BinaryOperator>(internalRhs);
    // ::std::string retVar = "NULL";

    // // Traverse the AST from the last add to the others 
    // BinaryOperator *bopParent = (BinaryOperator*)bop;

    // while( 
    //     ( bopParent != NULL ) &&
    //     ( !(node.Context->getParents(*bopParent)).empty() ) && 
    //     ( ( ((BinaryOperator*) (node.Context->getParents(*bopParent)[0]).get<BinaryOperator>()) ) != NULL) &&
    //     ( ( ((BinaryOperator*) (node.Context->getParents(*bopParent)[0]).get<BinaryOperator>())->getOpcodeStr() ) == "+") 
    // ){ 
    //     // Iterate if:
    //     //      1) the node bopParent exists        AND
    //     //      2) the node bopParent has a parent  AND
    //     //      3) this parent of bopParent is a BinaryOperator (aka casting to BinaryOperator* succeded) AND
    //     //      4) this parent of bopParent is a +

    //     // Assign to bopParent its parent (for the next iteration)
    //     bopParent = (BinaryOperator*) (node.Context->getParents(*bopParent)[0]).get<BinaryOperator>();

    //     // Retrieve text information for the current operator
    //     nabId = "nab_" + ::std::to_string(bopNum++);        
    //     rhsString = rw.getRewrittenText(bopParent->getRHS()->getSourceRange());
    //     lhsString = rw.getRewrittenText(bopParent->getLHS()->getSourceRange());

    //     rw.InsertTextBefore(funDecl->getSourceRange().getBegin(), "int " + nabId + " = 0;\n");
    //     bopReplacement = "inax1_sum(" + nabId + ", " + lhsString + ", " + rhsString + ")";

    //     rw.ReplaceText(bopParent->getSourceRange(), bopReplacement);  

    //     ////////////////////////////////////////////////////////////////////////////////////////////
    //     /// Debug
    //     DEBUG(::llvm::dbgs() << "****************************************************"
    //                             "****\nDump binary operation:\n");
    //     DEBUG(::llvm::dbgs() << "Operation: "
    //                         << rw.getRewrittenText(bopParent->getSourceRange()) << " ==> ["
    //                         << bopParent->getOpcodeStr() << "]\n");
    //     DEBUG(::llvm::dbgs() << "LHS: " << lhsString << "\n");
    //     DEBUG(::llvm::dbgs() << "RHS: " << rhsString << "\n");
    //     ////////////////////////////////////////////////////////////////////////////////////////////

    // }

    this->operationCounter = bopNum;

    // ::std::vector<Expr*> args;
    // //args->push_back(nab);
    // args.push_back((Expr*) lhs);
    // args.push_back((Expr*) rhs);

    // QualType t;
    // Expr* fn = NULL;

    // CallExpr func(
    //   *node.Context, 
    //   fn, 
    //   args, 
    //   t,
    //   VK_LValue, 
    //   bop->getLocEnd()
    // );



    

    
    return rw;


}

void chimera::loopbreaker::MutatorLoopBreaker::onCreatedMutant(const ::std::string &mDir) {}
//   // Create a specific report inside the mutant directory
//   ::std::error_code error;
//   ::llvm::raw_fd_ostream report(mDir + "inax1_report.csv", error,
//                                 ::llvm::sys::fs::OpenFlags::F_Text);
//   // Resolve operand/operation information, substituting the binary operator
//   // with the code of the I type operation
//   // This operation, due to the unknown order of processing, has to be performed
//   // here
//   // Make a copy to always read the old operand string
//   // FIXME: Check also operation type -> store binaryOperator pointer to
//   // compare?
//   ::std::vector<MutationInfo> cMutationsInfo = this->mutationsInfo;
//   // for (auto &mI : cMutationsInfo) {
//   //   if (mI.op1OpTy != NoOp) {
//   //     // Operand 1 is a binary operation
//   //     // Search in all info
//   //     for (const auto &mII : this->mutationsInfo) {
//   //       // Check that isn't the same mutationInfo
//   //       if (mII.nabId != mI.nabId) {
//   //         // Search both operand inside mI.op1, if they are both found AND
//   //         // the operation between them is mII.opTy there is a match.
//   //         // Search from the end of op1 and begin of op2 the OpcodeStr of mII
//   //         auto op1inOp = ::std::search(mI.op1.begin(), mI.op1.end(),
//   //                                      mII.op1.begin(), mII.op1.end());
//   //         auto op2inOp = ::std::search(mI.op1.begin(), mI.op1.end(),
//   //                                      mII.op2.begin(), mII.op2.end());
//   //         if (op1inOp != mI.op1.end() && op2inOp != mI.op1.end() &&
//   //             ::std::find(op1inOp + mII.op1.size() - 1, op2inOp,
//   //                         BinaryOperator::getOpcodeStr(mII.opTy).data()[0]) !=
//   //                 mI.op1.end()) {
//   //           DEBUG(::llvm::dbgs() << "Operand/operation: " << mI.op1
//   //                                << " IS Operation: " << mII.nabId << "\n");
//   //           mI.op1 = mII.nabId; // found the new label
//   //           break;
//   //         }
//   //       }
//   //     }
//   //   }
//   //   if (mI.op2OpTy != NoOp) {
//   //     // Operand 2 is a binary operation
//   //     // Search in all info
//   //     for (const auto &mII : this->mutationsInfo) {
//   //       // Check that isn't the same mutationInfo
//   //       if (mII.nabId != mI.nabId) {
//   //         // Search both operand inside mI.op1, if they are both found AND
//   //         // the operation between them is mII.opTy there is a match.
//   //         // Search from the end of op1 and begin of op2 the OpcodeStr of mII
//   //         auto op1inOp = ::std::search(mI.op2.begin(), mI.op2.end(),
//   //                                      mII.op1.begin(), mII.op1.end());
//   //         auto op2inOp = ::std::search(mI.op2.begin(), mI.op2.end(),
//   //                                      mII.op2.begin(), mII.op2.end());
//   //         if (op1inOp != mI.op2.end() && op2inOp != mI.op2.end() &&
//   //             ::std::find(op1inOp + mII.op1.size() - 1, op2inOp,
//   //                         BinaryOperator::getOpcodeStr(mII.opTy).data()[0]) !=
//   //                 mI.op1.end()) {
//   //           DEBUG(::llvm::dbgs() << "Operand/operation: " << mI.op2
//   //                                << " IS Operation: " << mII.nabId << "\n");
//   //           mI.op2 = mII.nabId; // found the new label
//   //           break;
//   //         }
//   //       }
//   //     }
//   //   }
//   // }

//   // Now resolve the retVar, that is where an operation produce a retVar that is
//   // used as input in a
//   // following operation, the two are dependant. So the input var of the latter
//   // operation can be substituted
//   // with the operationId of the first.
//   // The entries are ordered as location of occurrence, starting from the end it
//   // is necessary to see if
//   // an operand that is not a binary operation occurrs as retVar of previous
//   // operation
//   // for (auto rIt = cMutationsInfo.rbegin(), rEnd = cMutationsInfo.rend();
//   //      rIt != rEnd; ++rIt) {
//   //   // Operand 1
//   //   if (rIt->op1OpTy == NoOp) {
//   //     auto &localOp = rIt->op1;
//   //     // loop on the remaining operation
//   //     for (auto rIt2 = rIt + 1; rIt2 != rEnd; rIt2++) {
//   //       // Check if operand 1 is a retVar for anyone of them
//   //       if (rIt2->retOp != "NULL" && localOp == rIt2->retOp) {
//   //         DEBUG(::llvm::dbgs() << "Operand: " << localOp
//   //                              << " IS Operation: " << rIt2->nabId << "\n");
//   //         localOp = rIt2->nabId; // new label
//   //         break;
//   //       }
//   //     }
//   //   }
//   //   if (rIt->op2OpTy == NoOp) {
//   //     // Operand 2
//   //     auto &localOp = rIt->op2;
//   //     // loop on the remaining operation
//   //     for (auto rIt2 = rIt + 1; rIt2 != rEnd; rIt2++) {
//   //       // Check if operand 1 is a retVar for anyone of them
//   //       if (rIt2->retOp != "NULL" && localOp == rIt2->retOp) {
//   //         DEBUG(::llvm::dbgs() << "Operand: " << localOp
//   //                              << " IS Operation: " << rIt2->nabId << "\n");
//   //         localOp = rIt2->nabId; // new label
//   //         break;
//   //       }
//   //     }
//   //   }
//   // }

//   // for (const auto& mutationInfo : this->mutationsInfo) {
//   for (const auto &mutationInfo : cMutationsInfo) {
//     report << mutationInfo.nabId << "," << mutationInfo.line << ","
//            << "\"" << mutationInfo.op1 << "\","
//            << "\"" << mutationInfo.op2 << "\","
//            << "\"" << mutationInfo.retOp << "\"\n";
//   }
//   report.close();
// }