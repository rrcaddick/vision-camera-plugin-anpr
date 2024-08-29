package com.visioncamerapluginanpr

import android.util.Log
import com.facebook.react.bridge.JavaScriptContextHolder
import com.facebook.react.bridge.ReactApplicationContext
import com.facebook.react.bridge.ReactContextBaseJavaModule
import com.facebook.react.bridge.ReactMethod

import java.io.File

class VisionCameraPluginAnprModule(reactContext: ReactApplicationContext) : ReactContextBaseJavaModule(reactContext) {
  companion object {
    const val NAME = "VisionCameraPluginAnpr"

    init {
      System.loadLibrary("jpgt")
      System.loadLibrary("pngt")
      System.loadLibrary("leptonica")
      System.loadLibrary("tesseract")
      System.loadLibrary("VisionCameraPluginAnpr")
    }

    // Adds frame processor plugin to JS global variables
    @JvmStatic
    external fun nativeInstallPlugins(jsi: Long)

    @JvmStatic
    external fun nativeAddAssetPaths(configFilePath: String, runtimeDirPath: String, plateFilePath: String)
  }

  override fun getName(): String {
    return NAME
  }

  override fun initialize() {
      super.initialize()

      // Copies conf and runtime_data required by openalpr
      copyAssetsToDataDirectory()

      // Copies the shader
      AssetUtils.copyShader(getReactApplicationContext())

      // Calculate the paths
      val configFilePath = File(getReactApplicationContext().filesDir, "openalpr/openalpr.conf").absolutePath
      val runtimeDirPath = File(getReactApplicationContext().filesDir, "openalpr/runtime_data").absolutePath
      val plateFilePath = File(getReactApplicationContext().filesDir, "openalpr/no-plate-large.jpg").absolutePath

      Log.d("ReactNative", "Initialize called")
      
      // Call the native method to pass the paths
      nativeAddAssetPaths(configFilePath, runtimeDirPath, plateFilePath)
  }

  @ReactMethod(isBlockingSynchronousMethod = true)
  fun installPlugins() {
    val jsContext: JavaScriptContextHolder? = getReactApplicationContext().javaScriptContextHolder

    if (jsContext != null && jsContext.get() != 0L) {
      nativeInstallPlugins(jsContext.get())
    } else {
      Log.e("ReactNative", "JSI Runtime is not available in debug mode")
    }
  }

  private fun copyAssetsToDataDirectory() {
    val appDir = File(getReactApplicationContext().filesDir, "openalpr")
    if (!appDir.exists()) {
        appDir.mkdir()
    }

    AssetUtils.copyAssets(getReactApplicationContext(), "", appDir)
  }
}
