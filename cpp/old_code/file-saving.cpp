#include <jsi/jsi.h>
#include <string>

namespace androidFileSave {
  using namespace facebook;

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

    void saveImage(const std::string& fileName, const std::string& content) {
        std::string downloadsPath = getDownloadsPath();
        cv::imwrite(downloadsPath + "/${fileName}", yMat);
    }
}