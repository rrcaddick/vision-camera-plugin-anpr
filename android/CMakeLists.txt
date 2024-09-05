cmake_minimum_required(VERSION 3.4.1)
project(VisionCameraPluginAnpr)

set(CMAKE_VERBOSE_MAKEFILE ON)
set(CMAKE_CXX_STANDARD 14)

set(CMAKE_SYSTEM_VERSION 30) 

set(SRC_DIR ${CMAKE_SOURCE_DIR}/../cpp)
set(LIBS_DIR ${CMAKE_SOURCE_DIR}/../libs)
set(OPENCV_LIBS_DIR ${LIBS_DIR}/opencv)

find_package(ReactAndroid REQUIRED CONFIG)
find_package(fbjni REQUIRED CONFIG)
find_package(react-native-vision-camera REQUIRED CONFIG)

# Add shared libraries
add_library(${CMAKE_PROJECT_NAME} SHARED
  ${SRC_DIR}/vision-camera-plugin-anpr.cpp
  cpp-adapter.cpp
)

add_library(jpgt SHARED IMPORTED)
set_target_properties(jpgt PROPERTIES IMPORTED_LOCATION ${LIBS_DIR}/libjpgt.so)

add_library(pngt SHARED IMPORTED)
set_target_properties(pngt PROPERTIES IMPORTED_LOCATION ${LIBS_DIR}/libpngt.so)

add_library(leptonica SHARED IMPORTED)
set_target_properties(leptonica PROPERTIES IMPORTED_LOCATION ${LIBS_DIR}/libleptonica.so)

add_library(tesseract SHARED IMPORTED)
set_target_properties(tesseract PROPERTIES IMPORTED_LOCATION ${LIBS_DIR}/libtesseract.so)

# Define a function to add static library targets
function(add_static_library lib_name lib_path)
    add_library(${lib_name} STATIC IMPORTED)
    set_target_properties(${lib_name} PROPERTIES
            IMPORTED_LOCATION ${lib_path}/lib${lib_name}.a
    )
endfunction()

# Add OpenALPR libraries
add_static_library(openalpr ${LIBS_DIR})
add_static_library(support ${LIBS_DIR})

# Add 3rdparty OpenvCV static libraries
add_static_library(libjasper ${OPENCV_LIBS_DIR}/3rdparty)
add_static_library(tegra_hal ${OPENCV_LIBS_DIR}/3rdparty)
add_static_library(ittnotify ${OPENCV_LIBS_DIR}/3rdparty)
add_static_library(IlmImf ${OPENCV_LIBS_DIR}/3rdparty)
add_static_library(libjpeg-turbo ${OPENCV_LIBS_DIR}/3rdparty)
add_static_library(libpng ${OPENCV_LIBS_DIR}/3rdparty)
add_static_library(libtiff ${OPENCV_LIBS_DIR}/3rdparty)
add_static_library(libwebp ${OPENCV_LIBS_DIR}/3rdparty)
add_static_library(tbb ${OPENCV_LIBS_DIR}/3rdparty)

# Add OpenCV static libraries
add_static_library(opencv_core ${OPENCV_LIBS_DIR})
add_static_library(opencv_imgproc ${OPENCV_LIBS_DIR})
add_static_library(opencv_highgui ${OPENCV_LIBS_DIR})
add_static_library(opencv_calib3d ${OPENCV_LIBS_DIR})
add_static_library(opencv_objdetect ${OPENCV_LIBS_DIR})
add_static_library(opencv_ml ${OPENCV_LIBS_DIR})
add_static_library(opencv_features2d ${OPENCV_LIBS_DIR})
add_static_library(opencv_flann ${OPENCV_LIBS_DIR})
add_static_library(opencv_imgcodecs ${OPENCV_LIBS_DIR})

# Specifies a path to native header files.
target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE
  ${SRC_DIR}
  ${LIBS_DIR}/include/openalpr
  ${LIBS_DIR}/include/tesseract
  ${LIBS_DIR}/include/opencv
)

target_link_libraries(${CMAKE_PROJECT_NAME}
  android
  ReactAndroid::jsi
  fbjni::fbjni
  react-native-vision-camera::VisionCamera

  # OpenCV core libraries
  opencv_core
  opencv_imgproc
  opencv_highgui
  opencv_calib3d
  opencv_objdetect
  opencv_ml
  opencv_features2d
  opencv_flann
  opencv_imgcodecs

  # Third-party libraries
  libjasper
  tegra_hal
  ittnotify
  IlmImf
  libtiff
  libjpeg-turbo
  libpng
  libwebp
  tbb

  # System libraries
  log
  z

  # Application specific libraries
  jpgt
  pngt
  leptonica
  tesseract
  support
  openalpr
)