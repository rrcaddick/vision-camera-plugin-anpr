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

  uint8_t* getPixelData(jsi::Runtime& runtime, const jsi::Value& arg) {
    if (!arg.isObject() || !arg.asObject(runtime).isArrayBuffer(runtime)) {
      throw jsi::JSError(runtime, "Argument is not an ArrayBuffer");
    }

    // Get the ArrayBuffer and retrieve the data
    auto arrayBuffer = arg.asObject(runtime).getArrayBuffer(runtime);
    return static_cast<uint8_t*>(arrayBuffer.data(runtime));
  }

  void setAlprPaths(const char* configPath, const char* runtimePath) {
    g_configPath = configPath;
    g_runtimePath = runtimePath;
  }

  void initializeOpenALPR(std::string country, int topN, std::string region) {
    std::lock_guard<std::mutex> lock(g_openalprMutex);
    if (!g_openalprInstance) {
      LOGI("Initializing OpenALPR...");
      g_openalprInstance = std::make_unique<alpr::Alpr>(country, g_configPath, g_runtimePath);

      if(topN > 0) {
        g_openalprInstance->setTopN(topN);
      }

      if(!region.empty()) {
        LOGI("Setting region to %s", region.c_str());
        g_openalprInstance->setDefaultRegion(region);
      }

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

  auto initializeANPR  = [](jsi::Runtime& runtime, const jsi::Value& thisArg, const jsi::Value* args, size_t count) -> jsi::Value {
    try {
      if (count < 1 || !args[0].isString()) {
        LOGE("Invalid arguments");
        throw jsi::JSError(runtime, "Invalid arguments");
      }

      std::string country = args[0].getString(runtime).utf8(runtime);
      
      int topN = 0; 
      std::string region = "";

      if (count > 1 && args[1].isNumber()) {
        topN = args[1].asNumber();
      }

      if (count > 2 && args[2].isString()) {
        region = args[2].getString(runtime).utf8(runtime);
      }

      initializeOpenALPR(country, topN, region);

      // JSI requires a return value - undefined to specify void
      return jsi::Value::undefined();
    } catch (const std::exception& e) {
      LOGE("Error in initializeANPR: %s", e.what());
      throw jsi::JSError(runtime, std::string("Error in initializeANPR: ") + e.what());
    }
  };

  auto setTopN  = [](jsi::Runtime& runtime, const jsi::Value& thisArg, const jsi::Value* args, size_t count) -> jsi::Value {
    try {
      if (count < 1 || !args[0].isNumber()) {
        LOGE("TopN value must be a number");
        throw jsi::JSError(runtime, "TopN value must be a number");
      }

      int topN = args[0].asNumber();

      if (g_openalprInstance) {
        g_openalprInstance->setTopN(topN);
      } else {
        LOGE("OpenALPR not initialized");
        throw jsi::JSError(runtime, "OpenALPR not initialized");
      }

      // JSI requires a return value - undefined to specify void
      return jsi::Value::undefined();
    } catch (const std::exception& e) {
      LOGE("Error in setTopN: %s", e.what());
      throw jsi::JSError(runtime, std::string("Error in setTopN: ") + e.what());
    }
  };

  auto setCountry = [](jsi::Runtime& runtime, const jsi::Value& thisArg, const jsi::Value* args, size_t count) -> jsi::Value {
    try {
      if (count < 1 || !args[0].isString()) {
        LOGE("Country value must be a string");
        throw jsi::JSError(runtime, "Country value must be a string");
      }

      std::string country = args[0].getString(runtime).utf8(runtime);

      if (g_openalprInstance) {
        g_openalprInstance->setCountry(country);
      } else {
        LOGE("OpenALPR not initialized");
        throw jsi::JSError(runtime, "OpenALPR not initialized");
      }

      return jsi::Value::undefined();
    } catch (const std::exception& e) {
      LOGE("Error in setCountry: %s", e.what());
      throw jsi::JSError(runtime, std::string("Error in setCountry: ") + e.what());
    }
  };

  auto setPrewarp = [](jsi::Runtime& runtime, const jsi::Value& thisArg, const jsi::Value* args, size_t count) -> jsi::Value {
    try {
      if (count < 1 || !args[0].isString()) {
        LOGE("Prewarp value must be a string");
        throw jsi::JSError(runtime, "Prewarp value must be a string");
      }

      std::string prewarpConfig = args[0].getString(runtime).utf8(runtime);

      if (g_openalprInstance) {
        g_openalprInstance->setPrewarp(prewarpConfig);
      } else {
        LOGE("OpenALPR not initialized");
        throw jsi::JSError(runtime, "OpenALPR not initialized");
      }

      return jsi::Value::undefined();
    } catch (const std::exception& e) {
      LOGE("Error in setPrewarp: %s", e.what());
      throw jsi::JSError(runtime, std::string("Error in setPrewarp: ") + e.what());
    }
  };

  auto setMask = [](jsi::Runtime& runtime, const jsi::Value& thisArg, const jsi::Value* args, size_t count) -> jsi::Value {
    try {
      if (count < 4 || !args[0].isObject() || !args[1].isNumber() || !args[2].isNumber() || !args[3].isNumber()) {
        LOGE("Invalid arguments for setMask");
        throw jsi::JSError(runtime, "Invalid arguments for setMask");
      }

      uint8_t* pixelData = getPixelData(runtime, args[0]);
      int bytesPerPixel = args[1].asNumber();
      int imgWidth = args[2].asNumber();
      int imgHeight = args[3].asNumber();

      if (g_openalprInstance) {
        g_openalprInstance->setMask(pixelData, bytesPerPixel, imgWidth, imgHeight);
      } else {
        LOGE("OpenALPR not initialized");
        throw jsi::JSError(runtime, "OpenALPR not initialized");
      }

      return jsi::Value::undefined();
    } catch (const std::exception& e) {
      LOGE("Error in setMask: %s", e.what());
      throw jsi::JSError(runtime, std::string("Error in setMask: ") + e.what());
    }
  };

  auto setDetectRegion = [](jsi::Runtime& runtime, const jsi::Value& thisArg, const jsi::Value* args, size_t count) -> jsi::Value {
    try {
      if (count < 1 || !args[0].isBool()) {
        LOGE("DetectRegion value must be a boolean");
        throw jsi::JSError(runtime, "DetectRegion value must be a boolean");
      }

      bool detectRegion = args[0].getBool();

      if (g_openalprInstance) {
        g_openalprInstance->setDetectRegion(detectRegion);
      } else {
        LOGE("OpenALPR not initialized");
        throw jsi::JSError(runtime, "OpenALPR not initialized");
      }

      return jsi::Value::undefined();
    } catch (const std::exception& e) {
      LOGE("Error in setDetectRegion: %s", e.what());
      throw jsi::JSError(runtime, std::string("Error in setDetectRegion: ") + e.what());
    }
  };

  auto setDefaultRegion = [](jsi::Runtime& runtime, const jsi::Value& thisArg, const jsi::Value* args, size_t count) -> jsi::Value {
    try {
      if (count < 1 || !args[0].isString()) {
        LOGE("DefaultRegion value must be a string");
        throw jsi::JSError(runtime, "DefaultRegion value must be a string");
      }

      std::string region = args[0].getString(runtime).utf8(runtime);

      if (g_openalprInstance) {
        g_openalprInstance->setDefaultRegion(region);
      } else {
        LOGE("OpenALPR not initialized");
        throw jsi::JSError(runtime, "OpenALPR not initialized");
      }

      return jsi::Value::undefined();
    } catch (const std::exception& e) {
      LOGE("Error in setDefaultRegion: %s", e.what());
      throw jsi::JSError(runtime, std::string("Error in setDefaultRegion: ") + e.what());
    }
  };

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

  using JSIHostFunction = std::function<jsi::Value(jsi::Runtime&, const jsi::Value&, const jsi::Value*, size_t)>;

  void addPluginFunction(jsi::Runtime& runtime, const std::string& funcName, JSIHostFunction func) {
    auto pluginFunc = jsi::Function::createFromHostFunction(runtime, jsi::PropNameID::forUtf8(runtime, funcName), 1, func);
    runtime.global().setProperty(runtime, jsi::PropNameID::forUtf8(runtime, funcName), pluginFunc);
  }

  void installPlugin(jsi::Runtime& runtime) {
    bool isInstalled = runtime.global().hasProperty(runtime, "initializeANPR");

    if(isInstalled) {
      LOGI("Plugin already installed...");
      return;
    }
    
    LOGI("Installing plugin...");

    // Add initializeANPR
    addPluginFunction(runtime, "initializeANPR", initializeANPR);

    // Add setTopN
    addPluginFunction(runtime, "setTopN", setTopN);

    // Add setCountry
    addPluginFunction(runtime, "setCountry", setCountry);

    // Add setPrewarp
    addPluginFunction(runtime, "setPrewarp", setPrewarp);

    // Add setMask
    addPluginFunction(runtime, "setMask", setMask);

    // Add setDetectRegion
    addPluginFunction(runtime, "setDetectRegion", setDetectRegion);

    // Add setDefaultRegion
    addPluginFunction(runtime, "setDefaultRegion", setDefaultRegion);

    // Add recogniseFrame
    addPluginFunction(runtime, "recogniseFrame", recogniseFrame);

    LOGI("Plugin installation complete");
  }
}