package com.visioncamerapluginanpr

import android.content.Context
import android.util.Log
import java.io.File
import java.io.FileOutputStream
import java.io.IOException
import java.io.InputStream
import java.io.OutputStream

object AssetUtils {
  // Recursively copy everything from an asset directory to the specified destination directory
  @JvmStatic
  fun copyAssets(context: Context, assetDirName: String, destinationDir: File) {
    try {
      val assets = context.assets.list(assetDirName)
      if (assets != null && assets.isNotEmpty()) {
        // It's a directory, create if not exists
        if (!destinationDir.exists()) {
          destinationDir.mkdirs()
        }
        
        for (asset in assets) {
          val assetPath = if (assetDirName.isEmpty()) asset else "$assetDirName/$asset"
          val destinationFile = File(destinationDir, asset)
          copyAssets(context, assetPath, destinationFile) // Recursively copy
        }
      } else {
        // It's a file, copy it
        copyAssetFile(context, assetDirName, destinationDir)
      }
    } catch (e: IOException) {
      Log.e("AssetUtils", "Failed to copy assets: $assetDirName", e)
    }
  }

  @JvmStatic
  fun copyShader(context: Context) {
    Log.d("ReactNative", "Starting shader copy")
    val shaderInputStream = context.assets.open("shaders/compute_shader.comp.spv")
    val shaderDir = File(context.filesDir, "shaders")
    shaderDir.mkdirs()
    val shaderFile = File(shaderDir, "compute.spv")
    shaderFile.outputStream().use { output ->
      shaderInputStream.copyTo(output)
    }
    Log.d("ReactNative", "Finished shader copy")
    Log.d("ReactNative", "Shader copied to: ${shaderFile.absolutePath}")
    Log.d("ReactNative", "Shader file exists: ${shaderFile.exists()}")
    Log.d("ReactNative", "Shader file size: ${shaderFile.length()} bytes")
  }

  // Helper method to copy a single asset file to the specified destination
  private fun copyAssetFile(context: Context, assetFileName: String, destination: File) {
    try {
      context.assets.open(assetFileName).use { inputStream ->
        FileOutputStream(destination).use { outputStream ->
          val buffer = ByteArray(1024)
          var read: Int
          while (inputStream.read(buffer).also { read = it } != -1) {
            outputStream.write(buffer, 0, read)
          }
        }
      }
    } catch (e: IOException) {
      Log.e("ReactNative", "Failed to copy asset file: $assetFileName", e)
    }
  }
}
