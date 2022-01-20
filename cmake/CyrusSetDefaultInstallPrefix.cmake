# default install prefix to Filesystem Hierarchy Standard's "add-on" path
if (CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT
        AND NOT CMAKE_SYSTEM_NAME STREQUAL "Windows"
        AND PROJECT_IS_TOP_LEVEL)
    # todo: follow opt/ with provider, once registered with LANANA
    set(CMAKE_INSTALL_PREFIX "/opt/${PROJECT_NAME}" CACHE PATH
            "Base installation location." FORCE)
endif ()
