cmake_minimum_required(VERSION 3.15)
project(ConsonantCutterStandalone VERSION 0.1.0)

set(JUCE_DIR "" CACHE PATH "Path to JUCE directory")
if(NOT EXISTS "${JUCE_DIR}/CMakeLists.txt")
  message(FATAL_ERROR "JUCE_DIR is not set correctly. Point it to your JUCE folder (contains CMakeLists.txt).")
endif()
add_subdirectory(${JUCE_DIR} JUCE)

juce_add_gui_app(ConsonantCutterApp
    PRODUCT_NAME "ConsonantCutter"
    COMPANY_NAME "Tidalwave Studio"
)

target_sources(ConsonantCutterApp PRIVATE
    Source/Main.cpp
    Source/Processor.h
    Source/Processor.cpp
)

target_compile_features(ConsonantCutterApp PRIVATE cxx_std_17)

target_link_libraries(ConsonantCutterApp PRIVATE
    juce::juce_gui_extra
    juce::juce_audio_formats
    juce::juce_dsp
)

juce_generate_juce_header(ConsonantCutterApp)
