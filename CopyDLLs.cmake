set(DIST_DIR "${CMAKE_BINARY_DIR}/dist")

file(MAKE_DIRECTORY ${DIST_DIR})

# Copy exe sau khi build
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:${PROJECT_NAME}>
        ${DIST_DIR})

# Sử dụng đường dẫn chính xác tới MSYS2 của bạn
set(MSYS2_MINGW64_BIN "D:/Program Files/MSYS2/mingw64/bin")

# Kiểm tra thư mục bin có tồn tại không
if(NOT EXISTS "${MSYS2_MINGW64_BIN}")
    message(FATAL_ERROR "MSYS2 bin directory not found at: ${MSYS2_MINGW64_BIN}")
endif()

# Danh sách DLL cần copy
set(DLLS
    libgcc_s_seh-1.dll
    libwinpthread-1.dll
    libleptonica-6.dll
    libgif-7.dll
    libjpeg-8.dll
    libopenjp2-7.dll
    libpng16-16.dll
    libtiff-6.dll
    libdeflate.dll
    libjbig-0.dll
    libLerc.dll
    liblzma-5.dll
    libzstd.dll
    libwebp-7.dll
    libsharpyuv-0.dll
    libwebpmux-3.dll
    zlib1.dll
    libstdc++-6.dll
    libtesseract-5.5.dll
    libarchive-13.dll
    libb2-1.dll
    libbz2-1.dll
    libcrypto-3-x64.dll
    libexpat-1.dll
    libiconv-2.dll
    liblz4.dll
    libcurl-4.dll
    libbrotlidec.dll
    libbrotlicommon.dll
    libidn2-0.dll
    libintl-8.dll
    libunistring-5.dll
    libnghttp2-14.dll
    libnghttp3-9.dll
    libpsl-5.dll
    libssh2-1.dll
    libssl-3-x64.dll
    libgomp-1.dll
)

# Copy DLLs
foreach(dll ${DLLS})
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${MSYS2_MINGW64_BIN}/${dll}"
            "${DIST_DIR}")
endforeach()

# Copy thư mục tessdata nếu tồn tại
if(EXISTS "${CMAKE_SOURCE_DIR}/tessdata")
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_SOURCE_DIR}/tessdata"
            "${DIST_DIR}/tessdata"
    )
endif()