# Specify the source files for the LAME library
set(LAME_SRC
  "lame-3.100/libmp3lame/bitstream.c"
  "lame-3.100/libmp3lame/encoder.c"
  "lame-3.100/libmp3lame/fft.c"
  "lame-3.100/libmp3lame/gain_analysis.c"
  "lame-3.100/libmp3lame/id3tag.c"
  "lame-3.100/libmp3lame/lame.c"
  "lame-3.100/libmp3lame/mpglib_interface.c"
  "lame-3.100/libmp3lame/newmdct.c"
  "lame-3.100/libmp3lame/presets.c"
  "lame-3.100/libmp3lame/psymodel.c"
  "lame-3.100/libmp3lame/quantize.c"
  "lame-3.100/libmp3lame/quantize_pvt.c"
  "lame-3.100/libmp3lame/reservoir.c"
  "lame-3.100/libmp3lame/set_get.c"
  "lame-3.100/libmp3lame/tables.c"
  "lame-3.100/libmp3lame/takehiro.c"
  "lame-3.100/libmp3lame/util.c"
  "lame-3.100/libmp3lame/vbrquantize.c"
  "lame-3.100/libmp3lame/version.c"
)

# Specify the include files for the LAME library
set(LAME_INCLUDE
  "lame-3.100/include/lame.h"
  "lame-3.100/include/lame_export.h"
)

# Group source and header files
source_group("Source Files" FILES ${LAME_SRC})
source_group("Header Files" FILES ${LAME_INCLUDE})

# Add the LAME library as a static library
add_library("lame-3.100" STATIC ${LAME_SRC} ${LAME_INCLUDE})

# Set the folder property for the LAME library target
set_property(TARGET "lame-3.100" PROPERTY FOLDER "External Libraries")

# Disable project warnings for the LAME library target
disable_project_warnings("lame-3.100")

# Specify the include directories for the LAME library target
target_include_directories("lame-3.100" PUBLIC
  "lame-3.100/include"
  "${CMAKE_CURRENT_BINARY_DIR}/lame-3.100"
)
