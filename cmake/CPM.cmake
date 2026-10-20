# Bootstraps a fixed CPM release with an independently recorded checksum.
# The downloaded tool belongs to the dependency cache, not the source tree.
if(NOT CPM_SOURCE_CACHE)
    if(DEFINED ENV{CPM_SOURCE_CACHE})
        set(CPM_SOURCE_CACHE "$ENV{CPM_SOURCE_CACHE}" CACHE PATH "CPM source cache")
    else()
        set(CPM_SOURCE_CACHE "${CMAKE_SOURCE_DIR}/.cache/cpm" CACHE PATH "CPM source cache")
    endif()
endif()
set(cpm_bootstrap "${CPM_SOURCE_CACHE}/bootstrap/CPM-0.43.2.cmake")
file(DOWNLOAD
    "https://github.com/cpm-cmake/CPM.cmake/releases/download/v0.43.2/CPM.cmake"
    "${cpm_bootstrap}"
    EXPECTED_HASH SHA256=49a3bef91ceb65bb66d57255e12d1ffd22abc2f6408fa9fe4534c544a2f232aa
    TLS_VERIFY ON)
include("${cpm_bootstrap}")
