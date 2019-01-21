//===- Operators.h ----------------------------------------------*- C++ -*-===//
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
/// \file Operators.h
/// \author Federico Iannucci
/// \brief This file contains sample operators
//===----------------------------------------------------------------------===//

#ifndef INCLUDE_OPERATORS_EXAMPLES_OPERATORS_H
#define INCLUDE_OPERATORS_EXAMPLES_OPERATORS_H

#include "Core/MutationOperator.h"

namespace chimera
{
namespace inax1
{

/// \addtogroup OPERATORS_SAMPLE_OPERATORS Sample Mutation Operators
/// \{
/// @brief Create and return the ROR Operator
::std::unique_ptr<::chimera::m_operator::MutationOperator> getInAx1Operator();
/// \}
} // end namespace chimera::inax1
} // end namespace chimera

#endif /* INCLUDE_OPERATORS_EXAMPLES_OPERATORS_H */

