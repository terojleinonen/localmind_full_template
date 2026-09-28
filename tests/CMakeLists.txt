add_executable(localmind_tests
    main.cpp
    test_config.cpp
    test_embedder.cpp
    test_ollama.cpp
    test_query_engine.cpp
    test_server.cpp
    test_text_processor.cpp
    test_vector_store.cpp
)
target_link_libraries(localmind_tests PRIVATE localmind_core)
target_compile_options(localmind_tests PRIVATE -Wall -Wextra)

add_test(NAME localmind_tests COMMAND localmind_tests)
set_tests_properties(localmind_tests PROPERTIES TIMEOUT 120)
