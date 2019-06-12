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
/// \brief AxDCT mutator implementation
//===----------------------------------------------------------------------===//

#include "Operators/AxDCT/Mutators.h"
#include "llvm/Support/ErrorHandling.h"

#include "Log.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/ErrorHandling.h"
#include <iostream>

#define DEBUG_TYPE "mutator_axdct"

using namespace clang;
using namespace clang::ast_matchers;
using namespace chimera;
using namespace chimera::mutator;
using namespace chimera::log;
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
bool chimera::axdct::MutatorAxDCT::getMatchedNode(const NodeType &node, DynTypedNode &dynNode) {
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
StatementMatcher chimera::axdct::MutatorAxDCT::getStatementMatcher() {
  return stmt(
    forStmt(
      unless(hasAncestor(forStmt()))
    ).bind("outer_for_stmt")
  );
}

/// \brief  Coarse grain is enough to identify the inner for loop
bool chimera::axdct::MutatorAxDCT::match(const NodeType &node) {
    return true;
}

Rewriter &chimera::axdct::MutatorAxDCT::mutate(const NodeType &node, MutatorType type, Rewriter &rw) {

    // Retrieve a pointer to function declaration (or template function declaration) to insert global variables before it
    const FunctionDecl *funDecl = node.Nodes.getNodeAs<FunctionDecl>("functionDecl");
    const FunctionTemplateDecl *templDecl = (FunctionTemplateDecl*)(GET_PARENT_NODE(node, funDecl, FunctionTemplateDecl));

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

    // Form the replacing string
    ::std::string condReplacement = condVariableString + " < " + baseId; 

    //////////////////////////////////////////////////////////////////////////////////////////
    // Information for the report:
    MutatorAxDCT::MutationInfo mutationInfo;

    // * Operation Identifier
    mutationInfo.baseId = baseId;

    // * Line location
    FullSourceLoc loc(forStmt->getSourceRange().getBegin(), *(node.SourceManager));
    mutationInfo.line = loc.getSpellingLineNumber();

    // Save info into the report
    this->mutationsInfo.push_back(mutationInfo);
    //////////////////////////////////////////////////////////////////////////////////////////
      
    //////////////////////////////////////////////////////////////////////////////////////////
    // Debug
    char debug_info[500];

    ChimeraLogger::verbose("***************************************************\nDump for loop:");

    // sprintf(debug_info,"Statement: %s", rw.getRewrittenText(forStmt->getSourceRange()).c_str());
    // ChimeraLogger::verbose(debug_info);
    // 
    // sprintf(debug_info, "Condition: %s", condString.c_str());
    // ChimeraLogger::verbose(debug_info);
// 
    // sprintf(debug_info, "Condition Variable: %s", condVariableString.c_str());
    // ChimeraLogger::verbose(debug_info);
// 
    // sprintf(debug_info, "Condition Variable: %s", condVariableString.c_str());
    // ChimeraLogger::verbose(debug_info);
// 
    // sprintf(debug_info, "Mutate condition in: %s", condReplacement.c_str());
    // ChimeraLogger::verbose(debug_info);
// 
    // ChimeraLogger::verbose("****************************************************\n");

    ////////////////////////////////////////////////////////////////////////////////////////// 

    // Replace all the text of the binary operator with a function call
    rw.ReplaceText(cond->getSourceRange(), condReplacement); 

    // Stop if the current node (bop) has no parents
    assert( ((ForStmt*)forStmt->getBody()) && "Error: No more for statement children.");

    forStmt = (ForStmt*)forStmt->getBody()->IgnoreContainers();
    cond = (Expr*) forStmt->getCond();

    ::std::string innerCondString = rw.getRewrittenText(cond->getSourceRange());
    ::std::string innerCondVariableString = innerCondString.substr(0, innerCondString.find("<"));
  
    condReplacement = innerCondVariableString + " < " + baseId + "-" + condVariableString; 
    
    //////////////////////////////////////////////////////////////////////////////////////////

    //[FIXME: Nel csv dovrebbe andare una riga per ogni baseID]
    // Save info into the report
    /*FullSourceLoc innerLoc(forStmt->getSourceRange().getBegin(), *(node.SourceManager));
    mutationInfo.line = innerLoc.getSpellingLineNumber();
    this->mutationsInfo.push_back(mutationInfo);*/

    //////////////////////////////////////////////////////////////////////////////////////////
  
    //////////////////////////////////////////////////////////////////////////////////////////
    // Debug
    ChimeraLogger::verbose("***************************************************\nDump inner for loop:");

    sprintf(debug_info,"Statement: %s", rw.getRewrittenText(forStmt->getSourceRange()).c_str());
    ChimeraLogger::verbose(debug_info);
    
    sprintf(debug_info, "Condition: %s", innerCondString.c_str());
    ChimeraLogger::verbose(debug_info);

    sprintf(debug_info, "Condition Variable: %s", innerCondVariableString.c_str());
    ChimeraLogger::verbose(debug_info);

    sprintf(debug_info, "Condition Variable: %s", innerCondVariableString.c_str());
    ChimeraLogger::verbose(debug_info);

    sprintf(debug_info, "Mutate condition in: %s", condReplacement.c_str());
    ChimeraLogger::verbose(debug_info);

    ChimeraLogger::verbose("****************************************************\n");

    ////////////////////////////////////////////////////////////////////////////////////////// 

    // Replace all the text of the binary operator with a function call
    rw.ReplaceText(cond->getSourceRange(), condReplacement); 

    this->operationCounter = bopNum;    
    return rw;

}

void chimera::axdct::MutatorAxDCT::onCreatedMutant(const ::std::string &mDir) {
  // Create a specific report inside the mutant directory
  ::std::error_code error;
  ::llvm::raw_fd_ostream report(mDir + "axdct_report.csv", error, ::llvm::sys::fs::OpenFlags::F_Append);
  ::std::vector<MutationInfo> cMutationsInfo = this->mutationsInfo;

  ChimeraLogger::verbose("****************************************************\nStart writing report");

  while( !(this->mutationsInfo.empty()) ){
    ChimeraLogger::verbose("Writing element...");

    MutatorAxDCT::MutationInfo mutationInfo = this->mutationsInfo.back();
    report << mutationInfo.baseId << "," << mutationInfo.line 
    << ",\"NULL\"" << ",\"NULL\"" << ",\"NULL\"" 
    << "\n";
    this->mutationsInfo.pop_back();
  }
  report.close();
  ChimeraLogger::verbose("****************************************************\nReport written successfully\n");
}