# interprocedural/link-time optimization
include(CheckIPOSupported)
check_ipo_supported(RESULT ipo_supported OUTPUT err_msg)
if (ipo_supported)
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION ON)
else ()
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION OFF)
    message(NOTICE
            "Interprocedural linker optimization is not supported: ${err_msg}\n"
            "Continuing without it.")
endif ()
