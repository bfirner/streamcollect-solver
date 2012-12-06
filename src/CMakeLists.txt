SET(SourceFiles
  streamcollect_solver.cpp
)

add_executable (streamcollect_solver ${SourceFiles})
target_link_libraries (streamcollect_solver owl-common owl-solver pthread)

INSTALL(TARGETS streamcollect_solver RUNTIME DESTINATION bin/owl)
