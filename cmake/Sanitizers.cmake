# Instruments project code for lifetime and undefined-behavior checks.
# Windows uses the regular presets; the sanitizer preset targets Linux/macOS.
if(SOMNIUM_ENABLE_SANITIZERS AND (WIN32 OR MSVC))
    message(FATAL_ERROR "SOMNIUM_ENABLE_SANITIZERS requires Linux or macOS with Clang/GCC")
endif()
function(somnium_sanitizers target)
    if(SOMNIUM_ENABLE_SANITIZERS)
        target_compile_options(${target} PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
        target_link_options(${target} PRIVATE -fsanitize=address,undefined)
    endif()
endfunction()
