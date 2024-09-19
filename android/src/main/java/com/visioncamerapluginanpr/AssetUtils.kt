package com.visioncamerapluginanpr

import android.content.Context
import android.content.res.AssetManager
import android.util.Log
import java.io.File
import java.io.FileOutputStream
import java.io.IOException

object AssetUtils {
  @JvmStatic
  fun copyAssets(context: Context, destinationDir: File) {
    try {
      val assetManager = context.assets

      if (assetExists(assetManager, "openalpr.conf")) {
        copyAssetFile(context, "openalpr.conf", File(destinationDir, "openalpr.conf"))
      } else {
        copyAssetFile(context, "openalpr/openalpr.conf", File(destinationDir, "openalpr.conf"))
      }
      
      // Copy runtime_data folder recursively
      if (assetExists(assetManager, "runtime_data")) {
        copyAssetDirectory(context, "runtime_data", File(destinationDir, "runtime_data"))
      } else {
        copyAssetDirectory(context, "openalpr/runtime_data", File(destinationDir, "runtime_data"))
      }
    } catch (e: IOException) {
      Log.e("ReactNative", "Failed to copy assets", e)
    }
  }

  // Recursively copy assets from a directory to the specified destination directory
  private fun copyAssetDirectory(context: Context, assetDirName: String, destinationDir: File) {
    try {
      val assets = context.assets.list(assetDirName)
      if (assets != null && assets.isNotEmpty()) {
        // It's a directory, create if not exists
        if (!destinationDir.exists()) {
          destinationDir.mkdirs()
        }
        
        for (asset in assets) {
          val assetPath = "$assetDirName/$asset"
          val destinationFile = File(destinationDir, asset)
          copyAssetDirectory(context, assetPath, destinationFile) // Recursively copy
        }
      } else {
        // It's a file, copy it
        copyAssetFile(context, assetDirName, destinationDir)
      }
    } catch (e: IOException) {
      Log.e("ReactNative", "Failed to copy assets: $assetDirName", e)
    }
  }

  // Helper method to copy a single asset file to the specified destination
  private fun copyAssetFile(context: Context, assetFileName: String, destination: File) {
    try {
      context.assets.open(assetFileName).use { inputStream ->
        FileOutputStream(destination).use { outputStream ->
          inputStream.copyTo(outputStream)
        }
      }
    } catch (e: IOException) {
      Log.e("ReactNative", "Failed to copy asset file: $assetFileName", e)
    }
  }

  // Helper function to check if an asset exists
  private fun assetExists(assetManager: AssetManager, path: String): Boolean {
    return try {
      val assets = assetManager.list(path)
      assets != null && assets.isNotEmpty()
    } catch (e: IOException) {
      false
    }
  }
}