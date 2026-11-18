add_test(NAME simtlab_smoke COMMAND simtlab)

add_executable(simtlab_opcode_test opcode_test.cpp)

target_compile_features(simtlab_opcode_test PRIVATE cxx_std_20)
target_include_directories(simtlab_opcode_test PRIVATE ${PROJECT_SOURCE_DIR}/include)
set_target_properties(simtlab_opcode_test PROPERTIES CXX_EXTENSIONS OFF)

add_test(NAME simtlab_opcode_test COMMAND simtlab_opcode_test)
