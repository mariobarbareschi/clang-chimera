//===- Mutators.h -----------------------------------------------*- C++ -*-===//
//
//  Copyright (C) 2015, 2016 Antonio Tammaro  (ntonjeta@autistici.org)
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
/// \author Antonio Tammaro
///// \brief This file contains loop perforation mutators
//===----------------------------------------------------------------------===//

#ifndef INCLUDE_OPERATORS_PERFORATION_MUTATORS_H
#define INCLUDE_OPERATORS_PERFORATION_MUTATORS_H

#include "Core/Mutator.h"

using namespace clang;
using namespace clang::ast_matchers;

namespace chimera
{
namespace perforation
{

/// \addtogroup OPERATORS_SAMPLE_MUTATORS Sample Mutators
/// \{

/**
 * @brief This mutator try to apply loop perforation technique for approximate computing
 *        others (at the moment -).
 */
class MutatorLoopPerforation : public chimera::mutator::Mutator
{
public:
    /**
     * @brief Constuctor
     */
    MutatorLoopPerforation()
        : Mutator ( ::chimera::mutator::StatementMatcherType, // A binary operator is a statement
                    "mutator_loop_perforation_operator", // String identifier
                    "loop perforation", // Description
                    1,
                    true),inc(nullptr),binc(nullptr),cond(nullptr),bas(nullptr),opId(0) { }
    virtual clang::ast_matchers::StatementMatcher getStatementMatcher() override; // Need to override this method, first part of matching rules
    virtual bool match ( const ::chimera::mutator::NodeType &node ) override; // Also this one, second part of matching rules
    virtual bool getMatchedNode ( const chimera::mutator::NodeType &,
                                  clang::ast_type_traits::DynTypedNode & ) override; // This is pure virtual and must be implemented
    virtual clang::Rewriter &mutate ( const chimera::mutator::NodeType &node,
                                      mutator::MutatorType type,
                                      clang::Rewriter &rw ) override; // mutation rules
private: 
    // Retrive the condition of forstmt
    const ::clang::BinaryOperator *cond;
    // Retrive the unary inc/dec of forstmt  
    const ::clang::UnaryOperator *inc;
    // Retrive 
    const ::clang::BinaryOperator *binc;
    const ::clang::BinaryOperator *bas;

    unsigned int opId;
};

/// \}
} // end namespace chimera::perforation
} // end namespace chimera

#endif /* INCLUDE_OPERATORS_PERFORATION_MUTATORS_H */
