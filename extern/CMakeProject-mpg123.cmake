# Build and statically link mpg123
set(MPG123_SOURCE_DIR "${SM_EXTERN_DIR}/mpg123")

include(ExternalProject)

if (WIN32)
  ExternalProject_Add(
    mpg123
    SOURCE_DIR "${MPG123_SOURCE_DIR}"
    CONFIGURE_COMMAND ${MPG123_SOURCE_DIR}/windows-builds.sh x86_64
    BUILD_COMMAND ""
    INSTALL_COMMAND ""
  )
else()
  ExternalProject_Add(
    mpg123
    SOURCE_DIR "${MPG123_SOURCE_DIR}"
    CONFIGURE_COMMAND ${MPG123_SOURCE_DIR}/configure --disable-shared --enable-static
    BUILD_COMMAND make
    INSTALL_COMMAND ""
  )
endif()

ExternalProject_Get_Property(mpg123 SOURCE_DIR BINARY_DIR)
set(MPG123_INCLUDE_DIR "${MPG123_SOURCE_DIR}/src/include" CACHE INTERNAL "mpg123 include")
set(MPG123_LIBRARY "${BINARY_DIR}/mpg123" CACHE INTERNAL "mpg123 library")

# Link the mpg123 library
set_target_properties(mpg123 PROPERTIES
  IMPORTED_LOCATION "${MPG123_LIBRARY}"
  INTERFACE_INCLUDE_DIRECTORIES "${MPG123_INCLUDE_DIR}"
)
# Add the mpg123 include directory to your target
include_directories(${MPG123_INCLUDE_DIR})
