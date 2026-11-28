add_test(NAME simtlab_smoke COMMAND simtlab)

add_executable(simtlab_opcode_test opcode_test.cpp)

target_compile_features(simtlab_opcode_test PRIVATE cxx_std_20)
target_include_directories(simtlab_opcode_test PRIVATE ${PROJECT_SOURCE_DIR}/include)
set_target_properties(simtlab_opcode_test PROPERTIES CXX_EXTENSIONS OFF)

add_test(NAME simtlab_opcode_test COMMAND simtlab_opcode_test)

add_executable(simtlab_register_test register_test.cpp)

target_compile_features(simtlab_register_test PRIVATE cxx_std_20)
target_include_directories(simtlab_register_test PRIVATE ${PROJECT_SOURCE_DIR}/include)
set_target_properties(simtlab_register_test PROPERTIES CXX_EXTENSIONS OFF)

add_test(NAME simtlab_register_test COMMAND simtlab_register_test)

add_executable(simtlab_operand_test operand_test.cpp)

target_compile_features(simtlab_operand_test PRIVATE cxx_std_20)
target_include_directories(simtlab_operand_test PRIVATE ${PROJECT_SOURCE_DIR}/include)
set_target_properties(simtlab_operand_test PROPERTIES CXX_EXTENSIONS OFF)

add_test(NAME simtlab_operand_test COMMAND simtlab_operand_test)

add_executable(simtlab_instruction_model_test instruction_model_test.cpp)

target_compile_features(simtlab_instruction_model_test PRIVATE cxx_std_20)
target_include_directories(simtlab_instruction_model_test PRIVATE ${PROJECT_SOURCE_DIR}/include)
set_target_properties(simtlab_instruction_model_test PROPERTIES CXX_EXTENSIONS OFF)

add_test(NAME simtlab_instruction_model_test COMMAND simtlab_instruction_model_test)

add_executable(simtlab_token_test token_test.cpp)

target_compile_features(simtlab_token_test PRIVATE cxx_std_20)
target_include_directories(simtlab_token_test PRIVATE ${PROJECT_SOURCE_DIR}/include)
set_target_properties(simtlab_token_test PROPERTIES CXX_EXTENSIONS OFF)

add_test(NAME simtlab_token_test COMMAND simtlab_token_test)

add_executable(simtlab_lexer_test lexer_test.cpp ${PROJECT_SOURCE_DIR}/src/lexer.cpp)

target_compile_features(simtlab_lexer_test PRIVATE cxx_std_20)
target_include_directories(simtlab_lexer_test PRIVATE ${PROJECT_SOURCE_DIR}/include)
set_target_properties(simtlab_lexer_test PROPERTIES CXX_EXTENSIONS OFF)

add_test(NAME simtlab_lexer_test COMMAND simtlab_lexer_test)

add_executable(simtlab_parser_test parser_test.cpp ${PROJECT_SOURCE_DIR}/src/parser.cpp
    ${PROJECT_SOURCE_DIR}/src/lexer.cpp)
target_compile_features(simtlab_parser_test PRIVATE cxx_std_20)
target_include_directories(simtlab_parser_test PRIVATE ${PROJECT_SOURCE_DIR}/include)
set_target_properties(simtlab_parser_test PROPERTIES CXX_EXTENSIONS OFF)
add_test(NAME simtlab_parser_test COMMAND simtlab_parser_test)

add_executable(simtlab_execution_state_test execution_state_test.cpp ${PROJECT_SOURCE_DIR}/src/parser.cpp
    ${PROJECT_SOURCE_DIR}/src/lexer.cpp)
target_compile_features(simtlab_execution_state_test PRIVATE cxx_std_20)
target_include_directories(simtlab_execution_state_test PRIVATE ${PROJECT_SOURCE_DIR}/include)
set_target_properties(simtlab_execution_state_test PROPERTIES CXX_EXTENSIONS OFF)
add_test(NAME simtlab_execution_state_test COMMAND simtlab_execution_state_test)
