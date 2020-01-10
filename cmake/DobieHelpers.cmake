function(dobie_cxx_compile_options TARGET)

    set(DOBIE_GNU_FLAGS
        -Wall -Wundef -Wsign-compare -Wconversion -Wstrict-aliasing -Wtype-limits

        # These probably should be fixed instead of disabled,
        # but doing so to keep the warning count more managable for now.
        -Wno-reorder -Wno-unused-variable -Wno-unused-value

        # Required on Debug configuration and all configurations on OSX, Dobie WILL crash otherwise.
        $<$<OR:$<CXX_COMPILER_ID:AppleClang>,$<CONFIG:Debug>>:-fomit-frame-pointer>

        $<$<CXX_COMPILER_ID:GNU>:-Wno-unused-but-set-variable> # GNU only warning

        # Might be useful for debugging:
        #-fomit-frame-pointer -fwrapv -fno-delete-null-pointer-checks -fno-strict-aliasing -fvisibility=hidden
    )
    set(DOBIE_MSVC_FLAGS
        /W4 # Warning level 4
    )

    target_compile_options(${TARGET} PRIVATE
        $<$<CXX_COMPILER_ID:GNU,Clang,AppleClang>:${DOBIE_GNU_FLAGS}>
        $<$<CXX_COMPILER_ID:MSVC>:${DOBIE_MSVC_FLAGS}>)

    # Needed to avoid ruining global scope with Windows.h on win32
    target_compile_definitions(${TARGET} PRIVATE
        $<$<PLATFORM_ID:Windows>:WIN32_LEAN_AND_MEAN NOMINMAX>
        $<$<CXX_COMPILER_ID:MSVC>:VC_EXTRALEAN>)

endfunction()
