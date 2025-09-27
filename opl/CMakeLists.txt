add_library(opl STATIC
    ${CMAKE_CURRENT_LIST_DIR}/emu8950.c
    ${CMAKE_CURRENT_LIST_DIR}/slot_render.cpp
    ${CMAKE_CURRENT_LIST_DIR}/opl_pico.c
    ${CMAKE_CURRENT_LIST_DIR}/slot_render_pico.S
)
target_compile_options(opl PRIVATE -fms-extensions)
target_compile_definitions(opl PRIVATE
    USE_EMU8950_OPL
    EMU8950_ASM
    EMU8950_NO_RATECONV
    EMU8950_NO_TLL
    EMU8950_NO_FLOAT
    EMU8950_NO_TEST_FLAG
    EMU8950_SIMPLER_NOISE
    EMU8950_SHORT_NOISE_UPDATE_CHECK
)
target_include_directories(opl PUBLIC ${CMAKE_CURRENT_LIST_DIR})
target_link_libraries(opl PUBLIC pico_audio_i2s hardware_gpio hardware_interp)
