#include <vulkan/vulkan.h>
#include <vulkan/vulkan_android.h>
#include <android/hardware_buffer.h>
#include <vector>
#include <stdexcept>
#include <iostream>
#include <fstream>

#define VK_CHECK(x)                                                 \
    do {                                                            \
        VkResult err = x;                                           \
        if (err != VK_SUCCESS) {                                    \
            std::cerr << "Vulkan error: " << err << ", File: " << __FILE__ << ", Line: " << __LINE__ << std::endl; \
            throw std::runtime_error("Vulkan error");               \
        }                                                           \
    } while (0)

class VulkanYPlaneRotator {
public:
    VulkanYPlaneRotator(AHardwareBuffer* hardwareBuffer, uint32_t width, uint32_t height)
        : hardwareBuffer(hardwareBuffer), width(width), height(height) {
        initVulkan();
        createBuffers();
        createComputePipeline();
    }

    ~VulkanYPlaneRotator() {
        cleanup();
    }

    std::vector<uint8_t> rotateYPlane() {
        executeComputeShader();
        return retrieveOutputData();
    }

private:
    AHardwareBuffer* hardwareBuffer;
    uint32_t width, height;

    VkInstance instance;
    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
    VkDevice device;
    VkQueue computeQueue;
    VkCommandPool commandPool;
    VkCommandBuffer commandBuffer;
    VkPipelineLayout pipelineLayout;
    VkPipeline computePipeline;
    VkDescriptorSetLayout descriptorSetLayout;
    VkDescriptorSet descriptorSet;
    VkDescriptorPool descriptorPool;
    VkBuffer inputBuffer, outputBuffer;
    VkDeviceMemory inputMemory, outputMemory;

    void initVulkan() {
        createInstance();
        pickPhysicalDevice();
        createLogicalDevice();
        createCommandPool();
    }

    void createInstance() {
        VkApplicationInfo appInfo{};
        appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        appInfo.pApplicationName = "YPlaneRotator";
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

        return deviceFeatures.geometryShader; // Check for necessary features
    }

    void createLogicalDevice() {
        VkDeviceQueueCreateInfo queueCreateInfo{};
        queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueCreateInfo.queueFamilyIndex = findQueueFamilies(physicalDevice);
        queueCreateInfo.queueCount = 1;

        float queuePriority = 1.0f;
        queueCreateInfo.pQueuePriorities = &queuePriority;

        VkPhysicalDeviceFeatures deviceFeatures{};
        VkDeviceCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        createInfo.pQueueCreateInfos = &queueCreateInfo;
        createInfo.queueCreateInfoCount = 1;
        createInfo.pEnabledFeatures = &deviceFeatures;

        VK_CHECK(vkCreateDevice(physicalDevice, &createInfo, nullptr, &device));

        vkGetDeviceQueue(device, findQueueFamilies(physicalDevice), 0, &computeQueue);
    }

    uint32_t findQueueFamilies(VkPhysicalDevice device) {
        uint32_t queueFamilyCount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);

        std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
        vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());

        for (uint32_t i = 0; i < queueFamilyCount; i++) {
            if (queueFamilies[i].queueFlags & VK_QUEUE_COMPUTE_BIT) {
                return i;
            }
        }

        throw std::runtime_error("Failed to find a suitable queue family.");
    }

    void createCommandPool() {
        VkCommandPoolCreateInfo poolInfo{};
        poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        poolInfo.queueFamilyIndex = findQueueFamilies(physicalDevice);
        poolInfo.flags = 0;

        VK_CHECK(vkCreateCommandPool(device, &poolInfo, nullptr, &commandPool));
    }

    void createBuffers() {
        createBuffer(width * height, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, &inputBuffer, &inputMemory);
        createBuffer(width * height, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT, &outputBuffer, &outputMemory);
    }

    void createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkBuffer* buffer, VkDeviceMemory* bufferMemory) {
        VkBufferCreateInfo bufferInfo{};
        bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferInfo.size = size;
        bufferInfo.usage = usage;
        bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        VK_CHECK(vkCreateBuffer(device, &bufferInfo, nullptr, buffer));

        VkMemoryRequirements memRequirements;
        vkGetBufferMemoryRequirements(device, *buffer, &memRequirements);

        VkMemoryAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize = memRequirements.size;
        allocInfo.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

        VK_CHECK(vkAllocateMemory(device, &allocInfo, nullptr, bufferMemory));
        VK_CHECK(vkBindBufferMemory(device, *buffer, *bufferMemory, 0));
    }

    uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) {
        VkPhysicalDeviceMemoryProperties memProperties;
        vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProperties);

        for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
            if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
                return i;
            }
        }

        throw std::runtime_error("Failed to find suitable memory type.");
    }

    void createComputePipeline() {
        auto shaderCode = readFile("/data/user/0/visioncamerapluginanpr.example/files/shaders/compute.spv");

        VkShaderModule shaderModule = createShaderModule(shaderCode);

        VkPipelineShaderStageCreateInfo shaderStageInfo{};
        shaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        shaderStageInfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
        shaderStageInfo.module = shaderModule;
        shaderStageInfo.pName = "main";

        VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
        pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipelineLayoutInfo.setLayoutCount = 0;
        pipelineLayoutInfo.pSetLayouts = nullptr;
        pipelineLayoutInfo.pushConstantRangeCount = 0;
        pipelineLayoutInfo.pPushConstantRanges = nullptr;

        VK_CHECK(vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &pipelineLayout));

        VkComputePipelineCreateInfo pipelineInfo{};
        pipelineInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
        pipelineInfo.stage = shaderStageInfo;
        pipelineInfo.layout = pipelineLayout;

        VK_CHECK(vkCreateComputePipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &computePipeline));

        vkDestroyShaderModule(device, shaderModule, nullptr);
    }

    VkShaderModule createShaderModule(const std::vector<char>& code) {
        VkShaderModuleCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        createInfo.codeSize = code.size();
        createInfo.pCode = reinterpret_cast<const uint32_t*>(code.data());

        VkShaderModule shaderModule;
        VK_CHECK(vkCreateShaderModule(device, &createInfo, nullptr, &shaderModule));

        return shaderModule;
    }

    std::vector<char> readFile(const std::string& filename) {
        std::ifstream file(filename, std::ios::ate | std::ios::binary);

        if (!file.is_open()) {
            throw std::runtime_error("Failed to open file!");
        }

        size_t fileSize = (size_t)file.tellg();
        std::vector<char> buffer(fileSize);

        file.seekg(0);
        file.read(buffer.data(), fileSize);

        file.close();

        return buffer;
    }

    void executeComputeShader() {
        VkCommandBufferAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandPool = commandPool;
        allocInfo.commandBufferCount = 1;

        VK_CHECK(vkAllocateCommandBuffers(device, &allocInfo, &commandBuffer));

        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

        VK_CHECK(vkBeginCommandBuffer(commandBuffer, &beginInfo));

        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, computePipeline);

        VkBufferCopy copyRegion{};
        copyRegion.size = width * height;
        vkCmdCopyBuffer(commandBuffer, inputBuffer, outputBuffer, 1, &copyRegion);

        VK_CHECK(vkEndCommandBuffer(commandBuffer));

        VkSubmitInfo submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &commandBuffer;

        VK_CHECK(vkQueueSubmit(computeQueue, 1, &submitInfo, VK_NULL_HANDLE));
        VK_CHECK(vkQueueWaitIdle(computeQueue));

        vkFreeCommandBuffers(device, commandPool, 1, &commandBuffer);
    }

    std::vector<uint8_t> retrieveOutputData() {
        void* data;
        vkMapMemory(device, outputMemory, 0, width * height, 0, &data);

        std::vector<uint8_t> outputData(width * height);
        memcpy(outputData.data(), data, static_cast<size_t>(width * height));

        vkUnmapMemory(device, outputMemory);

        return outputData;
    }

    void cleanup() {
        vkDestroyBuffer(device, outputBuffer, nullptr);
        vkFreeMemory(device, outputMemory, nullptr);
        vkDestroyBuffer(device, inputBuffer, nullptr);
        vkFreeMemory(device, inputMemory, nullptr);

        vkDestroyPipeline(device, computePipeline, nullptr);
        vkDestroyPipelineLayout(device, pipelineLayout, nullptr);

        vkDestroyCommandPool(device, commandPool, nullptr);
        vkDestroyDevice(device, nullptr);
        vkDestroyInstance(instance, nullptr);
    }
};

int main() {
    AHardwareBuffer* hardwareBuffer; // Assume this is initialized with valid data
    uint32_t width = 1280;
    uint32_t height = 720;

    VulkanYPlaneRotator rotator(hardwareBuffer, width, height);
    std::vector<uint8_t> rotatedYPlane = rotator.rotateYPlane();

    // Use the rotated Y-plane data as needed...

    return 0;
}
