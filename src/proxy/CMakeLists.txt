add_executable(notiman-proxy WIN32
    main.cpp
    proxy_config.cpp
)

target_link_libraries(notiman-proxy PRIVATE notiman_shared third_party)

# System libraries
target_link_libraries(notiman-proxy PRIVATE ws2_32 shell32)
