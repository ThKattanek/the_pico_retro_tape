add_library(pico_st7735 INTERFACE)
target_sources(pico_st7735 INTERFACE
  ${CMAKE_CURRENT_LIST_DIR}/src/st7735/ST7735_TFT.cpp
  ${CMAKE_CURRENT_LIST_DIR}/src/st7735/ST7735_TFT_graphics.cpp
  ${CMAKE_CURRENT_LIST_DIR}/src/st7735/ST7735_TFT_Print.cpp
)
target_include_directories(pico_st7735 INTERFACE ${CMAKE_CURRENT_LIST_DIR}/include)

# Pull in pico libraries that we need
target_link_libraries(pico_st7735 INTERFACE hardware_spi)
