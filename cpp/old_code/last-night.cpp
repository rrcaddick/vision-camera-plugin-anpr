#include <jsi/jsi.h>
#include "vision-camera-plugin-anpr.h"
#include "react-native-vision-camera/FrameHostObject.h"
#include <opencv2/core.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>
#include "alpr.h"
#include <android/log.h>
#include <memory>
#include <mutex>
#include <string>
#include <android/hardware_buffer.h>

#define LOG_TAG "ReactNative"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

namespace visioncamerapluginanpr {
  using namespace facebook;
  using namespace vision;
  
  static std::string g_configPath;
  static std::string g_runtimePath;
  static std::string g_platePath;
  static std::unique_ptr<alpr::Alpr> g_openalprInstance = nullptr;
  static std::mutex g_openalprMutex;

  void setAlprPaths(const char* configPath, const char* runtimePath, const char* platePath) {
    g_configPath = configPath;
    g_runtimePath = runtimePath;
    g_platePath = platePath;
  }

  void initializeOpenALPR() {
    std::lock_guard<std::mutex> lock(g_openalprMutex);
    if (!g_openalprInstance) {
      LOGI("Initializing OpenALPR...");
      g_openalprInstance = std::make_unique<alpr::Alpr>("eu", g_configPath, g_runtimePath);
      g_openalprInstance->setTopN(2);
      g_openalprInstance->setDefaultRegion("eu");

      if (!g_openalprInstance->isLoaded()) {
        LOGE("Error loading OpenALPR library");
      } else {
        LOGI("OpenALPR initialized successfully");
      }
    }
  }

  // void logHardwareBuffer(uint64_t pointer, int width, int height) {     
  //     uint8_t* rgbData = reinterpret_cast<uint8_t*>(pointer);
  //     unsigned char* rgbData = reinterpret_cast<unsigned char*>(pointer);

  //     // Log RGB data samples
  //     __android_log_print(ANDROID_LOG_INFO, "ReactNative", "RGB Data Samples:");

  //     // Log the RGB values of the first 4 pixels
  //     for (int i = 0; i < 4; i++) {
  //         uint8_t r = rgbData[i * 3];
  //         uint8_t g = rgbData[i * 3 + 1];
  //         uint8_t b = rgbData[i * 3 + 2];
  //         __android_log_print(ANDROID_LOG_INFO, "ReactNative", "Pixel %d: R=%d, G=%d, B=%d", i, r, g, b);
  //     }

  //     // Calculate and log average R, G, B values
  //     double avgR = 0, avgG = 0, avgB = 0;
  //     int sampleSize = 1000;  // Sample first 1000 pixels
  //     for (int i = 0; i < sampleSize; i++) {
  //         avgR += rgbData[i * 3];
  //         avgG += rgbData[i * 3 + 1];
  //         avgB += rgbData[i * 3 + 2];
  //     }

  //     avgR /= sampleSize;
  //     avgG /= sampleSize;
  //     avgB /= sampleSize;

  //     __android_log_print(ANDROID_LOG_INFO, "ReactNative", "Average RGB: R=%.2f, G=%.2f, B=%.2f", 
  //                         avgR, avgG, avgB);
  // }

  std::string processImage(const uint8_t* sourceData, int width, int height, int bytesPerRow, int planesCount) {
      if (planesCount != 3) {
          LOGE("Unexpected planes count: %d", planesCount);
          return "{}";
      }

      // Assuming you have access to the three planes (Y, U, V) in separate buffers
      // For simplicity, let's assume `sourceData` contains YUV_420_888 data

      // Create separate pointers to Y, U, and V planes
      const uint8_t* yPlane = sourceData;
      const uint8_t* uPlane = sourceData + (height * bytesPerRow); // Adjust based on actual stride
      const uint8_t* vPlane = uPlane + (height / 2 * bytesPerRow / 2); // Adjust based on actual stride

      // OpenCV expects interleaved data, so you need to combine the planes
      cv::Mat yMat(height, width, CV_8UC1, (void*)yPlane);
      cv::Mat uMat(height / 2, width / 2, CV_8UC1, (void*)uPlane);
      cv::Mat vMat(height / 2, width / 2, CV_8UC1, (void*)vPlane);

      // Resize U and V planes to match Y plane
      cv::Mat uMatResized, vMatResized;
      cv::resize(uMat, uMatResized, cv::Size(width, height));
      cv::resize(vMat, vMatResized, cv::Size(width, height));

      // Merge Y, U, V planes into a single YUV image
      std::vector<cv::Mat> yuvChannels = {yMat, uMatResized, vMatResized};
      cv::Mat yuvImage;
      cv::merge(yuvChannels, yuvImage);

      // Convert YUV to RGB
      cv::Mat rgbImage;
      cv::cvtColor(yuvImage, rgbImage, cv::COLOR_YUV2RGB);

      // TODO: Save images for debugging

      // Now rgbImage contains the RGB data
      unsigned char* pixelData = rgbImage.data;

      std::vector<alpr::AlprRegionOfInterest> regionsOfInterest;
      LOGI("Calling OpenALPR recognize function");

      // Pass the RGB data to OpenALPR
      alpr::AlprResults results = g_openalprInstance->recognize(pixelData, rgbImage.elemSize(), rgbImage.cols, rgbImage.rows, regionsOfInterest);

      LOGI("OpenALPR recognition completed, number of plate results: %zu", results.plates.size());
      for (const auto& plate : results.plates) {
          LOGI("Plate: %s, Confidence: %f", plate.bestPlate.characters.c_str(), plate.bestPlate.overall_confidence);
      }

      std::string jsonResult = alpr::Alpr::toJson(results);
      LOGI("JSON result length: %zu", jsonResult.length());

      return jsonResult;
  }

  void installPlugins(jsi::Runtime& runtime) {
    LOGI("Installing plugins...");
    initializeOpenALPR();

    auto recognisePlate = [](jsi::Runtime& runtime, const jsi::Value& thisArg, const jsi::Value* args, size_t count) -> jsi::Value {
      try {
        // LOGI("recognisePlate called");
        if (count < 1 || !args[0].isObject()) {
          LOGE("Invalid arguments");
          throw jsi::JSError(runtime, "Invalid arguments");
        }

        auto frame = args[0].asObject(runtime).asHostObject<FrameHostObject>(runtime);
        int width = frame->get(runtime, jsi::PropNameID::forUtf8(runtime, "width")).asNumber();
        int height = frame->get(runtime, jsi::PropNameID::forUtf8(runtime, "height")).asNumber();
        int bytesPerRow = frame->get(runtime, jsi::PropNameID::forUtf8(runtime, "bytesPerRow")).asNumber();
        // int planesCount = frame->get(runtime, jsi::PropNameID::forUtf8(runtime, "planesCount")).asNumber();
        // LOGI("Frame dimensions: %dx%d. Planes count is: %d", width, height,  planesCount);
  
        auto nativeBufferFunc = frame->get(runtime, jsi::PropNameID::forUtf8(runtime, "getNativeBuffer")).asObject(runtime).asFunction(runtime);
        auto nativeBufferObject = nativeBufferFunc.call(runtime).asObject(runtime);
        auto pointerValue  = nativeBufferObject.getProperty(runtime, "pointer");
        uint64_t pointer = pointerValue.asBigInt(runtime).asUint64(runtime);
        unsigned char* rgbData = reinterpret_cast<unsigned char*>(pointer);

        std::vector<alpr::AlprRegionOfInterest> regionsOfInterest;

        int bytesPerPixel = bytesPerRow / width;

        // Pass the RGB data to OpenALPR
        alpr::AlprResults results = g_openalprInstance->recognize(rgbData, bytesPerPixel, width, height, regionsOfInterest);

        std::string jsonResult = alpr::Alpr::toJson(results);
      
        // #if __ANDROID_API__ >= 26
          // // TODO: Check that the native buffer and sourceData is being create properly and correct to be converted
          // LOGI("Using NativeBuffer");
          // auto nativeBufferFunc = frame->get(runtime, jsi::PropNameID::forUtf8(runtime, "getNativeBuffer")).asObject(runtime).asFunction(runtime);
          // auto nativeBufferObject = nativeBufferFunc.call(runtime).asObject(runtime);
          
          // auto pointerProp = nativeBufferObject.getProperty(runtime, "pointer");
          // if (!pointerProp.isBigInt()) {
          //   LOGE("Pointer property is not a BigInt");
          //   throw jsi::JSError(runtime, "Pointer property is not a BigInt");
          // }
          
          // uint64_t pointerValue = pointerProp.asBigInt(runtime).asUint64(runtime);
          // const uint8_t* sourceData = reinterpret_cast<const uint8_t*>(static_cast<uintptr_t>(pointerValue));
          
          // if (sourceData == nullptr) {
          //   LOGE("NativeBuffer data is null");
          //   throw jsi::JSError(runtime, "NativeBuffer data is null");
          // }

          // LOGI("Processing image from NativeBuffer, pointer value: %llu", pointerValue);
          // jsonResult = processImage(sourceData, width, height, bytesPerRow, planesCount);

          // // Call the delete function to release the NativeBuffer
          // auto deleteFunc = nativeBufferObject.getProperty(runtime, "delete").asObject(runtime).asFunction(runtime);
          // deleteFunc.call(runtime);
          
          
        // #else
        //   LOGI("Using ArrayBuffer");
        //   auto arrayBufferFunc = frame->get(runtime, jsi::PropNameID::forUtf8(runtime, "toArrayBuffer")).asObject(runtime).asFunction(runtime);
        //   auto arrayBuffer = arrayBufferFunc.call(runtime).getObject(runtime).getArrayBuffer(runtime);
        //   const uint8_t* sourceData = arrayBuffer.data(runtime);
        //   jsonResult = processImage(sourceData, width, height, bytesPerRow, planesCount);
        // #endif

        // LOGI("Returning JSON result");
        return jsi::String::createFromUtf8(runtime, jsonResult);
      } catch (const std::exception& e) {
        LOGE("Error in recognisePlate: %s", e.what());
        throw jsi::JSError(runtime, std::string("Error in recognisePlate: ") + e.what());
      }
    };

    LOGI("Creating JSI function");
    auto jsiFunc = jsi::Function::createFromHostFunction(runtime, jsi::PropNameID::forUtf8(runtime, "frameProcessor"), 1, recognisePlate);
    LOGI("Setting global property");
    runtime.global().setProperty(runtime, "recognisePlateProcessor", jsiFunc);
    LOGI("Plugin installation complete");
  }
}