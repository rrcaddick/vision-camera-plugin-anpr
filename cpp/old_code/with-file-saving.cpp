#include <jsi/jsi.h>
#include "vision-camera-plugin-anpr.h"
#include "react-native-vision-camera/FrameHostObject.h"
#include <opencv2/core.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>
#include "alpr.h"
#include <android/log.h>
#include <android/hardware_buffer.h>
#include <jni.h>
#include <memory>
#include <mutex>
#include <string>

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
    static JavaVM* g_jvm = nullptr;

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

    std::string processImage(const uint8_t* sourceData, int width, int height, int bytesPerRow, int planesCount) {
      if (planesCount != 3) {
          LOGE("Unexpected planes count: %d", planesCount);
          return "{}";
      }


      // YUV 420 format
      const uint8_t* yPlane = sourceData;
      const uint8_t* uPlane = yPlane + height * bytesPerRow;
      const uint8_t* vPlane = uPlane + (height / 2) * (bytesPerRow / 2);
      cv::Mat rgbImage;

      cv::Mat yMat(height, width, CV_8UC1, (void*)yPlane, bytesPerRow);
      cv::Mat uMat(height / 2, width / 2, CV_8UC1, (void*)uPlane, bytesPerRow / 2);
      cv::Mat vMat(height / 2, width / 2, CV_8UC1, (void*)vPlane, bytesPerRow / 2);

      // Convert YUV420 to RGB
      cv::Mat yuv420pImage;
      cv::resize(uMat, uMat, cv::Size(width, height), 0, 0, cv::INTER_NEAREST);
      cv::resize(vMat, vMat, cv::Size(width, height), 0, 0, cv::INTER_NEAREST);
      std::vector<cv::Mat> yuv_planes = {yMat, uMat, vMat};
      cv::merge(yuv_planes, yuv420pImage);
      cv::cvtColor(yuv420pImage, rgbImage, cv::COLOR_YUV2RGB_I420);

      // Save debug images
      std::string downloadsPath = getDownloadsPath();
      if (!downloadsPath.empty()) {
          cv::imwrite(downloadsPath + "/debug_y_plane.png", yMat);
          cv::imwrite(downloadsPath + "/debug_u_plane.png", uMat);
          cv::imwrite(downloadsPath + "/debug_v_plane.png", vMat);
          cv::imwrite(downloadsPath + "/debug_rgb_image.png", rgbImage);
          LOGI("Debug images saved to %s", downloadsPath.c_str());
      } else {
          LOGE("Failed to get downloads path");
      }

      std::vector<alpr::AlprRegionOfInterest> regionsOfInterest;
      LOGI("Calling OpenALPR recognize function");

      std::lock_guard<std::mutex> lock(g_openalprMutex);
      alpr::AlprResults results = g_openalprInstance->recognize(rgbImage.data, rgbImage.elemSize(), rgbImage.cols, rgbImage.rows, regionsOfInterest);

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
          LOGI("recognisePlate called");
          if (count < 1 || !args[0].isObject()) {
            LOGE("Invalid arguments");
            throw jsi::JSError(runtime, "Invalid arguments");
          }

          auto frame = std::static_pointer_cast<FrameHostObject>(args[0].getObject(runtime).getHostObject(runtime));
          int height = frame->get(runtime, jsi::PropNameID::forUtf8(runtime, "height")).asNumber();
          int width = frame->get(runtime, jsi::PropNameID::forUtf8(runtime, "width")).asNumber();
          int bytesPerRow = frame->get(runtime, jsi::PropNameID::forUtf8(runtime, "bytesPerRow")).asNumber();
          int planesCount = frame->get(runtime, jsi::PropNameID::forUtf8(runtime, "planesCount")).asNumber();
          LOGI("Frame dimensions: %dx%d. Planes count is: %d", width, height,  planesCount);

          std::string jsonResult;

          #if __ANDROID_API__ >= 26
            // TODO: Check that the native buffer and sourceData is being create properly and correct to be converted
            LOGI("Using NativeBuffer");
            auto nativeBufferFunc = frame->get(runtime, jsi::PropNameID::forUtf8(runtime, "getNativeBuffer")).asObject(runtime).asFunction(runtime);
            auto nativeBufferObject = nativeBufferFunc.call(runtime).asObject(runtime);
            
            auto pointerProp = nativeBufferObject.getProperty(runtime, "pointer");
            if (!pointerProp.isBigInt()) {
              LOGE("Pointer property is not a BigInt");
              throw jsi::JSError(runtime, "Pointer property is not a BigInt");
            }
            
            uint64_t pointerValue = pointerProp.asBigInt(runtime).asUint64(runtime);
            const uint8_t* sourceData = reinterpret_cast<const uint8_t*>(static_cast<uintptr_t>(pointerValue));
            
            if (sourceData == nullptr) {
              LOGE("NativeBuffer data is null");
              throw jsi::JSError(runtime, "NativeBuffer data is null");
            }

            LOGI("Processing image from NativeBuffer, pointer value: %llu", pointerValue);
            jsonResult = processImage(sourceData, width, height, bytesPerRow, planesCount);

            // Call the delete function to release the NativeBuffer
            auto deleteFunc = nativeBufferObject.getProperty(runtime, "delete").asObject(runtime).asFunction(runtime);
            deleteFunc.call(runtime);
          #else
            LOGI("Using ArrayBuffer");
            auto arrayBufferFunc = frame->get(runtime, jsi::PropNameID::forUtf8(runtime, "toArrayBuffer")).asObject(runtime).asFunction(runtime);
            auto arrayBuffer = arrayBufferFunc.call(runtime).getObject(runtime).getArrayBuffer(runtime);
            const uint8_t* sourceData = arrayBuffer.data(runtime);
            jsonResult = processImage(sourceData, width, height, bytesPerRow, planesCount);
          #endif

          LOGI("Returning JSON result");
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
} // namespace visioncamerapluginanpr

extern "C" JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM* vm, void* reserved) {
    visioncamerapluginanpr::g_jvm = vm;
    return JNI_VERSION_1_6;
}