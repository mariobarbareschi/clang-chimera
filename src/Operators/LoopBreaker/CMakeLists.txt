# CMakeLists for Example Operators
# Typically is necessary to change the target name (in this case 'example_operators') and
# add the relative path to the created source codes.
# Use this variable to change the target name
set (target_name "loopbreaker_operator")
set (target_lib_name ${target_name})

# Create a library target. 
# This command allows to list source codes. Those codes will be compiled.
# See the CMake add_library command for additional details.
add_library(${target_name}
            Mutators.cpp
            Operators.cpp
            ../InAx1/Mutators.cpp
            ../InAx1/Operators.cpp
            )
# Add the include directory
target_include_directories(${target_name}
                           PRIVATE ${CMAKE_SOURCE_DIR}/include
                           )
# Link to the core library, necessary to make visible the Core classes
target_link_libraries(${target_name}
		      core
		      )

# Modification of the target properties 
## Name: Change the output name of the library (the default is the same as the target name)
set_target_properties(${target_name} PROPERTIES OUTPUT_NAME ${target_lib_name})
## Output directory: Save the compiled static library in the proper folder
set_target_properties(${target_name} PROPERTIES ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/lib/operators)

# Add to the operator library: ${operators_lib}
target_link_libraries(${operators_lib} INTERFACE
                      ${target_name}
                     )

