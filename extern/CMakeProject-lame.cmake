# Specify the source files for the LAME library
set(LAME_SRC
  "${CMAKE_CURRENT_SOURCE_DIR}/libmp3lame/bitstream.c"
  "${CMAKE_CURRENT_SOURCE_DIR}/libmp3lame/encoder.c"
  "${CMAKE_CURRENT_SOURCE_DIR}/libmp3lame/fft.c"
  "${CMAKE_CURRENT_SOURCE_DIR}/libmp3lame/gain_analysis.c"
  "${CMAKE_CURRENT_SOURCE_DIR}/libmp3lame/id3tag.c"
  "${CMAKE_CURRENT_SOURCE_DIR}/libmp3lame/lame.c"
  "${CMAKE_CURRENT_SOURCE_DIR}/libmp3lame/mpglib_interface.c"
  "${CMAKE_CURRENT_SOURCE_DIR}/libmp3lame/newmdct.c"
  "${CMAKE_CURRENT_SOURCE_DIR}/libmp3lame/presets.c"
  "${CMAKE_CURRENT_SOURCE_DIR}/libmp3lame/psymodel.c"
  "${CMAKE_CURRENT_SOURCE_DIR}/libmp3lame/quantize.c"
  "${CMAKE_CURRENT_SOURCE_DIR}/libmp3lame/quantize_pvt.c"
  "${CMAKE_CURRENT_SOURCE_DIR}/libmp3lame/reservoir.c"
  "${CMAKE_CURRENT_SOURCE_DIR}/libmp3lame/set_get.c"
  "${CMAKE_CURRENT_SOURCE_DIR}/libmp3lame/tables.c"
  "${CMAKE_CURRENT_SOURCE_DIR}/libmp3lame/takehiro.c"
  "${CMAKE_CURRENT_SOURCE_DIR}/libmp3lame/util.c"
  "${CMAKE_CURRENT_SOURCE_DIR}/libmp3lame/vbrquantize.c"
  "${CMAKE_CURRENT_SOURCE_DIR}/libmp3lame/version.c"
)

# Specify the include files for the LAME library
set(LAME_INCLUDE
  "${CMAKE_CURRENT_SOURCE_DIR}/include/lame.h"
  "${CMAKE_CURRENT_SOURCE_DIR}/libmp3lame/bitstream.h"
  "${CMAKE_CURRENT_SOURCE_DIR}/libmp3lame/encoder.h"
  "${CMAKE_CURRENT_SOURCE_DIR}/libmp3lame/fft.h"
  "${CMAKE_CURRENT_SOURCE_DIR}/libmp3lame/gain_analysis.h"
  "${CMAKE_CURRENT_SOURCE_DIR}/libmp3lame/id3tag.h"
  "${CMAKE_CURRENT_SOURCE_DIR}/libmp3lame/machine.h"
  "${CMAKE_CURRENT_SOURCE_DIR}/libmp3lame/mpglib_interface.h"
  "${CMAKE_CURRENT_SOURCE_DIR}/libmp3lame/newmdct.h"
  "${CMAKE_CURRENT_SOURCE_DIR}/libmp3lame/psymodel.h"
  "${CMAKE_CURRENT_SOURCE_DIR}/libmp3lame/quantize.h"
  "${CMAKE_CURRENT_SOURCE_DIR}/libmp3lame/quantize_pvt.h"
  "${CMAKE_CURRENT_SOURCE_DIR}/libmp3lame/reservoir.h"
  "${CMAKE_CURRENT_SOURCE_DIR}/libmp3lame/set_get.h"
  "${CMAKE_CURRENT_SOURCE_DIR}/libmp3lame/tables.h"
  "${CMAKE_CURRENT_SOURCE_DIR}/libmp3lame/util.h"
  "${CMAKE_CURRENT_SOURCE_DIR}/libmp3lame/vbrquantize.h"
  "${CMAKE_CURRENT_SOURCE_DIR}/libmp3lame/version.h"
)

# Group source and header files
source_group("Source Files" FILES ${LAME_SRC})
source_group("Header Files" FILES ${LAME_INCLUDE})


# Set the language for the LAME source files to C
set_source_files_properties(${LAME_SRC} PROPERTIES LANGUAGE C)
