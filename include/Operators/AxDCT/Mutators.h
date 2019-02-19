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
/// \brief Mutator declaration for AxDCT
//===----------------------------------------------------------------------===//

#ifndef INCLUDE_OPERATORS_AXDCT_MUTATORS_H
#define INCLUDE_OPERATORS_AXDCT_MUTATORS_H

#include "Core/Mutator.h"

namespace chimera
{
namespace axdct
{

/// \addtogroup OPERATORS_SAMPLE_MUTATORS Sample Mutators
/// \{

/**
 * @brief This mutator matches the binary relational operator >, and replaces it with
 *        others (at the moment < and <=).
 */
class MutatorAxDCT : public chimera::mutator::Mutator
{
      struct MutationInfo {
        ::std::string baseId;  ///< Operation Identifier
        unsigned line;  ///< Occurrence line
      };
public:
    /**
     * @brief Constuctor
     */
    MutatorAxDCT()
        : Mutator ( ::chimera::mutator::StatementMatcherType, // A binary operator is a statement
                    "mutator_axdct", // String identifier
                    "Breakes a nested for loop by a global parameter", // Description
                    1, // One mutation type
                    true
                  ) {}
    virtual clang::ast_matchers::StatementMatcher getStatementMatcher() override; // Need to override this method, first part of matching rules
    virtual bool match ( const ::chimera::mutator::NodeType &node ) override; // Also this one, second part of matching rules
    virtual bool getMatchedNode ( const chimera::mutator::NodeType &,
                                  clang::ast_type_traits::DynTypedNode & ) override; // This is pure virtual and must be implemented
    virtual clang::Rewriter &mutate ( const chimera::mutator::NodeType &node,
                                      mutator::MutatorType type,
                                      clang::Rewriter &rw ) override; // mutation rules
    virtual void onCreatedMutant(const ::std::string&) override;

private:
      unsigned int operationCounter = 0;  ///< Counter to keep tracks of done mutations
      ::std::vector<MutationInfo> mutationsInfo;  ///< It maintains info about mutations, in order to be saved
      bool hasReported = false;
};

/// \}
} // end namespace chimera::axdct
} // end namespace chimera

#endif /* INCLUDE_OPERATORS_AXDCT_MUTATORS_H */
