#include <jsi/jsi.h>
#include "vision-camera-plugin-anpr.h"
#include "react-native-vision-camera/FrameHostObject.h"
#include "alpr.h"
#include <android/log.h>
#include <memory>
#include <mutex>
#include <string>
#include <android/hardware_buffer.h>
#include <jni.h>

#define LOG_TAG "ReactNative"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

namespace visioncamerapluginanpr {
  using namespace facebook;
  using namespace vision;
  
  static std::string g_configPath;
  static std::string g_runtimePath;
  static std::unique_ptr<alpr::Alpr> g_openalprInstance = nullptr;
  static std::mutex g_openalprMutex;

  void setAlprPaths(const char* configPath, const char* runtimePath) {
    g_configPath = configPath;
    g_runtimePath = runtimePath;
  }

  void initializeOpenALPR(std::string country) {
    std::lock_guard<std::mutex> lock(g_openalprMutex);
    if (!g_openalprInstance) {
      LOGI("Initializing OpenALPR...");
      g_openalprInstance = std::make_unique<alpr::Alpr>(country, g_configPath, g_runtimePath);
      g_openalprInstance->setTopN(20);
      g_openalprInstance->setDefaultRegion("za");

      if (!g_openalprInstance->isLoaded()) {
        LOGE("Error loading OpenALPR library");
      } else {
        LOGI("OpenALPR initialized successfully");
      }
    }
  }

  AHardwareBuffer* getHardwareBuffer(jsi::Runtime& runtime, std::shared_ptr<FrameHostObject> frame) {
    auto nativeBufferFunc = frame->get(runtime, jsi::PropNameID::forUtf8(runtime, "getNativeBuffer")).asObject(runtime).asFunction(runtime);
    auto nativeBufferObject = nativeBufferFunc.call(runtime).asObject(runtime);
    auto pointerValue  = nativeBufferObject.getProperty(runtime, "pointer");
    uint64_t pointer = pointerValue.asBigInt(runtime).asUint64(runtime);

    return reinterpret_cast<AHardwareBuffer*>(pointer);
  }

  std::string processImage(AHardwareBuffer* hardwareBuffer, int width, int height) {
      AHardwareBuffer_Planes planes;
      int result = AHardwareBuffer_lockPlanes(hardwareBuffer,AHARDWAREBUFFER_USAGE_CPU_READ_RARELY,-1, nullptr, &planes);

      if (result == 0) {
          uint8_t* yPlane = static_cast<uint8_t*>(planes.planes[0].data);
          int yStride = planes.planes[0].rowStride;

          unsigned char* pixelData = new unsigned char[width * height];

          // Rotate the Y-plane 90 degrees clockwise
          for (int y = 0; y < height; ++y) {
              for (int x = 0; x < width; ++x) {
                  pixelData[x * height + (height - y - 1)] = yPlane[y * yStride + x];
              }
          }

          std::vector<alpr::AlprRegionOfInterest> regionsOfInterest;
          alpr::AlprResults results = g_openalprInstance->recognize(pixelData, 1, height, width, regionsOfInterest);
          return alpr::Alpr::toJson(results);
          delete[] pixelData;
      } else {
          LOGE("Failed to lock HardwareBuffer planes for reading! Error code: %d", result);
          throw std::runtime_error("Failed to lock HardwareBuffer planes for reading!");
      }
  }

  auto recogniseFrame = [](jsi::Runtime& runtime, const jsi::Value& thisArg, const jsi::Value* args, size_t count) -> jsi::Value {
    try {
      if (count < 1 || !args[0].isObject()) {
        LOGE("Invalid arguments");
        throw jsi::JSError(runtime, "Invalid arguments");
      }

      auto frame = args[0].asObject(runtime).asHostObject<FrameHostObject>(runtime);

      int height = frame->get(runtime, jsi::PropNameID::forUtf8(runtime, "height")).asNumber();
      int width = frame->get(runtime, jsi::PropNameID::forUtf8(runtime, "width")).asNumber();
      
      
      std::string jsonResult = processImage(getHardwareBuffer(runtime, frame), width, height);
      return jsi::String::createFromUtf8(runtime, jsonResult);
    } catch (const std::exception& e) {
      LOGE("Error in recogniseFrame: %s", e.what());
      throw jsi::JSError(runtime, std::string("Error in recogniseFrame: ") + e.what());
    }
  };

  auto initializeANPR  = [](jsi::Runtime& runtime, const jsi::Value& thisArg, const jsi::Value* args, size_t count) -> jsi::Value {
    try {
      if (count < 1 || !args[0].isString()) {
        LOGE("Invalid arguments");
        throw jsi::JSError(runtime, "Invalid arguments");
      }
      std::string country = args[0].getString(runtime).utf8(runtime);
      initializeOpenALPR(country);

      // JSI requires a return value - undefined to specify void
      return jsi::Value::undefined();
    } catch (const std::exception& e) {
      LOGE("Error in initializeANPR: %s", e.what());
      throw jsi::JSError(runtime, std::string("Error in initializeANPR: ") + e.what());
    }
  };

  void installPlugin(jsi::Runtime& runtime) {
    bool isInstalled = runtime.global().hasProperty(runtime, "initializeANPR");

    if(isInstalled) {
      LOGI("Plugin already installed...");
      return;
    }
    
    LOGI("Installing plugin...");

    // Add initializeANPR
    auto initializeANPRFunc = jsi::Function::createFromHostFunction(runtime, jsi::PropNameID::forUtf8(runtime, "initializeANPR"), 1, initializeANPR);

    runtime.global().setProperty(runtime, "initializeANPR", initializeANPRFunc);

    // Add recogniseFrame
    auto recogniseFrameFunc = jsi::Function::createFromHostFunction(runtime, jsi::PropNameID::forUtf8(runtime, "recogniseFrame"), 1, recogniseFrame);

    runtime.global().setProperty(runtime, "recogniseFrame", recogniseFrameFunc);
    LOGI("Plugin installation complete");
  }
}