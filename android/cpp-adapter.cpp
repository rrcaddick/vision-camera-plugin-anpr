#include <jni.h>
#include <jsi/jsi.h>
#include "vision-camera-plugin-anpr.h"
#include <string>

using namespace facebook;

extern "C"
JNIEXPORT void JNICALL
Java_com_visioncamerapluginanpr_VisionCameraPluginAnprModule_nativeInstallPlugins(JNIEnv *env, jclass type, jlong jsi) {
    auto runtime = reinterpret_cast<facebook::jsi::Runtime *>(jsi);

    if (runtime) {
        visioncamerapluginanpr::installPlugins(*runtime);
    }
}

extern "C"
JNIEXPORT void JNICALL
Java_com_visioncamerapluginanpr_VisionCameraPluginAnprModule_nativeAddAssetPaths(JNIEnv* env, jobject clazz, jstring configFilePath, jstring runtimeDirPath, jstring plateFilePath) {
    const char* configPath = env->GetStringUTFChars(configFilePath, nullptr);
    const char* runtimePath = env->GetStringUTFChars(runtimeDirPath, nullptr);
    const char* platePath = env->GetStringUTFChars(plateFilePath, 0);

    // Store the paths globally
    visioncamerapluginanpr::setAlprPaths(configPath, runtimePath, platePath);

    env->ReleaseStringUTFChars(configFilePath, configPath);
    env->ReleaseStringUTFChars(runtimeDirPath, runtimePath);
    env->ReleaseStringUTFChars(plateFilePath, platePath);
}