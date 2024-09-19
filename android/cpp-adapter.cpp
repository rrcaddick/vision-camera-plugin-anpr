#include <jni.h>
#include <jsi/jsi.h>
#include "vision-camera-plugin-anpr.h"
#include <string>

using namespace facebook;

extern "C"
JNIEXPORT void JNICALL
Java_com_visioncamerapluginanpr_VisionCameraPluginAnprModule_nativeInstallPlugin(JNIEnv *env, jclass type, jlong jsi) {
    auto runtime = reinterpret_cast<facebook::jsi::Runtime *>(jsi);

    if (runtime) {
        visioncamerapluginanpr::installPlugin(*runtime);
    }
}

extern "C"
JNIEXPORT void JNICALL
Java_com_visioncamerapluginanpr_VisionCameraPluginAnprModule_nativeAddAssetPaths(JNIEnv* env, jobject clazz, jstring configFilePath, jstring runtimeDirPath) {
    const char* configPath = env->GetStringUTFChars(configFilePath, nullptr);
    const char* runtimePath = env->GetStringUTFChars(runtimeDirPath, nullptr);

    // Store the paths globally
    visioncamerapluginanpr::setAlprPaths(configPath, runtimePath);

    env->ReleaseStringUTFChars(configFilePath, configPath);
    env->ReleaseStringUTFChars(runtimeDirPath, runtimePath);
}