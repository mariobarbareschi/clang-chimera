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

using namespace clang;
using namespace clang::ast_matchers;

/// \brief It is used to retrieve the node, it hides the binding string.
///        In Mutator.h there is code template snippet that should be fine for
///        all the majority of the cases.
bool chimera::inax1::MutatorInAx1::getMatchedNode(
    const chimera::mutator::NodeType &node,
    clang::ast_type_traits::DynTypedNode &dynNode) {
  const BinaryOperator *Op = node.Nodes.getNodeAs<BinaryOperator>("greater_op");
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
  return stmt(binaryOperator(hasOperatorName(">")).bind("greater_op"),
              anyOf(hasAncestor(forStmt().bind("forStmt")),
                    anything() // It must be as last
                    ));
}

/// \brief This method implements the fine grained matching rules, indeed it is
///        not possible in an easy way to specify that the node matched it has
///        not to be part of the condition expression of a for statement
bool chimera::inax1::MutatorInAx1::match(
    const ::chimera::mutator::NodeType &node) {
  // First operation: Retrieve the node
  const BinaryOperator *bop =
      node.Nodes.getNodeAs<BinaryOperator>("greater_op");
  assert(bop && "BinaryOperator is nullptr");

  // In order to see if the operation is not part of the condition expression,
  // it is simply checked if the operation position is not in the range of such
  // expression
  SourceRange bopRange = bop->getSourceRange();
  // IF a construct has been matched
  // If stmt
  const ForStmt *forStmt = node.Nodes.getNodeAs<ForStmt>("forStmt");
  // Check if there is forStmt
  if (forStmt != nullptr) {
    // Check if it is inside the ExpressionCondition range
    if (bopRange.getBegin().getRawEncoding() >=
            forStmt->getCond()->getSourceRange().getBegin().getRawEncoding() &&
        bopRange.getEnd().getRawEncoding() >=
            forStmt->getCond()->getSourceRange().getEnd().getRawEncoding()) {
      // The match is invalid, return false
      return false;
    }
  }

  // At this point the match is still valid, return true
  return true;
}

::clang::Rewriter &chimera::inax1::MutatorInAx1::mutate(
    const ::chimera::mutator::NodeType &node,
    ::chimera::mutator::MutatorType type, ::clang::Rewriter &rw) {
  // As first operation always retrieve the node
  const BinaryOperator *op = node.Nodes.getNodeAs<BinaryOperator>("greater_op");
  // Assert a precondition
  assert(op != nullptr && "getNodeAs returned a nullptr");

  // The rewriter object passed can be used to "mutate" the source code
  // In this case there are two mutation types, so the type parameter will
  // assume values: 0 and 1.
  // Select the correct replacement using the mutation type
  std::string opReplacement = "";
  switch (type) {
  case 0: {
    opReplacement = "<"; // First replacement
  } break;
  case 1: {
    opReplacement = "<="; // Second replacement
  } break;
  default:
    llvm_unreachable("Mutation type NOT SUPPORTED!");
    break;
  }

  // Get the left and right hand side of the operation
  std::string lhs = rw.getRewrittenText(op->getLHS()->getSourceRange());
  std::string rhs = rw.getRewrittenText(op->getRHS()->getSourceRange());

  // Replace all the text of the binary operator, substituting the operation
  // (which bind lhs and rhs) with the replacement
  rw.ReplaceText(op->getSourceRange(), lhs + " " + opReplacement + " " + rhs);
  return rw;
}
