add_executable(notiman
    main.cpp
)

target_link_libraries(notiman PRIVATE notiman_shared third_party)

# System libraries
target_link_libraries(notiman PRIVATE ws2_32)
