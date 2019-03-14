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
/// \brief This file contains sample mutators
//===----------------------------------------------------------------------===//

#include "Operators/InAx1/Mutators.h"
#include "llvm/Support/ErrorHandling.h"

#include "Log.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/ErrorHandling.h"
#include <iostream>

#define DEBUG_TYPE "mutator_inax1"

using namespace clang;
using namespace clang::ast_matchers;
using namespace chimera;
using namespace chimera::mutator;
using namespace clang::ast_type_traits;
using namespace chimera::log;

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
bool chimera::inax1::MutatorInAx1::getMatchedNode(const NodeType &node, DynTypedNode &dynNode) {
  const BinaryOperator *bop = node.Nodes.getNodeAs<BinaryOperator>("inax1_op");
  assert(bop && "BinaryOperator is nullptr");
  if (bop != nullptr) {
    dynNode = DynTypedNode::create(*bop);
    return true;
  } else
    return false;
}

/// \brief  This method implements the coarse grained matching rules,
///         returning the statement matcher to match the binary
///         operation
StatementMatcher chimera::inax1::MutatorInAx1::getStatementMatcher() {
  // It has to match a binary operation with a specific operator name (+) and 
  // specific operands (int, int).
  // In order to retrieve the match, it is necessary to bind a string in this 
  // case "inax1_op".
  return stmt(
    binaryOperator(
      anyOf(hasOperatorName("+"), hasOperatorName("-")),
      hasRHS(XHS_MATCHER("int", "rhs")),
      hasLHS(XHS_MATCHER("int", "lhs"))
    ).bind("inax1_op"),

    unless(
          anyOf(
            hasAncestor(callExpr()), //uncomment to avoid mutation of input parameters of a function call
            hasAncestor(arraySubscriptExpr()))
    )
  );
}

/// \brief  This method implements the fine grained matching rules, because it is
///         not possible in an easy way to specify that the node matched has
///         to be the last "+"" operator of a chain of adds.
///         In other words we want that for a statement of "x+y+z+t" the matched
///         node has to be the last + (that one between z and t).
bool chimera::inax1::MutatorInAx1::match(const NodeType &node) {

    // First operation: Retrieve the node
    const BinaryOperator *bop = node.Nodes.getNodeAs<BinaryOperator>("inax1_op");
    assert(bop && "BinaryOperator is nullptr");

    // Second operation: extract lhs and rhs
    const Expr *internalLhs = node.Nodes.getNodeAs<Expr>("lhs");
    const Expr *internalRhs = node.Nodes.getNodeAs<Expr>("rhs");
    const Expr *lhs = bop->getLHS()->IgnoreCasts()->IgnoreParens();
    const Expr *rhs = bop->getRHS()->IgnoreCasts()->IgnoreParens();
    assert(internalLhs && "internalLhs is nullptr");
    assert(internalRhs && "internalRhs is nullptr");

    //FIXME: check that internalXhs is not a + and not only a binary operator
    bool isLhsBinaryOp = ::llvm::isa<BinaryOperator>(lhs);
    bool isRhsBinaryOp = ::llvm::isa<BinaryOperator>(rhs);

    // Third operation: discard match if Xhs is a +
    if (isLhsBinaryOp){
        if( (((BinaryOperator*)lhs)->getOpcodeStr() == "+") ||
             ((BinaryOperator*)lhs)->getOpcodeStr() == "-") return false;
    }
    
    if (isRhsBinaryOp){
        if( (((BinaryOperator*)rhs)->getOpcodeStr() == "+") ||
             ((BinaryOperator*)lhs)->getOpcodeStr() == "-") return false;
    }

    ////////////////////////////////////////////////////////////////////////////////////////////
    /// Debug
    char log_string[500];
    Rewriter rw(*(node.SourceManager), node.Context->getLangOpts());
    ChimeraLogger::verbose("********************************************************\nMatched operation:");

    sprintf(log_string, "Operation: %s ==> [%s]", rw.getRewrittenText(bop->getSourceRange()).c_str(),bop->getOpcodeStr().str().c_str());
    ChimeraLogger::verbose(log_string);

    sprintf(log_string, "LHS: %s", rw.getRewrittenText(lhs->getSourceRange()).c_str());
    ChimeraLogger::verbose(log_string);

    sprintf(log_string, "RHS: %s\n", rw.getRewrittenText(rhs->getSourceRange()).c_str());
    ChimeraLogger::verbose(log_string);
    //////////////////////////////////////////////////////////////////////////////////////////// 

    if(rw.getRewrittenText(lhs->getSourceRange()) == "") return false;
    if(rw.getRewrittenText(rhs->getSourceRange()) == "") return false;

    return true;
}

Rewriter &chimera::inax1::MutatorInAx1::mutate(const NodeType &node, MutatorType type, Rewriter &rw) {

    char log_info[500];

    // Retrieve a pointer to function declaration (or template function declaration) to insert global variables before it
    const FunctionDecl *funDecl = node.Nodes.getNodeAs<FunctionDecl>("functionDecl");
    const FunctionTemplateDecl *templDecl = (FunctionTemplateDecl*)(GET_PARENT_NODE(node, funDecl, FunctionTemplateDecl));

    // Set the operation number
    unsigned int bopNum = this->nabCounter++;
    // Local rewriter to hold the original code
    Rewriter oriRw(*(node.SourceManager), node.Context->getLangOpts());

    // Retrieve binary operation, left and right hand side
    BinaryOperator *bop   = (BinaryOperator*) node.Nodes.getNodeAs<BinaryOperator>("inax1_op");
    Expr *internalLhs     = (Expr*)           node.Nodes.getNodeAs<Expr>("lhs");
    Expr *internalRhs     = (Expr*)           node.Nodes.getNodeAs<Expr>("rhs");

    std::string cellId = "cellType_" + ::std::to_string(this->cellTypeCounter++);
    std::string cellStr = "InexactAdderType ";
    cellStr.append(cellId);
    cellStr.append(" = InAx1;\n");

    // Add InexactAdders inclusion and cellType
    if(templDecl != NULL) {
      rw.InsertTextBefore(templDecl->getSourceRange().getBegin(), cellStr.c_str());
      rw.InsertTextBefore(templDecl->getSourceRange().getBegin(), "#include <inexact_adders.h>\n");

      // Information for the report:
      MutatorInAx1::MutationInfo mutationInfo;
      mutationInfo.nabId = cellId;
      FullSourceLoc loc(templDecl->getSourceRange().getBegin(), *(node.SourceManager));
      mutationInfo.line = loc.getSpellingLineNumber();
      this->mutationsInfo.push_back(mutationInfo);  

    } else {                  
      rw.InsertTextBefore(funDecl->getSourceRange().getBegin(), cellStr.c_str());  
      rw.InsertTextBefore(funDecl->getSourceRange().getBegin(), "#include <inexact_adders.h>\n");

      // Information for the report:
      MutatorInAx1::MutationInfo mutationInfo;
      mutationInfo.nabId = cellId;
      FullSourceLoc loc(funDecl->getSourceRange().getBegin(), *(node.SourceManager));
      mutationInfo.line = loc.getSpellingLineNumber();
      this->mutationsInfo.push_back(mutationInfo);  
    }

    
    
  do{
    
    Expr *lhs             = (Expr*)           bop->getLHS()->IgnoreCasts();
    Expr *rhs             = (Expr*)           bop->getRHS()->IgnoreCasts();

    bool isLhsBinaryOp = ::llvm::isa<BinaryOperator>(lhs);
    bool isRhsBinaryOp = ::llvm::isa<BinaryOperator>(rhs);

    // Assert that binary operator and Xhs are not null
    assert (bop && "BinaryOperator is nullptr"); 
    assert (lhs && "LHS is nullptr");
    assert (rhs && "RHS is nullptr");

    // Create a global var before the function
    ::std::string nabId = "nab_" + ::std::to_string(bopNum++);

    if(templDecl != NULL) 
      rw.InsertTextBefore(templDecl->getSourceRange().getBegin(), "int " + nabId + " = 0;\n");
    else                  
      rw.InsertTextBefore(funDecl->getSourceRange().getBegin(), "int " + nabId + " = 0;\n");

    // Retrieve the name of the operands
    ::std::string lhsString       = rw.getRewrittenText(lhs->getSourceRange());
    ::std::string rhsString       = rw.getRewrittenText(rhs->getSourceRange());
    ::std::string operationString = ""; 
    if( bop->getOpcodeStr() == "+" ){
      operationString = "false";
    } else if( bop->getOpcodeStr() == "-" ){
      operationString = "true";
    } else {
      operationString = "UNDEFINED";
    }

    // Start collecting information for the report (everything but the return variable):
    MutatorInAx1::MutationInfo mutationInfo;

    // * Operation Identifier
    mutationInfo.nabId = nabId;

    // * Line location
    FullSourceLoc loc(bop->getSourceRange().getBegin(), *(node.SourceManager));
    mutationInfo.line = loc.getSpellingLineNumber();

    // * Information about operands:
    // ** LHS
    mutationInfo.op1 = lhsString;
    mutationInfo.op1OpTy = NoOp;
    if (isLhsBinaryOp) {
      mutationInfo.op1OpTy = ((const BinaryOperator *)internalLhs)->getOpcode();
    }

    // ** RHS
    mutationInfo.op2 = rhsString;
    mutationInfo.op2OpTy = NoOp;
    if (isRhsBinaryOp) {
      mutationInfo.op2OpTy = ((const BinaryOperator *)internalRhs)->getOpcode();
    }

    // ** Return variable (placeholder)
    mutationInfo.retOp = "NULL";

    // Form the replacing string
    ::std::string bopReplacement = "inexactAdder(" + nabId + ", " + lhsString + ", " + rhsString + ", " + operationString + ", " + cellId + ")";
      
    ////////////////////////////////////////////////////////////////////////////////////////////
    /// Debug
    ChimeraLogger::verbose("********************************************************\nDump binary operation:");

    sprintf(log_info, "Operation: %s  ==> [%s]",rw.getRewrittenText(bop->getSourceRange()).c_str(),bop->getOpcodeStr().str().c_str());
    ChimeraLogger::verbose(log_info);

    sprintf(log_info, "LHS: %s", lhsString.c_str());
    ChimeraLogger::verbose(log_info);

    sprintf(log_info, "RHS: %s", rhsString.c_str());
    ChimeraLogger::verbose(log_info);

    sprintf(log_info, "Mutation in: %s\n", bopReplacement.c_str());
    ChimeraLogger::verbose(log_info);

    //////////////////////////////////////////////////////////////////////////////////////////// 

    // Replace all the text of the binary operator with a function call
    rw.ReplaceText(bop->getSourceRange(), bopReplacement); 

    // Stop if the current node (bop) has no parents
    if( node.Context->getParents(*bop).empty() ) { 
      ChimeraLogger::verbose("No more parents. Exiting\n");
      break; 
    }

    // Retrieve parent type
    std::string parentType = PARENT_NODE_TYPE(node, bop);

    if(parentType == "BinaryOperator"){
      // If the parent is a BinaryOperator then assign to bop its parent
      ChimeraLogger::verbose("Parent is a BOP\n");
      bop = (BinaryOperator*)(GET_PARENT_NODE(node, bop, BinaryOperator));

    } else if((parentType == "ParenExpr")){
      // If the parent is a parenthesis node then retrieve it and traverse up the ast
      // till the next non-parenthesis node. 
      ParenExpr* parens = (ParenExpr*)(GET_PARENT_NODE(node, bop, ParenExpr));
      while( ( PARENT_NODE_TYPE(node, parens) == "ParenExpr") ){
          parens = (ParenExpr*)(GET_PARENT_NODE(node, parens, ParenExpr));
      }
      ChimeraLogger::verbose("Parens skipped successfully.\n");

      // If the content of parenthesis is not a BinaryOperator then exit else assign
      // parenthesis content to bop
      if( (PARENT_NODE_TYPE(node, parens) != "BinaryOperator") ) {
        sprintf(log_info, "WARNING: Unexpected parens content of type [%s]. Exiting...\n", PARENT_NODE_TYPE(node, parens).str().c_str());
        ChimeraLogger::verbose(log_info);
        bop = NULL;
      } else bop = (BinaryOperator*)(GET_PARENT_NODE(node, parens, BinaryOperator));

    } else if((parentType == "FunDecl") || (parentType == "VarDecl") || (parentType == "ImplicitCastExpr")){
      // If the parent is a FunDecl or a VarDecl then exit
      ChimeraLogger::verbose("Function o Variable Declaration reached. Exiting...\n");
      bop = NULL;

    } else {
      // If the parent is not one of the previous IFs, then exit and print the unexpected type
      sprintf(log_info, "WARNING: Unexpected parent of type [%s]. Exiting...\n", parentType.c_str());
      ChimeraLogger::verbose(log_info);
      bop = NULL;
    }

    if( bop && (bop->getOpcodeStr()) == "=" ){
      // Get return variable name, if exists
      
      // Check if it is a DeclRef expression
      if (::llvm::isa<DeclRefExpr>(bop->getLHS())) {
        mutationInfo.retOp = ((const DeclRefExpr *)(bop->getLHS()))
                    ->getNameInfo()
                    .getName()
                    .getAsString();
      }
        
      // If a new BinaryOperator has been assigned to bop (indeed bop is not NULL) 
      // and it's a =, then exit 
      sprintf(log_info, "BOP opcod is [%s]. Exiting...\n", bop->getOpcodeStr().str().c_str());
      ChimeraLogger::verbose(log_info);
      bop = NULL;
    }

    // Save info into the report
    this->mutationsInfo.push_back(mutationInfo);
  } while(bop != NULL);

    this->nabCounter = bopNum;

    return rw;


}

void chimera::inax1::MutatorInAx1::onCreatedMutant(const ::std::string &mDir) {
  // Create a specific report inside the mutant directory

  ::std::error_code error;
  ::llvm::raw_fd_ostream report(mDir + this->reportName + ".csv", error, ::llvm::sys::fs::OpenFlags::F_Append);

  ChimeraLogger::verbose("****************************************************\nStart writing report");

  while( !(this->mutationsInfo.empty()) ){
    ChimeraLogger::verbose("Writing element...");

    MutatorInAx1::MutationInfo mutationInfo = this->mutationsInfo.back();
    report << mutationInfo.nabId << "," << mutationInfo.line << ","
           << "\"" << mutationInfo.op1 << "\","
           << "\"" << mutationInfo.op2 << "\","
           << "\"" << mutationInfo.retOp << "\"\n";

    this->mutationsInfo.pop_back();
  }
  report.close();
  ChimeraLogger::verbose("****************************************************\nReport written successfully");
}