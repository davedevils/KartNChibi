add_executable(test_ring_buffer test_ring_buffer.cpp ../src/ring_buffer.cpp)
target_include_directories(test_ring_buffer PRIVATE ../src)
target_compile_options(test_ring_buffer PRIVATE /W4 /WX /UNDEBUG)
add_test(NAME ring_buffer COMMAND test_ring_buffer)

add_executable(test_config test_config.cpp ../src/config.cpp)
target_include_directories(test_config PRIVATE ../src)
target_compile_options(test_config PRIVATE /W4 /WX /UNDEBUG)
add_test(NAME config COMMAND test_config)

add_executable(test_safe_mem test_safe_mem.cpp ../src/safe_mem.cpp)
target_include_directories(test_safe_mem PRIVATE ../src)
target_compile_options(test_safe_mem PRIVATE /W4 /WX /UNDEBUG)
add_test(NAME safe_mem COMMAND test_safe_mem)

add_executable(test_capture test_capture.cpp ../src/capture.cpp ../src/safe_mem.cpp)
target_include_directories(test_capture PRIVATE ../src)
target_compile_options(test_capture PRIVATE /W4 /WX /UNDEBUG)
add_test(NAME capture COMMAND test_capture)

add_executable(test_jsonl test_jsonl.cpp ../src/jsonl.cpp ../src/config.cpp)
target_include_directories(test_jsonl PRIVATE ../src)
target_compile_options(test_jsonl PRIVATE /W4 /WX /UNDEBUG)
add_test(NAME jsonl COMMAND test_jsonl)

add_executable(test_sink test_sink.cpp ../src/sink.cpp ../src/poll.cpp ../src/jsonl.cpp ../src/config.cpp ../src/ring_buffer.cpp ../src/capture.cpp ../src/safe_mem.cpp ../src/bp_command.cpp ../src/bp_queue.cpp ../src/bp_serialize.cpp ../src/ghidra_client.cpp ../src/bp_engine.cpp)
target_include_directories(test_sink PRIVATE ../src)
target_link_libraries(test_sink PRIVATE ws2_32)
target_compile_options(test_sink PRIVATE /W4 /WX /UNDEBUG)
add_test(NAME sink COMMAND test_sink)

add_executable(test_hook_engine test_hook_engine.cpp
    ../src/hook_engine.cpp ../src/capture.cpp ../src/safe_mem.cpp
    ../src/config.cpp ../src/ring_buffer.cpp)
target_include_directories(test_hook_engine PRIVATE ../src)
target_link_libraries(test_hook_engine PRIVATE minhook)
target_compile_options(test_hook_engine PRIVATE /W4 /WX /UNDEBUG)
add_test(NAME hook_engine COMMAND test_hook_engine)

add_executable(test_poll test_poll.cpp
    ../src/poll.cpp ../src/capture.cpp ../src/safe_mem.cpp
    ../src/config.cpp ../src/ring_buffer.cpp)
target_include_directories(test_poll PRIVATE ../src)
target_compile_options(test_poll PRIVATE /W4 /WX /UNDEBUG)
add_test(NAME poll COMMAND test_poll)

add_executable(test_bp_command test_bp_command.cpp ../src/bp_command.cpp)
target_include_directories(test_bp_command PRIVATE ../src)
target_compile_options(test_bp_command PRIVATE /W4 /WX /UNDEBUG)
add_test(NAME bp_command COMMAND test_bp_command)

add_executable(test_bp_queue test_bp_queue.cpp ../src/bp_queue.cpp)
target_include_directories(test_bp_queue PRIVATE ../src)
target_compile_options(test_bp_queue PRIVATE /W4 /WX /UNDEBUG)
add_test(NAME bp_queue COMMAND test_bp_queue)

add_executable(test_bp_serialize test_bp_serialize.cpp ../src/bp_serialize.cpp)
target_include_directories(test_bp_serialize PRIVATE ../src)
target_compile_options(test_bp_serialize PRIVATE /W4 /WX /UNDEBUG)
add_test(NAME bp_serialize COMMAND test_bp_serialize)

add_executable(test_ghidra_client test_ghidra_client.cpp ../src/ghidra_client.cpp)
target_include_directories(test_ghidra_client PRIVATE ../src)
target_link_libraries(test_ghidra_client PRIVATE ws2_32)
target_compile_options(test_ghidra_client PRIVATE /W4 /WX /UNDEBUG)
add_test(NAME ghidra_client COMMAND test_ghidra_client)

add_executable(test_bp_engine test_bp_engine.cpp ../src/bp_engine.cpp ../src/bp_queue.cpp ../src/safe_mem.cpp)
target_include_directories(test_bp_engine PRIVATE ../src)
target_compile_options(test_bp_engine PRIVATE /W4 /WX /UNDEBUG)
add_test(NAME bp_engine COMMAND test_bp_engine)
