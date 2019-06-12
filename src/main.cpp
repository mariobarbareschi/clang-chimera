//===- main.cpp -------------------------------------------------*- C++ -*-===//
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
/// \file main.cpp
/// \author Federico Iannucci
/// \brief Application main function
//===----------------------------------------------------------------------===//

#include "Operators/Operators.h"
#include "Tooling/ChimeraTool.h"

// For testing purpose
#include "Testing/MutatorsTesting.h"

int main(int argc, const char **argv) {
  // Create a Chimera Tool
  ::chimera::ChimeraTool chimeraTool;
  
  chimeraTool.registerMutationOperator(::chimera::flapmutator::getFLAPOperator());
  chimeraTool.registerMutationOperator(::chimera::vpamutator::getVPAOperator());
  chimeraTool.registerMutationOperator(::chimera::vpa_nmutator::getVPANOperator());
  chimeraTool.registerMutationOperator(::chimera::perforation::getPerforationFirstOperator());
  chimeraTool.registerMutationOperator(::chimera::perforation::getPerforationSecondOperator());
  chimeraTool.registerMutationOperator(::chimera::adder::getAdderOperator());
  chimeraTool.registerMutationOperator(::chimera::axdct::getAxDCTOperator());


  return chimeraTool.run(argc, argv);
}
