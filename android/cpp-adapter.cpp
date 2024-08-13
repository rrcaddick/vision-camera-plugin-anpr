#include <jni.h>
#include "vision-camera-plugin-anpr.h"

extern "C"
JNIEXPORT jdouble JNICALL
Java_com_visioncamerapluginanpr_VisionCameraPluginAnprModule_nativeMultiply(JNIEnv *env, jclass type, jdouble a, jdouble b) {
    return visioncamerapluginanpr::multiply(a, b);
}
