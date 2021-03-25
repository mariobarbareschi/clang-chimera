# Create an interface library
set(operators_lib "operators")
add_library(${operators_lib} INTERFACE)

# List all directories, which should contain Mutation Operators and Mutators
file(GLOB children ${CMAKE_CURRENT_SOURCE_DIR}/*)
foreach(child ${children})
  if(IS_DIRECTORY ${child})
    message(STATUS "Mutation Operator directory: ${child}")
    # Add the CMakeLists.txt inside the subdirectory
    add_subdirectory(${child})
  endif(IS_DIRECTORY ${child})
endforeach(child)

# By default, load all operators library in ${CMAKE_SOURCE_DIR}/lib/operators
file(GLOB libs ${CMAKE_SOURCE_DIR}/lib/operators/lib*.a)
foreach(lib ${libs})
  message(STATUS "Mutation Operator library: ${lib}")
  # Link the library found
  target_link_libraries(${operators_lib} INTERFACE ${lib})
endforeach(lib)
