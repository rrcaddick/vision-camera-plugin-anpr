package com.visioncamerapluginanpr

import android.util.Log
import com.facebook.react.bridge.JavaScriptContextHolder
import com.facebook.react.bridge.ReactApplicationContext
import com.facebook.react.bridge.ReactContextBaseJavaModule
import com.facebook.react.bridge.ReactMethod

import java.io.File
import java.io.IOException

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
    external fun nativeInstallPlugin(jsi: Long)

    @JvmStatic
    external fun nativeAddAssetPaths(configFilePath: String, runtimeDirPath: String)
  }

  override fun getName(): String {
    return NAME
  }

  override fun initialize() {
      super.initialize()

      Log.d("ReactNative", "Starting initialization")

      // Copies conf and runtime_data required by openalpr
      copyAssetsToDataDirectory()

      // Calculate the paths
      val configFilePath = File(getReactApplicationContext().filesDir, "openalpr/openalpr.conf").absolutePath
      val runtimeDirPath = File(getReactApplicationContext().filesDir, "openalpr/runtime_data").absolutePath
      
      // Call the native method to pass the paths
      nativeAddAssetPaths(configFilePath, runtimeDirPath)

      Log.d("ReactNative", "Initialization complete")
  }


  private fun copyAssetsToDataDirectory() {
    val appDir = File(getReactApplicationContext().filesDir, "openalpr")
    if (!appDir.exists()) {
      appDir.mkdir()
    }
    
    AssetUtils.copyAssets(getReactApplicationContext(), appDir)
  }

  @ReactMethod(isBlockingSynchronousMethod = true)
  fun installPlugin() {
    val jsContext: JavaScriptContextHolder? = getReactApplicationContext().javaScriptContextHolder

    if (jsContext != null && jsContext.get() != 0L) {
      nativeInstallPlugin(jsContext.get())
    } else {
      Log.e("ReactNative", "JSI Runtime is not available in debug mode")
    }
  }
}
