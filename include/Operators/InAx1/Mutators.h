//===- Mutators.h -----------------------------------------------*- C++ -*-===//
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
/// \file Mutators.h
/// \author Andrea Aletto
/// \brief This file contains sample mutators
//===----------------------------------------------------------------------===//

#ifndef INCLUDE_OPERATORS_INAX1_MUTATORS_H
#define INCLUDE_OPERATORS_INAX1_MUTATORS_H

#include "Core/Mutator.h"

namespace chimera
{
namespace inax1
{

/// \addtogroup OPERATORS_SAMPLE_MUTATORS Sample Mutators
/// \{

/**
 * @brief This mutator matches the binary relational operator >, and replaces it with
 *        others (at the moment < and <=).
 */
class MutatorInAx1 : public chimera::mutator::Mutator
{
    ::clang::BinaryOperatorKind NoOp = ::clang::BinaryOperatorKind::BO_Comma;
      struct MutationInfo {
        ::std::string nabId;  ///< Operation Identifier
        unsigned line;  ///< Occurrence line
        ::std::string op1;  ///< Operand 1
        ::clang::BinaryOperatorKind op1OpTy;  ///< It is != NoOp if operand 1 is a binary operation
        ::std::string op2;  ///< Operand 2
        ::clang::BinaryOperatorKind op2OpTy;  ///< It is != NoOp if operand 2 is a binary operation
        ::std::string retOp;  ///< Operand which eventually is returned
      };
public:
    /**
     * @brief Constuctor
     */
    MutatorInAx1()
        : Mutator ( ::chimera::mutator::StatementMatcherType, // A binary operator is a statement
                    "mutator_inax1", // String identifier
                    "Replaces exact sum with inexact sum based on InAx1 cell", // Description
                    1, // One mutation type
                    true
                  ) {}
    MutatorInAx1(::std::string reportName)
        : Mutator ( ::chimera::mutator::StatementMatcherType, // A binary operator is a statement
                    "mutator_inax1", // String identifier
                    "Replaces exact sum with inexact sum based on InAx1 cell", // Description
                    1, // One mutation type
                    true
                  ), reportName(reportName) {}
    virtual clang::ast_matchers::StatementMatcher getStatementMatcher() override; // Need to override this method, first part of matching rules
    virtual bool match ( const ::chimera::mutator::NodeType &node ) override; // Also this one, second part of matching rules
    virtual bool getMatchedNode ( const chimera::mutator::NodeType &,
                                  clang::ast_type_traits::DynTypedNode & ) override; // This is pure virtual and must be implemented
    virtual clang::Rewriter &mutate ( const chimera::mutator::NodeType &node,
                                      mutator::MutatorType type,
                                      clang::Rewriter &rw ) override; // mutation rules
    virtual void onCreatedMutant(const ::std::string&) override;

private:
      unsigned int operationCounter;  ///< Counter to keep tracks of done mutations
      ::std::vector<MutationInfo> mutationsInfo;  ///< It maintains info about mutations, in order to be saved
      ::std::string reportName = "inax1_report";
      bool hasReported = false;
};

/// \}
} // end namespace chimera::inax1
} // end namespace chimera

#endif /* INCLUDE_OPERATORS_INAX1_MUTATORS_H */
