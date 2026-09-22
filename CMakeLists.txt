cmake_minimum_required(VERSION 3.22)
project(AutotelPresetsLibrarian VERSION 0.1.0 LANGUAGES CXX)

# Two targets, because a headless test binary (or a command-line tool) has no business linking a
# GUI module:
#
#   atl_presets_files  -- the file format and the folder scanner. juce_data_structures only.
#   atl_presets_ui     -- the librarian panel. Adds juce_gui_basics.
#
# JUCE itself is *not* added here: this project is consumed with add_subdirectory from a plugin
# that has already added JUCE, and two `add_subdirectory(JUCE)` calls in one build tree collide.
# See README.md.

if(NOT TARGET juce::juce_data_structures)
    message(FATAL_ERROR
        "AutotelPresetsLibrarian expects the consuming project to have added JUCE already "
        "(add_subdirectory(<path-to-JUCE>) before add_subdirectory(<this repo>)).")
endif()

add_library(atl_presets_files STATIC
    Source/PresetFile.cpp
    Source/PresetLibrary.cpp)
target_include_directories(atl_presets_files PUBLIC Source)
target_compile_features(atl_presets_files PUBLIC cxx_std_20)
target_link_libraries(atl_presets_files PUBLIC juce::juce_data_structures)
# Plugins are shared objects, so everything linked into one has to be position independent.
set_target_properties(atl_presets_files PROPERTIES POSITION_INDEPENDENT_CODE ON)

add_library(atl_presets_ui STATIC
    Source/PresetPanel.cpp
    Source/PresetPanelStyle.cpp
    Source/PresetPanelViews.cpp
    Source/PresetEditPopover.cpp)
target_include_directories(atl_presets_ui PUBLIC Source)
target_compile_features(atl_presets_ui PUBLIC cxx_std_20)
target_link_libraries(atl_presets_ui PUBLIC atl_presets_files juce::juce_gui_basics)
set_target_properties(atl_presets_ui PROPERTIES POSITION_INDEPENDENT_CODE ON)

# The unit tests are juce::UnitTest classes with no runner of their own: a consuming project adds
# these sources to whatever test binary it already has, so there is one runner per build tree
# rather than one per library. `ATL_PRESETS_TEST_SOURCES` is absolute, so it works from anywhere.
set(ATL_PRESETS_TEST_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/Tests/PresetFileTests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Tests/PresetLibraryTests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Tests/PresetPanelTests.cpp
    PARENT_SCOPE)
