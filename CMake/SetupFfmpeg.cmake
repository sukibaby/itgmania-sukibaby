if(CMAKE_GENERATOR MATCHES "Ninja")
  message(
    FATAL_ERROR
      "You cannot use the Ninja generator when building the bundled ffmpeg library."
    )
endif()

set(SM_FFMPEG_SRC_DIR "${SM_EXTERN_DIR}/ffmpeg")
set(SM_FFMPEG_CONFIGURE_EXE "${SM_FFMPEG_SRC_DIR}/configure")

list(APPEND FFMPEG_CONFIGURE
            "${SM_FFMPEG_CONFIGURE_EXE}"
            "--disable-autodetect"
            "--disable-avdevice"
            "--disable-avfilter"
            "--disable-devices"
            "--disable-doc"
            "--disable-filters"
            "--disable-lzma"
            "--disable-network"
            "--disable-postproc"
            "--disable-programs"
            "--disable-swresample"
            "--disable-vaapi"
            "--disable-bzlib"
            "--enable-gpl"
            "--enable-pthreads"
            "--enable-static"
            "--enable-zlib"
            "--prefix=/")

if(CMAKE_POSITION_INDEPENDENT_CODE)
  list(APPEND FFMPEG_CONFIGURE "--enable-pic")
endif()

if(MACOSX)
  list(APPEND FFMPEG_CONFIGURE "--enable-cross-compile")
  list(APPEND FFMPEG_CONFIGURE "--enable-videotoolbox")
  list(APPEND FFMPEG_CONFIGURE "--extra-cflags=-mmacosx-version-min=11")
  list(APPEND FFMPEG_CONFIGURE "--extra-ldflags=-mmacosx-version-min=11")

  set(FFMPEG_CONFIGURE_ARM64 ${FFMPEG_CONFIGURE})
  list(APPEND FFMPEG_CONFIGURE_ARM64 "--arch=arm64" "--extra-cflags=-arch arm64" "--extra-ldflags=-arch arm64")

  set(FFMPEG_CONFIGURE_X86_64 ${FFMPEG_CONFIGURE})
  list(APPEND FFMPEG_CONFIGURE_X86_64 "--arch=x86_64" "--extra-cflags=-arch x86_64" "--extra-ldflags=-arch x86_64")
endif()

if(NOT WITH_EXTERNAL_WARNINGS)
  list(APPEND FFMPEG_CONFIGURE "--extra-cflags=-w")
endif()

list(APPEND SM_FFMPEG_MAKE $(MAKE))
if(WITH_FFMPEG_JOBS GREATER 0)
  list(APPEND SM_FFMPEG_MAKE "-j${WITH_FFMPEG_JOBS}")
endif()
list(APPEND SM_FFMPEG_MAKE "&&" "make" "DESTDIR=./dest" "install")

# Build for arm64
externalproject_add("ffmpeg_arm64"
                    SOURCE_DIR "${SM_FFMPEG_SRC_DIR}"
                    CONFIGURE_COMMAND ${FFMPEG_CONFIGURE_ARM64}
                    BUILD_COMMAND "${SM_FFMPEG_MAKE}"
                    UPDATE_COMMAND ""
                    INSTALL_COMMAND ""
                    TEST_COMMAND "")

# Build for x86_64
externalproject_add("ffmpeg_x86_64"
                    SOURCE_DIR "${SM_FFMPEG_SRC_DIR}"
                    CONFIGURE_COMMAND ${FFMPEG_CONFIGURE_X86_64}
                    BUILD_COMMAND "${SM_FFMPEG_MAKE}"
                    UPDATE_COMMAND ""
                    INSTALL_COMMAND ""
                    TEST_COMMAND "")

# Combine the binaries using lipo
add_custom_command(TARGET ffmpeg_arm64 POST_BUILD
                   COMMAND lipo -create
                   ${BINARY_DIR}/dest_arm64/lib/libffmpeg.a
                   ${BINARY_DIR}/dest_x86_64/lib/libffmpeg.a
                   -output ${BINARY_DIR}/dest/lib/libffmpeg.a)

externalproject_get_property("ffmpeg_arm64" BINARY_DIR)
set(SM_FFMPEG_LIB ${BINARY_DIR}/dest/lib)
set(SM_FFMPEG_INCLUDE ${BINARY_DIR}/dest/include)
