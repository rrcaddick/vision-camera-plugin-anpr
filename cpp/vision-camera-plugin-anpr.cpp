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
#include <jni.h>
#include "VulkanProcessor.h"
#include <algorithm>
#include <numeric>
#include <array>

#define LOG_TAG "ReactNative"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

namespace visioncamerapluginanpr {
  using namespace facebook;
  using namespace vision;
  using namespace vulkanProcessor;
  
  static std::string g_configPath;
  static std::string g_runtimePath;
  static std::string g_platePath;
  static std::unique_ptr<alpr::Alpr> g_openalprInstance = nullptr;
  static std::mutex g_openalprMutex;
  static JavaVM* g_jvm = nullptr;
  static int imgCount = 0;


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

   JNIEnv* getJNIEnv() {
        JNIEnv* env;
        int status = g_jvm->GetEnv((void**)&env, JNI_VERSION_1_6);
        if (status < 0) {
            status = g_jvm->AttachCurrentThread(&env, nullptr);
            if (status < 0) {
                return nullptr;
            }
        }
        return env;
    }

    std::string getDownloadsPath() {
        JNIEnv* env = getJNIEnv();
        if (!env) {
            LOGE("Failed to get JNIEnv");
            return "";
        }

        jclass environmentClass = env->FindClass("android/os/Environment");
        jmethodID getExternalStoragePublicDirectoryMethod = env->GetStaticMethodID(environmentClass, "getExternalStoragePublicDirectory", "(Ljava/lang/String;)Ljava/io/File;");
        jfieldID dirDownloadsField = env->GetStaticFieldID(environmentClass, "DIRECTORY_DOWNLOADS", "Ljava/lang/String;");
        jobject dirDownloads = env->GetStaticObjectField(environmentClass, dirDownloadsField);
        jobject file = env->CallStaticObjectMethod(environmentClass, getExternalStoragePublicDirectoryMethod, dirDownloads);

        jclass fileClass = env->FindClass("java/io/File");
        jmethodID getAbsolutePathMethod = env->GetMethodID(fileClass, "getAbsolutePath", "()Ljava/lang/String;");
        jstring pathJString = (jstring)env->CallObjectMethod(file, getAbsolutePathMethod);

        const char* pathCStr = env->GetStringUTFChars(pathJString, nullptr);
        std::string path(pathCStr);
        env->ReleaseStringUTFChars(pathJString, pathCStr);

        env->DeleteLocalRef(environmentClass);
        env->DeleteLocalRef(dirDownloads);
        env->DeleteLocalRef(file);
        env->DeleteLocalRef(fileClass);
        env->DeleteLocalRef(pathJString);

        return path;
    }

  std::string processImage(AHardwareBuffer* hardwareBuffer) {
      AHardwareBuffer_Desc bufferDesc;
      AHardwareBuffer_describe(hardwareBuffer, &bufferDesc);
      uint32_t width = bufferDesc.width;
      uint32_t height = bufferDesc.height;
      uint32_t stride = bufferDesc.stride;

      LOGI("Buffer width: %d, height: %d, stride: %d", width, height, stride);

      static VulkanProcessor processor;

      std::vector<uint8_t> processedImage = processor.processImage(hardwareBuffer, width, height, stride);

      LOGI("Processed image size: %zu", processedImage.size());

      // Validate the first few bytes of the image
      for (size_t i = 0; i < std::min(processedImage.size(), size_t(20)); i++) {
          LOGI("processedImage[%zu] = %u", i, processedImage[i]);
      }

      // Log more details about the output
      int nonZeroCount = 0;
      uint64_t sum = 0;
      for (size_t i = 0; i < processedImage.size(); i++) {
          if (processedImage[i] != 0) {
              nonZeroCount++;
              sum += processedImage[i];
          }
          if (i < 20) {
              LOGI("Value at index %zu: %u", i, processedImage[i]);
          }
      }
      LOGI("Number of non-zero values: %d", nonZeroCount);
      LOGI("Average value: %.2f", nonZeroCount > 0 ? (float)sum / nonZeroCount : 0);

      // Ensure the size matches the expected size (width * height for grayscale)
      if (processedImage.size() != width * height) {
        LOGI("Processed image size does not match expected size! Expected: %d, Got: %zu", 
            width * height, processedImage.size());
        // return "{}";
      }

      // Create an OpenCV image from the processed data (grayscale)
      cv::Mat image(height, width, CV_8UC1, processedImage.data());
   
      // cv::Mat image(width, height, CV_8UC1, processedImage.data());

      std::string downloadsPath = getDownloadsPath();
      if(imgCount <= 20) {
        if (!downloadsPath.empty()) {
            std::string imgNumber;
            if(imgCount < 10) {
                imgNumber = "0" + std::to_string(imgCount);
            } else {
                imgNumber = std::to_string(imgCount);
            }
            cv::imwrite(downloadsPath + "/vulkan_image_" + imgNumber + ".png", image);
            LOGI("Debug images saved to %s", downloadsPath.c_str());
        } else {
            LOGE("Failed to get downloads path");
        }
        imgCount++;
      } else {
        throw std::runtime_error("Debug images limit reached");
      }

      std::string jsonData = "{}";

      LOGI("Converted to char properly");
      try {
        // Use the processed image with OpenALPR
        // LOGI("Attempting to recognize plate...");
        // std::vector<alpr::AlprRegionOfInterest> regionsOfInterest;
        // alpr::AlprResults results = g_openalprInstance->recognize(processedImage.data(), 1, width, height, regionsOfInterest);
        // jsonData = alpr::Alpr::toJson(results);
      } catch (const std::exception& e) {
        LOGE("Error in recognisePlate: %s", e.what());
      }

      return jsonData;
  }

  AHardwareBuffer* getHardwareBuffer(std::shared_ptr<FrameHostObject> frame, jsi::Runtime& runtime) {
    auto nativeBufferFunc = frame->get(runtime, jsi::PropNameID::forUtf8(runtime, "getNativeBuffer")).asObject(runtime).asFunction(runtime);
    auto nativeBufferObject = nativeBufferFunc.call(runtime).asObject(runtime);
    auto pointerValue  = nativeBufferObject.getProperty(runtime, "pointer");
    uint64_t pointer = pointerValue.asBigInt(runtime).asUint64(runtime);
    return reinterpret_cast<AHardwareBuffer*>(pointer);
  }

  void installPlugins(jsi::Runtime& runtime) {
    LOGI("Installing plugins...");

    auto recognisePlate = [](jsi::Runtime& runtime, const jsi::Value& thisArg, const jsi::Value* args, size_t count) -> jsi::Value {
      try {
        if (count < 1 || !args[0].isObject()) {
          LOGE("Invalid arguments");
          throw jsi::JSError(runtime, "Invalid arguments");
        }

        auto frame = args[0].asObject(runtime).asHostObject<FrameHostObject>(runtime);
        std::string orientation = frame->get(runtime, jsi::PropNameID::forUtf8(runtime, "orientation")).asString(runtime).utf8(runtime);
        int planesCount = frame->get(runtime, jsi::PropNameID::forUtf8(runtime, "planesCount")).asNumber();
        int width = frame->get(runtime, jsi::PropNameID::forUtf8(runtime, "width")).asNumber();
        int height = frame->get(runtime, jsi::PropNameID::forUtf8(runtime, "height")).asNumber();
        LOGI("Planes count is: %d, width is: %d, height is: %d", planesCount, width, height);

        // Get HardwareBuffer from FrameHostObject
        AHardwareBuffer* hardwareBuffer = getHardwareBuffer(frame, runtime);
        
        std::string jsonResult = processImage(hardwareBuffer);
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

extern "C" JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM* vm, void* reserved) {
    visioncamerapluginanpr::g_jvm = vm;
    return JNI_VERSION_1_6;
}