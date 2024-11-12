# Build and statically link mpg123
set(MPG123_SOURCE_DIR "${SM_EXTERN_DIR}/mpg123")

include(ExternalProject)
ExternalProject_Add(
  mpg123
  SOURCE_DIR "${MPG123_SOURCE_DIR}"
  CONFIGURE_COMMAND ${MPG123_SOURCE_DIR}/configure --disable-shared --enable-static
  BUILD_COMMAND make
  INSTALL_COMMAND ""
)

ExternalProject_Get_Property(mpg123 SOURCE_DIR BINARY_DIR)
set(MPG123_INCLUDE_DIR "${SOURCE_DIR}/src/include" CACHE INTERNAL "mpg123 include")
set(MPG123_LIBRARY "${BINARY_DIR}/mpg123" CACHE INTERNAL "mpg123 library")

# Link the mpg123 library
set_target_properties(mpg123 PROPERTIES
  IMPORTED_LOCATION "${MPG123_LIBRARY}"
  INTERFACE_INCLUDE_DIRECTORIES "${MPG123_INCLUDE_DIR}"
)
