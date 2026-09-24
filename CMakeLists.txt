cmake_minimum_required(VERSION 3.25)
project(OrganVST VERSION 0.1.0 LANGUAGES C CXX)
if(APPLE)
  enable_language(OBJC OBJCXX)
endif()
set(CMAKE_OSX_DEPLOYMENT_TARGET "13.0" CACHE STRING "Minimum macOS version")
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(GO_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/.deps/grandorgue")
if(NOT EXISTS "${GO_SOURCE}/src/core/GOWave.cpp")
  message(FATAL_ERROR "Run python3 scripts/bootstrap.py first")
endif()
find_package(wxWidgets REQUIRED COMPONENTS base core adv html net)
include(${wxWidgets_USE_FILE})
find_package(PkgConfig REQUIRED)
pkg_check_modules(WAVPACK REQUIRED IMPORTED_TARGET wavpack)
pkg_check_modules(FFTW REQUIRED IMPORTED_TARGET fftw3f)
find_package(Threads REQUIRED)
find_package(ZLIB REQUIRED)
find_package(LibArchive REQUIRED)
find_package(yaml-cpp CONFIG REQUIRED)
set(GITHUB_PROJECT "GrandOrgue/grandorgue")
set(FULL_VERSION "${PROJECT_VERSION}-OrganVST")
set(NUM_WIN_VERSION "0,1,0")
set(HAVE_ATOMIC 1)
set(HAVE_MUTEX 1)
configure_file("${GO_SOURCE}/src/core/go_defs.h.in" generated/go_defs.h)
include(cmake/core-sources.cmake)
include(cmake/engine-sources.cmake)
add_library(organ_engine STATIC ${GO_CORE_SOURCES} ${GO_ENGINE_SOURCES}
  "${GO_SOURCE}/src/grandorgue/midi/dialog-creator/GOMidiConfigDispatcher.cpp"
  "${GO_SOURCE}/submodules/ZitaConvolver/source/zita-convolver.cc"
  src/engine/HostServices.cpp)
target_sources(organ_engine PRIVATE src/engine/OrganInstance.cpp src/assets/OrganAssets.cpp)
target_include_directories(organ_engine PUBLIC
  "${GO_SOURCE}/src/core" "${GO_SOURCE}/src/grandorgue"
  "${GO_SOURCE}/submodules/ZitaConvolver/source"
  "${CMAKE_CURRENT_BINARY_DIR}/generated")
target_compile_definitions(organ_engine PUBLIC ORGANVST_HOST=1)
target_link_libraries(organ_engine PUBLIC ${wxWidgets_LIBRARIES}
  PkgConfig::WAVPACK PkgConfig::FFTW ZLIB::ZLIB yaml-cpp::yaml-cpp LibArchive::LibArchive Threads::Threads)
add_executable(organ_probe src/tools/organ_probe.cpp)
target_link_libraries(organ_probe PRIVATE organ_engine)
target_compile_definitions(organ_probe PRIVATE GO_RESOURCE_DIR="${GO_SOURCE}")
enable_testing()
find_package(Python3 REQUIRED COMPONENTS Interpreter)
add_test(NAME engine_render COMMAND ${Python3_EXECUTABLE}
  ${CMAKE_CURRENT_SOURCE_DIR}/tests/render_test.py $<TARGET_FILE:organ_probe>
  ${CMAKE_CURRENT_BINARY_DIR}/fixtures)

option(ORGANVST_BUILD_PLUGIN "Build VST3 and Steinberg validator" ON)
if(ORGANVST_BUILD_PLUGIN)
  set(SMTG_ENABLE_VST3_PLUGIN_EXAMPLES OFF CACHE BOOL "" FORCE)
  set(SMTG_ENABLE_VST3_HOSTING_EXAMPLES OFF CACHE BOOL "" FORCE)
  set(SMTG_ENABLE_VSTGUI_SUPPORT ON CACHE BOOL "" FORCE)
  set(SMTG_CREATE_PLUGIN_LINK OFF CACHE BOOL "" FORCE)
  set(VSTGUI_UISCRIPTING OFF CACHE BOOL "Unused editor scripting" FORCE)
  add_subdirectory(.deps/vst3sdk vst3sdk)
  if(MINGW)
    target_link_options(validator PRIVATE -municode)
    target_compile_definitions(vstgui PRIVATE VSTGUI_WIN32_CUSTOMFONT_SUPPORT=0)
    target_compile_options(vstgui PRIVATE -include locale)
    get_target_property(vstgui_sources vstgui SOURCES)
    list(FILTER vstgui_sources EXCLUDE REGEX "win32(taskexecutor|directcomposition)\\.cpp$")
    set_property(TARGET vstgui PROPERTY SOURCES "${vstgui_sources}")
    target_sources(vstgui PRIVATE
      "${CMAKE_CURRENT_SOURCE_DIR}/src/platform/MinGWTaskExecutor.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/src/platform/MinGWDirectComposition.cpp")
    target_include_directories(vstgui PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/.deps/vst3sdk/vstgui4")
    # GCC does not honor the MSVC #pragma comment(lib, ...) directives.
    target_link_libraries(vstgui PUBLIC d3d11 d2d1 dwrite windowscodecs
      dwmapi shlwapi imm32 opengl32 glu32 ole32 oleaut32 uuid gdi32 comdlg32)
    add_executable(windows_ui_tasks tests/windows_ui_tasks.cpp src/platform/MinGWTaskExecutor.cpp)
    target_include_directories(windows_ui_tasks PRIVATE .deps/vst3sdk/vstgui4)
    target_compile_definitions(windows_ui_tasks PRIVATE UNICODE _UNICODE)
    target_link_libraries(windows_ui_tasks PRIVATE Threads::Threads)
    add_test(NAME windows_ui_tasks COMMAND windows_ui_tasks)
    set_tests_properties(windows_ui_tasks PROPERTIES TIMEOUT 20)
  endif()
  set(public_sdk_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/.deps/vst3sdk/public.sdk")
  set(organvst_plugin_sources src/plugin/Entry.cpp src/plugin/Processor.cpp src/plugin/Controller.cpp src/plugin/Services.cpp)
  if(MINGW)
    set(organvst_binary_dir "${CMAKE_BINARY_DIR}/VST3/$<CONFIG>/OrganVST.vst3/Contents/x86_64-win")
    add_library(OrganVSTEngine MODULE ${organvst_plugin_sources}
      "${public_sdk_SOURCE_DIR}/source/main/dllmain.cpp")
    target_include_directories(OrganVSTEngine PRIVATE src)
    target_link_libraries(OrganVSTEngine PRIVATE organ_engine sdk vstgui_support)
    set_target_properties(OrganVSTEngine PROPERTIES PREFIX "" OUTPUT_NAME "OrganVST-engine"
      LIBRARY_OUTPUT_DIRECTORY "${organvst_binary_dir}")
    get_filename_component(organvst_runtime_dir "${CMAKE_CXX_COMPILER}" DIRECTORY)
    add_custom_command(TARGET OrganVSTEngine POST_BUILD
      COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/scripts/stage_windows_runtime.py"
        "${organvst_runtime_dir}" "$<TARGET_FILE:OrganVSTEngine>"
        "$<TARGET_FILE:validator>" "$<TARGET_FILE:moduleinfotool>")
    add_dependencies(OrganVSTEngine validator moduleinfotool)
    enable_language(RC)
    smtg_add_vst3plugin(OrganVST src/platform/WindowsLoader.c src/platform/WindowsLoader.rc)
    get_target_property(loader_sources OrganVST SOURCES)
    list(FILTER loader_sources EXCLUDE REGEX "/dllmain\\.cpp$")
    set_property(TARGET OrganVST PROPERTY SOURCES "${loader_sources}")
    set_target_properties(OrganVST PROPERTIES LINKER_LANGUAGE C
      LIBRARY_OUTPUT_DIRECTORY "${organvst_binary_dir}")
    target_link_options(OrganVST PRIVATE -static-libgcc)
    add_dependencies(OrganVST OrganVSTEngine)
  else()
    smtg_add_vst3plugin(OrganVST ${organvst_plugin_sources})
    target_include_directories(OrganVST PRIVATE src)
    target_link_libraries(OrganVST PRIVATE organ_engine sdk vstgui_support)
  endif()
  if(APPLE)
    smtg_target_set_bundle(OrganVST BUNDLE_IDENTIFIER "org.organvst.instrument" COMPANY_NAME "OrganVST contributors")
  endif()
  add_custom_command(TARGET OrganVST POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_directory
    "${GO_SOURCE}/sounds" "$<TARGET_FILE_DIR:OrganVST>/../Resources/sounds")
endif()

add_executable(asset_probe src/tools/asset_probe.cpp src/assets/OrganAssets.cpp)
target_link_libraries(asset_probe PRIVATE ${wxWidgets_LIBRARIES} LibArchive::LibArchive)
add_test(NAME asset_packs COMMAND ${Python3_EXECUTABLE}
  ${CMAKE_CURRENT_SOURCE_DIR}/tests/assets_test.py $<TARGET_FILE:asset_probe>
  ${CMAKE_CURRENT_BINARY_DIR}/asset-fixtures)
add_executable(instance_probe src/tools/instance_probe.cpp)
target_link_libraries(instance_probe PRIVATE organ_engine)
target_compile_definitions(instance_probe PRIVATE GO_RESOURCE_DIR="${GO_SOURCE}")
add_test(NAME independent_instances COMMAND ${Python3_EXECUTABLE}
  ${CMAKE_CURRENT_SOURCE_DIR}/tests/instances_test.py $<TARGET_FILE:instance_probe>
  ${CMAKE_CURRENT_BINARY_DIR}/instance-fixtures)

if(MINGW)
  enable_language(RC)
  # Native test executables also load wxWidgets and need Common Controls v6.
  foreach(probe organ_probe asset_probe instance_probe)
    target_sources(${probe} PRIVATE src/platform/WindowsTestHost.rc)
  endforeach()
endif()
