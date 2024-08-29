#include <vulkan/vulkan.h>
#include <vulkan/vulkan_android.h>
#include <android/hardware_buffer.h>
#include <android/log.h>
#include <fstream>
#include <iostream>
#include <vector>
#include <stdexcept>
#include <algorithm>
#include <memory>

// #define VK_CHECK(x) \
//     do { \
//         VkResult err = x; \
//         if (err) { \
//             __android_log_print(ANDROID_LOG_ERROR, "ReactNative", "Vulkan error: %d", err); \
//             throw std::runtime_error("Vulkan error"); \
//         } \
//     } while (0)

#define LOG_TAG "ReactNative"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

#define VK_CHECK(x)                                                 \
    do {                                                            \
        VkResult err = x;                                           \
        if (err) {                                                  \
            LOGE("Vulkan error: %d, File: %s, Line: %d", err, __FILE__, __LINE__); \
            throw std::runtime_error("Vulkan error");               \
        }                                                           \
    } while (0)

namespace vulkanProcessor {
  extern std::string g_shaderPath;

  inline void setShaderPath() {
      g_shaderPath = "/data/user/0/com.visioncamerapluginanpr/files/shaders/compute.spv";
      LOGI("Shader path set to: %s", g_shaderPath.c_str());
  }

  class VulkanProcessor {
  public:
      VulkanProcessor() {
        try {
            initVulkan();
            createCommandPool();
            createDescriptorSetLayout();
            createComputePipeline();
            createDescriptorPool();
        } catch (const std::exception& e) {
            LOGE("Error in VulkanProcessor constructor: %s", e.what());
            cleanup();
            throw;
        }
      }

      ~VulkanProcessor() {
          cleanup();
      }


      std::vector<uint8_t> processImage(AHardwareBuffer* hardwareBuffer, uint32_t width, uint32_t height) {
        auto inputBuffers = createBufferFromHardwareBuffer(hardwareBuffer);
    
        // Log information about input buffers
        for (int i = 0; i < 3; ++i) {
            VkMemoryRequirements memRequirements;
            vkGetBufferMemoryRequirements(device, inputBuffers[i], &memRequirements);
            LOGI("Input buffer %d size: %zu", i, memRequirements.size);
        }
            
        VkBuffer outputBuffer = createBuffer(width * height, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
        LOGI("Output buffer created successfully. Size: %u", width * height);

        VkDescriptorSet descriptorSet = createDescriptorSet(inputBuffers[0], inputBuffers[1], inputBuffers[2], outputBuffer);
        LOGI("Descriptor set created successfully");

        VkCommandBuffer commandBuffer = beginSingleTimeCommands();
        LOGI("Command buffer created and recording started");

        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, computePipeline);
        LOGI("Pipeline bound to command buffer");

        vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineLayout, 0, 1, &descriptorSet, 0, nullptr);
        LOGI("Descriptor set bound to command buffer");

        struct PushConstants {
            uint32_t width;
            uint32_t height;
            uint32_t yStride;
            uint32_t uvStride;
        } pushConstants{width, height, width, width / 2};

        vkCmdPushConstants(commandBuffer, pipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(PushConstants), &pushConstants);

        LOGI("Push constants set");

        LOGI("Dispatching compute shader with grid size: %ux%ux1", (width + 15) / 16, (height + 15) / 16);
        vkCmdDispatch(commandBuffer, (width + 15) / 16, (height + 15) / 16, 1);

        LOGI("Command buffer recording completed");
        endSingleTimeCommands(commandBuffer);

        LOGI("Compute operations completed successfully");

        std::vector<uint8_t> result(width * height);
        void* mappedMemory;
        vkMapMemory(device, outputBufferMemory, 0, width * height, 0, &mappedMemory);
        memcpy(result.data(), mappedMemory, width * height);
        vkUnmapMemory(device, outputBufferMemory);

        LOGI("Result copied to output vector");

        // Clean up resources
        for (auto& buffer : inputBuffers) {
            vkDestroyBuffer(device, buffer, nullptr);
        }
        vkDestroyBuffer(device, outputBuffer, nullptr);
        vkFreeMemory(device, outputBufferMemory, nullptr);
        vkFreeDescriptorSets(device, descriptorPool, 1, &descriptorSet);

        LOGI("Resources cleaned up");

        return result;
    }

  private:
      VkInstance instance;
      VkPhysicalDevice physicalDevice;
      VkDevice device;
      VkQueue computeQueue;
      VkCommandPool commandPool;
      VkDescriptorPool descriptorPool;
      VkDescriptorSetLayout descriptorSetLayout;
      VkPipelineLayout pipelineLayout;
      VkPipeline computePipeline;
      VkShaderModule computeShaderModule;
      VkDeviceMemory outputBufferMemory;

      void initVulkan() {
          createInstance();
          pickPhysicalDevice();
          createLogicalDevice();
      }

      void createInstance() {
          VkApplicationInfo appInfo{};
          appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
          appInfo.pApplicationName = "OpenALPR Vulkan Processor";
          appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
          appInfo.pEngineName = "No Engine";
          appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
          appInfo.apiVersion = VK_API_VERSION_1_1;

          VkInstanceCreateInfo createInfo{};
          createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
          createInfo.pApplicationInfo = &appInfo;

          std::vector<const char*> extensions = {
              VK_KHR_SURFACE_EXTENSION_NAME,
              VK_KHR_ANDROID_SURFACE_EXTENSION_NAME,
              VK_KHR_EXTERNAL_MEMORY_CAPABILITIES_EXTENSION_NAME,
              VK_KHR_EXTERNAL_SEMAPHORE_CAPABILITIES_EXTENSION_NAME
          };
          createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
          createInfo.ppEnabledExtensionNames = extensions.data();

          VK_CHECK(vkCreateInstance(&createInfo, nullptr, &instance));
      }

      void pickPhysicalDevice() {
          uint32_t deviceCount = 0;
          vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);
          if (deviceCount == 0) {
              throw std::runtime_error("Failed to find GPUs with Vulkan support");
          }

          std::vector<VkPhysicalDevice> devices(deviceCount);
          vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data());

          for (const auto& device : devices) {
              if (isDeviceSuitable(device)) {
                  physicalDevice = device;
                  break;
              }
          }

          if (physicalDevice == VK_NULL_HANDLE) {
              throw std::runtime_error("Failed to find a suitable GPU");
          }
      }

      bool isDeviceSuitable(VkPhysicalDevice device) {
          VkPhysicalDeviceProperties deviceProperties;
          VkPhysicalDeviceFeatures deviceFeatures;
          vkGetPhysicalDeviceProperties(device, &deviceProperties);
          vkGetPhysicalDeviceFeatures(device, &deviceFeatures);

          // Log device info
          __android_log_print(ANDROID_LOG_INFO, "VulkanProcessor", "Checking device: %s", deviceProperties.deviceName);
          __android_log_print(ANDROID_LOG_INFO, "VulkanProcessor", "Device type: %d", deviceProperties.deviceType);
          __android_log_print(ANDROID_LOG_INFO, "VulkanProcessor", "API version: %d.%d.%d",
              VK_VERSION_MAJOR(deviceProperties.apiVersion),
              VK_VERSION_MINOR(deviceProperties.apiVersion),
              VK_VERSION_PATCH(deviceProperties.apiVersion));

          // Check for compute capability
          // VkQueueFamilyProperties queueFamilyProperties;
          uint32_t queueFamilyCount = 0;
          vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);
          std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
          vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());

          bool hasComputeQueue = false;
          for (const auto& queueFamily : queueFamilies) {
              if (queueFamily.queueFlags & VK_QUEUE_COMPUTE_BIT) {
                  hasComputeQueue = true;
                  break;
              }
          }

          return hasComputeQueue && deviceFeatures.shaderStorageImageExtendedFormats;
      }

      void createLogicalDevice() {
          VkDeviceQueueCreateInfo queueCreateInfo{};
          queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
          queueCreateInfo.queueFamilyIndex = findComputeQueueFamily();
          queueCreateInfo.queueCount = 1;
          float queuePriority = 1.0f;
          queueCreateInfo.pQueuePriorities = &queuePriority;

          VkPhysicalDeviceFeatures deviceFeatures{};
          deviceFeatures.shaderStorageImageExtendedFormats = VK_TRUE;

          VkDeviceCreateInfo createInfo{};
          createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
          createInfo.pQueueCreateInfos = &queueCreateInfo;
          createInfo.queueCreateInfoCount = 1;
          createInfo.pEnabledFeatures = &deviceFeatures;

          std::vector<const char*> deviceExtensions = {
              VK_KHR_EXTERNAL_MEMORY_EXTENSION_NAME,
              VK_KHR_EXTERNAL_MEMORY_FD_EXTENSION_NAME,
              VK_ANDROID_EXTERNAL_MEMORY_ANDROID_HARDWARE_BUFFER_EXTENSION_NAME,
              VK_KHR_STORAGE_BUFFER_STORAGE_CLASS_EXTENSION_NAME
          };
          createInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size());
          createInfo.ppEnabledExtensionNames = deviceExtensions.data();

          VK_CHECK(vkCreateDevice(physicalDevice, &createInfo, nullptr, &device));

          vkGetDeviceQueue(device, findComputeQueueFamily(), 0, &computeQueue);
      }

      uint32_t findComputeQueueFamily() {
          uint32_t queueFamilyCount = 0;
          vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, nullptr);

          std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
          vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, queueFamilies.data());

          for (uint32_t i = 0; i < queueFamilyCount; i++) {
              if (queueFamilies[i].queueFlags & VK_QUEUE_COMPUTE_BIT) {
                  return i;
              }
          }

          throw std::runtime_error("Failed to find a compute queue family");
      }

      void createCommandPool() {
          VkCommandPoolCreateInfo poolInfo{};
          poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
          poolInfo.queueFamilyIndex = findComputeQueueFamily();
          poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

          VK_CHECK(vkCreateCommandPool(device, &poolInfo, nullptr, &commandPool));
      }

      void createDescriptorSetLayout() {
          std::array<VkDescriptorSetLayoutBinding, 4> layoutBindings{};
          for (int i = 0; i < 4; ++i) {
              layoutBindings[i].binding = i;
              layoutBindings[i].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
              layoutBindings[i].descriptorCount = 1;
              layoutBindings[i].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
          }

          VkDescriptorSetLayoutCreateInfo layoutInfo{};
          layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
          layoutInfo.bindingCount = static_cast<uint32_t>(layoutBindings.size());
          layoutInfo.pBindings = layoutBindings.data();

          VK_CHECK(vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &descriptorSetLayout));
      }

      void createDescriptorPool() {
          VkDescriptorPoolSize poolSize{};
          poolSize.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
          poolSize.descriptorCount = 4;  // One for each plane (Y, U, V) and one for output

          VkDescriptorPoolCreateInfo poolInfo{};
          poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
          poolInfo.poolSizeCount = 1;
          poolInfo.pPoolSizes = &poolSize;
          poolInfo.maxSets = 1;

          VK_CHECK(vkCreateDescriptorPool(device, &poolInfo, nullptr, &descriptorPool));
      }

      void createComputePipeline() {
          auto shaderCode = readShaderFile();
          VkShaderModuleCreateInfo createInfo{};
          createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
          createInfo.codeSize = shaderCode.size();
          createInfo.pCode = reinterpret_cast<const uint32_t*>(shaderCode.data());

          VkShaderModule shaderModule;
          VK_CHECK(vkCreateShaderModule(device, &createInfo, nullptr, &shaderModule));

          VkPipelineShaderStageCreateInfo shaderStageInfo{};
          shaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
          shaderStageInfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
          shaderStageInfo.module = shaderModule;
          shaderStageInfo.pName = "main";

          VkPushConstantRange pushConstantRange{};
          pushConstantRange.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
          pushConstantRange.offset = 0;
          pushConstantRange.size = sizeof(uint32_t) * 4;  // For width, height, yStride, and uvStride

          VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
          pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
          pipelineLayoutInfo.setLayoutCount = 1;
          pipelineLayoutInfo.pSetLayouts = &descriptorSetLayout;
          pipelineLayoutInfo.pushConstantRangeCount = 1;
          pipelineLayoutInfo.pPushConstantRanges = &pushConstantRange;

          VK_CHECK(vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &pipelineLayout));

          VkComputePipelineCreateInfo pipelineInfo{};
          pipelineInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
          pipelineInfo.stage = shaderStageInfo;
          pipelineInfo.layout = pipelineLayout;

          VK_CHECK(vkCreateComputePipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &computePipeline));

          vkDestroyShaderModule(device, shaderModule, nullptr);
      }

      std::array<VkBuffer, 3> createBufferFromHardwareBuffer(AHardwareBuffer* hardwareBuffer) {
          AHardwareBuffer_Desc bufferDesc;
          AHardwareBuffer_describe(hardwareBuffer, &bufferDesc);
          LOGI("AHardwareBuffer description - width: %u, height: %u, layers: %u, format: %u, usage: 0x%lx",
              bufferDesc.width, bufferDesc.height, bufferDesc.layers, bufferDesc.format, bufferDesc.usage);

          VkAndroidHardwareBufferFormatPropertiesANDROID formatInfo{};
          formatInfo.sType = VK_STRUCTURE_TYPE_ANDROID_HARDWARE_BUFFER_FORMAT_PROPERTIES_ANDROID;

          VkAndroidHardwareBufferPropertiesANDROID properties{};
          properties.sType = VK_STRUCTURE_TYPE_ANDROID_HARDWARE_BUFFER_PROPERTIES_ANDROID;
          properties.pNext = &formatInfo;

          VK_CHECK(vkGetAndroidHardwareBufferPropertiesANDROID(device, hardwareBuffer, &properties));

          LOGI("Hardware buffer size: %zu", properties.allocationSize);
          LOGI("Hardware buffer format: %d", formatInfo.format);
          LOGI("Hardware buffer external format: %lu", formatInfo.externalFormat);

          std::array<VkBuffer, 3> buffers;
          std::array<VkDeviceSize, 3> planeSizes = {
              bufferDesc.width * bufferDesc.height,
              bufferDesc.width * bufferDesc.height / 4,
              bufferDesc.width * bufferDesc.height / 4
          };

          for (int i = 0; i < 3; ++i) {
              VkBufferCreateInfo bufferInfo{};
              bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
              bufferInfo.size = planeSizes[i];
              bufferInfo.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
              bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

              VK_CHECK(vkCreateBuffer(device, &bufferInfo, nullptr, &buffers[i]));

              VkMemoryRequirements memRequirements;
              vkGetBufferMemoryRequirements(device, buffers[i], &memRequirements);

              VkImportAndroidHardwareBufferInfoANDROID importInfo{};
              importInfo.sType = VK_STRUCTURE_TYPE_IMPORT_ANDROID_HARDWARE_BUFFER_INFO_ANDROID;
              importInfo.buffer = hardwareBuffer;

              VkMemoryDedicatedAllocateInfo dedicatedAllocInfo{};
              dedicatedAllocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO;
              dedicatedAllocInfo.pNext = &importInfo;
              dedicatedAllocInfo.buffer = buffers[i];

              VkMemoryAllocateInfo allocInfo{};
              allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
              allocInfo.pNext = &dedicatedAllocInfo;
              allocInfo.allocationSize = memRequirements.size;
              allocInfo.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

              VkDeviceMemory bufferMemory;
              VK_CHECK(vkAllocateMemory(device, &allocInfo, nullptr, &bufferMemory));
              VK_CHECK(vkBindBufferMemory(device, buffers[i], bufferMemory, 0));
          }

          return buffers;
      }

      VkBuffer createBuffer(VkDeviceSize size, VkBufferUsageFlags usage) {
          LOGI("Creating buffer of size %zu with usage flags 0x%x", size, usage);
          
          VkBuffer buffer;
          VkBufferCreateInfo bufferInfo{};
          bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
          bufferInfo.size = size;
          bufferInfo.usage = usage;
          bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

          VkResult result = vkCreateBuffer(device, &bufferInfo, nullptr, &buffer);
          if (result != VK_SUCCESS) {
              LOGE("Failed to create buffer. Error: %d", result);
              throw std::runtime_error("Failed to create buffer");
          }

          VkMemoryRequirements memRequirements;
          vkGetBufferMemoryRequirements(device, buffer, &memRequirements);

          VkMemoryAllocateInfo allocInfo{};
          allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
          allocInfo.allocationSize = memRequirements.size;
          allocInfo.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

          result = vkAllocateMemory(device, &allocInfo, nullptr, &outputBufferMemory);
          if (result != VK_SUCCESS) {
              LOGE("Failed to allocate buffer memory. Error: %d", result);
              throw std::runtime_error("Failed to allocate buffer memory");
          }

          result = vkBindBufferMemory(device, buffer, outputBufferMemory, 0);
          if (result != VK_SUCCESS) {
              LOGE("Failed to bind buffer memory. Error: %d", result);
              throw std::runtime_error("Failed to bind buffer memory");
          }

          LOGI("Buffer created successfully");
          return buffer;
      }

      uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) {
          VkPhysicalDeviceMemoryProperties memProperties;
          vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProperties);

          for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
              if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
                  return i;
              }
          }

          throw std::runtime_error("Failed to find suitable memory type");
      }

      VkDescriptorSet createDescriptorSet(VkBuffer yBuffer, VkBuffer uBuffer, VkBuffer vBuffer, VkBuffer outputBuffer) {
          VkDescriptorSetAllocateInfo allocInfo{};
          allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
          allocInfo.descriptorPool = descriptorPool;
          allocInfo.descriptorSetCount = 1;
          allocInfo.pSetLayouts = &descriptorSetLayout;

          VkDescriptorSet descriptorSet;
          VK_CHECK(vkAllocateDescriptorSets(device, &allocInfo, &descriptorSet));

          std::array<VkDescriptorBufferInfo, 4> bufferInfos = {
              {{yBuffer, 0, VK_WHOLE_SIZE},
              {uBuffer, 0, VK_WHOLE_SIZE},
              {vBuffer, 0, VK_WHOLE_SIZE},
              {outputBuffer, 0, VK_WHOLE_SIZE}}
          };

          std::array<VkWriteDescriptorSet, 4> descriptorWrites{};
          for (int i = 0; i < 4; ++i) {
              descriptorWrites[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
              descriptorWrites[i].dstSet = descriptorSet;
              descriptorWrites[i].dstBinding = i;
              descriptorWrites[i].dstArrayElement = 0;
              descriptorWrites[i].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
              descriptorWrites[i].descriptorCount = 1;
              descriptorWrites[i].pBufferInfo = &bufferInfos[i];
          }

          vkUpdateDescriptorSets(device, static_cast<uint32_t>(descriptorWrites.size()), descriptorWrites.data(), 0, nullptr);

          return descriptorSet;
      }

      VkCommandBuffer beginSingleTimeCommands() {
          VkCommandBufferAllocateInfo allocInfo{};
          allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
          allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
          allocInfo.commandPool = commandPool;
          allocInfo.commandBufferCount = 1;

          VkCommandBuffer commandBuffer;
          VK_CHECK(vkAllocateCommandBuffers(device, &allocInfo, &commandBuffer));

          VkCommandBufferBeginInfo beginInfo{};
          beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
          beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

          VK_CHECK(vkBeginCommandBuffer(commandBuffer, &beginInfo));

          return commandBuffer;
      }

      void endSingleTimeCommands(VkCommandBuffer commandBuffer) {
          LOGI("Ending single time commands");
          VK_CHECK(vkEndCommandBuffer(commandBuffer));

          VkSubmitInfo submitInfo{};
          submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
          submitInfo.commandBufferCount = 1;
          submitInfo.pCommandBuffers = &commandBuffer;

          // Create a semaphore for queue synchronization
          VkSemaphoreCreateInfo semaphoreInfo{};
          semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
          VkSemaphore semaphore;
          VK_CHECK(vkCreateSemaphore(device, &semaphoreInfo, nullptr, &semaphore));

          submitInfo.signalSemaphoreCount = 1;
          submitInfo.pSignalSemaphores = &semaphore;

          VkFenceCreateInfo fenceInfo{};
          fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
          VkFence fence;
          VK_CHECK(vkCreateFence(device, &fenceInfo, nullptr, &fence));

          LOGI("Submitting command buffer to queue");
          VkResult result = vkQueueSubmit(computeQueue, 1, &submitInfo, fence);
          if (result != VK_SUCCESS) {
              LOGE("Failed to submit compute command buffer. Error: %d", result);
              throw std::runtime_error("Failed to submit compute command buffer");
          }

          LOGI("Waiting for fence");
          // Increase timeout to 10 seconds (10000000000 nanoseconds)
          result = vkWaitForFences(device, 1, &fence, VK_TRUE, 10000000000);
          if (result != VK_SUCCESS) {
              LOGE("Failed to wait for compute command buffer fence. Error: %d", result);
              
              // Additional error checking
              VkResult fenceStatus = vkGetFenceStatus(device, fence);
              LOGE("Fence status: %d", fenceStatus);

              // Get queue status
              uint32_t queueCount;
              vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueCount, nullptr);
              std::vector<VkQueueFamilyProperties> queueProps(queueCount);
              vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueCount, queueProps.data());
              LOGE("Compute queue family properties: queueFlags=%u, queueCount=%u", 
                  queueProps[findComputeQueueFamily()].queueFlags, 
                  queueProps[findComputeQueueFamily()].queueCount);

              throw std::runtime_error("Failed to wait for compute command buffer fence");
          }

          LOGI("Fence signaled successfully");
          vkDestroySemaphore(device, semaphore, nullptr);
          vkDestroyFence(device, fence, nullptr);
          vkFreeCommandBuffers(device, commandPool, 1, &commandBuffer);
      }

      void recordComputeCommand(VkCommandBuffer commandBuffer, VkDescriptorSet descriptorSet, uint32_t width, uint32_t height) {
          vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, computePipeline);
          vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineLayout, 0, 1, &descriptorSet, 0, nullptr);

          vkCmdDispatch(commandBuffer, (width + 15) / 16, (height + 15) / 16, 1);
      }

      void cleanup() {
          if (device != VK_NULL_HANDLE) {
              vkDeviceWaitIdle(device);
              if (computePipeline != VK_NULL_HANDLE)
                  vkDestroyPipeline(device, computePipeline, nullptr);
              if (pipelineLayout != VK_NULL_HANDLE)
                  vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
              if (descriptorSetLayout != VK_NULL_HANDLE)
                  vkDestroyDescriptorSetLayout(device, descriptorSetLayout, nullptr);
              if (descriptorPool != VK_NULL_HANDLE)
                  vkDestroyDescriptorPool(device, descriptorPool, nullptr);
              if (commandPool != VK_NULL_HANDLE)
                  vkDestroyCommandPool(device, commandPool, nullptr);
              vkDestroyDevice(device, nullptr);
          }
          if (instance != VK_NULL_HANDLE)
              vkDestroyInstance(instance, nullptr);
      }

      std::vector<char> readShaderFile() {
          // LOGI("Attempting to read shader file from: %s", "/data/user/0/visioncamerapluginanpr.example/files/shaders/compute.spv");
          std::ifstream file("/data/user/0/visioncamerapluginanpr.example/files/shaders/compute.spv", std::ios::ate | std::ios::binary);

          if (!file.is_open()) {
              __android_log_print(ANDROID_LOG_ERROR, "ReactNative", "Failed to open shader file: %s", "/data/user/0/visioncamerapluginanpr.example/files/shaders/compute.spv");
              throw std::runtime_error("Failed to open shader file: /data/user/0/visioncamerapluginanpr.example/files/shaders/compute.spv");
          }

          size_t fileSize = (size_t) file.tellg();
          std::vector<char> buffer(fileSize);

          file.seekg(0);
          file.read(buffer.data(), fileSize);

          file.close();

          return buffer;
      }
  };
} // namespace vulkanProcessor

// // Main function to use the VulkanProcessor
// std::vector<uint8_t> processImageWithVulkan(AHardwareBuffer* hardwareBuffer, uint32_t width, uint32_t height) {
//     static VulkanProcessor processor; // Create a static instance to reuse across calls
//     return processor.processImage(hardwareBuffer, width, height);
// }

// // Update the existing processImage function
// std::string processImage(AHardwareBuffer* hardwareBuffer, int width, int height) {
//     std::vector<uint8_t> processedImage = processImageWithVulkan(hardwareBuffer, width, height);

//     // Use the processed image with OpenALPR
//     std::vector<alpr::AlprRegionOfInterest> regionsOfInterest;
//     alpr::AlprResults results = g_openalprInstance->recognize(processedImage.data(), 1, height, width, regionsOfInterest);
//     return alpr::Alpr::toJson(results);
// }