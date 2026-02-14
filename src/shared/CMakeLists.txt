add_library(notiman_shared STATIC)

target_sources(notiman_shared PRIVATE
    config.h
    config.cpp
    corner.h
    corner.cpp
    hook_parser.h
    hook_parser.cpp
    icon.h
    icon.cpp
    payload.h
    payload.cpp
    positioning.h
    positioning.cpp
)

target_link_libraries(notiman_shared PUBLIC third_party)
