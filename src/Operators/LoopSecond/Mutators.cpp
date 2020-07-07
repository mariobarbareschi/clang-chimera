//===- Mutators.cpp ---------------------------------------------*- C++ -*-===//
  //
//  Copyright (C) 2015, 2016  Antonio Tammaro (ntonjeta@autistici.org)
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
/// \author Antonio Tammaro
/// \brief This file contains loop perforation mutator
//===----------------------------------------------------------------------===//

#include "Log.h"
#include "Operators/LoopSecond/Mutators.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/Debug.h"
#include <iostream>

#define DEBUG_TYPE "mutator_iidea"

using namespace clang;
using namespace clang::ast_matchers;
using namespace chimera::log;
using namespace std;

///////////////////////////////////////////////////////////////////////////////
// Flap operation mutator

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
bool chimera::perforation::MutatorLoopPerforation2::getMatchedNode(
    const chimera::mutator::NodeType &node,
    clang::ast_type_traits::DynTypedNode &dynNode)
{
  const ForStmt *Op = node.Nodes.getNodeAs<ForStmt>("for");

  if (Op != nullptr) {
    dynNode = ast_type_traits::DynTypedNode::create(*Op);
    return true;
  } else return false;
  return true;
}

/// \brief This method returns the statement matcher to match the binary
/// operation
::clang::ast_matchers::StatementMatcher
chimera::perforation::MutatorLoopPerforation2::getStatementMatcher()
{
 return stmt(
            // First consider unsigned int decl
            // Match the condition statement (Es. i<n)
            anyOf(binaryOperator(
                    hasParent(forStmt().bind("for")),
                    hasLHS(XHS_MATCHER("unsigned int", "lhs"))).bind("binary_cond"),
                  anything()),
            // Last consider int decl
            // Match the condition statement (Es. i<n)
            anyOf(binaryOperator(
                    hasParent(forStmt().bind("for")),
                    hasLHS(XHS_MATCHER("int", "lhs"))).bind("binary_cond"),
                  anything()),
            // Match case of binary intialization (Es. i = 10)
            anyOf(binaryOperator(
                    hasParent(forStmt().bind("for"))).bind("binary_init"),
                  anything()),
  
            // Match case of unary operand (Es. i++)
            anyOf(unaryOperator(
                    hasParent(forStmt().bind("for"))).bind("unary_op"),
                  anything()),
            // Match case of binary operand (Es. i= i+1)
            anyOf(binaryOperator(
                    hasParent(binaryOperator(
                      hasParent(forStmt().bind("for"))).bind("binary_assign"))).bind("binary_op"),
                  anything())           
        );

}

/// \brief This method implements the fine grained matching rules, indeed it is
///        not possible in an easy way to specify that the node matched it has
///        not to be part of the condition expression of a for statement
bool chimera::perforation::MutatorLoopPerforation2::match(
    const ::chimera::mutator::NodeType &node)
{
  // First operation: Retrieve the node
  const BinaryOperator *bcond = node.Nodes.getNodeAs<BinaryOperator>("binary_cond");
  const UnaryOperator  *uop   = node.Nodes.getNodeAs<UnaryOperator>("unary_op");
  const BinaryOperator *binit = node.Nodes.getNodeAs<BinaryOperator>("binary_init");
  const BinaryOperator *bop   = node.Nodes.getNodeAs<BinaryOperator>("binary_op");
  const BinaryOperator *bas   = node.Nodes.getNodeAs<BinaryOperator>("binary_assign");
  const ForStmt        *fst   = node.Nodes.getNodeAs<ForStmt>("for");

  // Retrive the binary operator 
  if (uop != nullptr)
    if(uop->getLocStart() == fst->getInc()->getLocStart()) 
      this->inc = uop;
  
  // Retrive the binary condition
  if (bcond != nullptr){
    if(bcond->getLocStart() == fst->getCond()->getLocStart()){ this->cond = bcond;
    }// If binary Operator corrispond to Initialization, retrive init. 
    if(bcond->getLocStart() == fst->getInit()->getLocStart()){ this->init = bcond; 
    }  
  }

  if (bcond != nullptr){
    if(bcond->getLocStart() == fst->getCond()->getLocStart()) this->cond = bcond;
    else
      if(bcond->isCompoundAssignmentOp())
        if(bcond->getLocStart() == fst->getInc()->getLocStart()){
          this->binc = bcond;
          this->bas  = bcond;
        }
  }
 
  if (bas != nullptr && bop != nullptr)
    if(bas->getLocStart() == fst->getInc()->getLocStart()){
      this->binc = bop; 
      this->bas  = bas;
    }

  // Retrive the binary condition  
  if (bcond != nullptr)
    if(bcond->getLocStart() == fst->getCond()->getLocStart()) this->cond = bcond;

  // Retrive the binary initializzation
  if(binit != nullptr)
    if(binit->getLocStart() == fst->getInit()->getLocStart())
      this->init = binit;
  
  // Fine grain condition
  // Check if both operator are in same forStmt  
  if (this->inc != nullptr && this->init != nullptr && this->cond != nullptr){
    if(fst->getInc()->getLocStart()  == this->inc->getLocStart() &&
       fst->getInit()->getLocStart() == this->init->getLocStart() &&
       fst->getCond()->getLocStart() == this->cond->getLocStart())
      return true;
  }
  if (this->binc != nullptr && this->bas != nullptr && this->init != nullptr && this->cond != nullptr){
    if(fst->getInc()->getLocStart()  == this->bas->getLocStart() &&
       fst->getInit()->getLocStart() == this->init->getLocStart() &&
       fst->getCond()->getLocStart() == this->cond->getLocStart())
      return true;
  }
  // If operator are from different forStmt return false
  return false;
}


static int  mapOpCode(::clang::BinaryOperator::Opcode code) {
  int  retString;
  switch (code) {
  case BO_Add:
  case BO_AddAssign:
    retString = 1;
    break;
  case BO_Sub:
  case BO_SubAssign:
    retString = 2;
    break;
  case BO_Mul:
  case BO_MulAssign:
    retString = 3;
    break;
  case BO_Div:
  case BO_DivAssign:
    retString = 4;
    break;
  default:
    llvm_unreachable("OpCode unsupported");
  }
  return retString;
}


::clang::Rewriter &chimera::perforation::MutatorLoopPerforation2::mutate(
    const ::chimera::mutator::NodeType &node,
    ::chimera::mutator::MutatorType type, ::clang::Rewriter &rw) 
{   
  bool inc = true; 
  // Local Rewriter to hold the original code
  Rewriter bkrw (*(node.SourceManager), node.Context->getLangOpts());
  // As first operation always retrieve the node
  const ForStmt *fst = node.Nodes.getNodeAs<ForStmt>("for");
  const FunctionDecl *funDecl = node.Nodes.getNodeAs<FunctionDecl>("functionDecl");
  const clang::ASTContext * ctx = node.Context;
  // Assert a precondition
  assert(fst      != nullptr && "getNodeAs returned a nullptr");
  assert(funDecl  != nullptr && "getNodeAs returned a nullptr");
  
  // Insert global variable
  this->opId++; 
  
  rw.InsertTextBefore(funDecl->getSourceRange().getBegin(),"int stride" + to_string(this->opId) + " = 1;\n");

//  rw.InsertTextBefore(fst->getSourceRange().getBegin(),
//                      "stride" + to_string(this->opId) + " = 1;\n");

  // Retrive left operator from condition
  std::string lhs = rw.getRewrittenText(this->cond->getLHS()->getSourceRange());
  // Declare Replacement String
  std::string replacement = "if ( " + lhs + " \% stride" + to_string(this->opId) + " == 0) {";
  // Insert replacement
  rw.InsertTextAfterToken(fst->getRParenLoc(),replacement);
  rw.InsertTextAfterToken(fst->getBody()->getLocEnd(),";}"); 
  //check if is increasing or decreasing for
  
  if(this->binc){
    int r =  mapOpCode(this->binc->getOpcode());  
    if(r == 2) inc = false;
  }else if(this->inc && this->inc->isDecrementOp())
               inc = false;

  //////////////////////////////////////////////////////////////////////////////////////////////////
  // Store mutations info:
  MutatorLoopPerforation2::MutationInfo mutationInfo;
  // * Operation Identifier
  mutationInfo.opId = "stride" + to_string(opId);
  // * Line location
  FullSourceLoc loc(fst->getSourceRange().getBegin(), *(node.SourceManager));
  mutationInfo.line = loc.getSpellingLineNumber();
  // Insert in mutationInfo  

  // Add a for Lenght Parameter  
  //FIXME generalize it ... not only with defined value
  
  clang::Expr::EvalResult  initRHSEvalResult,condRHSEvalResult;
  llvm::APSInt condRHSIval,initRHSIval;

  // Retrive right operator from condition
  clang::Expr* condRHS = this->cond->getRHS();
  // Retrive right operator from initialization 
  clang::Expr* initRHS = this->init->getRHS();
  
  if(initRHS->isEvaluatable(*(ctx)) && condRHS->isEvaluatable(*(ctx))){
    if(initRHS->EvaluateAsInt(initRHSIval,*(ctx))){}
    else if(initRHS->EvaluateAsRValue(initRHSEvalResult,*(ctx)))
            initRHSIval = initRHSEvalResult.Val.getInt(); 
    
    if(condRHS->EvaluateAsInt(condRHSIval,*(ctx))){}
    else if(condRHS->EvaluateAsRValue(condRHSEvalResult,*(ctx)))
            condRHSIval = condRHSEvalResult.Val.getInt();
   
    if(inc){
      mutationInfo.inc = "U";
      mutationInfo.forLenght = condRHSIval.getExtValue() - initRHSIval.getExtValue(); 
    }
    else{
      mutationInfo.inc = "D";
       mutationInfo.forLenght = initRHSIval.getExtValue() - condRHSIval.getExtValue();
    }
  }else{
    if(inc){
      mutationInfo.inc = "U";
      mutationInfo.forLenght = -9999;
    }
    else{
      mutationInfo.inc = "D";
      mutationInfo.forLenght = -9999;
    }

  }
 
  this->mutationsInfo.push_back(mutationInfo);

  DEBUG(::llvm::dbgs() << rw.getRewrittenText(fst->getSourceRange()) << "\n");

  this->cond = this->init = nullptr;
  // Return Rewriter and close functions
  return rw;
}


void ::chimera::perforation::MutatorLoopPerforation2::onCreatedMutant(
    const ::std::string &mDir) {
  // Create a specific report inside the mutant directory
  ::std::error_code error;
  ::llvm::raw_fd_ostream report(mDir + "loop_report.csv", error,
                                ::llvm::sys::fs::OpenFlags::F_Text);
  // Resolve operand/operation information, substituting the binary operator
  // with the code of the I type operation
  // This operation, due to the unknown order of processing, has to be performed
  // here
  // Make a copy to always read the old operand string
  ::std::vector<MutationInfo> cMutationsInfo = this->mutationsInfo;

  for (const auto &mutationInfo : cMutationsInfo){
    report << mutationInfo.opId << ","
           << mutationInfo.line << ","
           << mutationInfo.inc  << ","
           << mutationInfo.forLenght << "\n";
  }
  report.close();
}


