#pragma once
#include <vulkan/vulkan.h>
#include <vector>
#include <array>

namespace nes {
class VulkanRenderer {
public:
    VulkanRenderer();
    ~VulkanRenderer();
    void init(uint32_t width, uint32_t height);
    void update_frame(const std::vector<uint8_t>& rgb_pixels);
    void render();

private:
    VkInstance instance_;
    VkDevice device_;
    VkQueue queue_;
    VkSwapchainKHR swapchain_;
    std::vector<VkImageView> image_views_;
    VkPipeline pipeline_;
    VkBuffer vertex_buffer_, index_buffer_;
    VkDeviceMemory vertex_memory_, index_memory_;
    VkDescriptorSet descriptor_set_;
    VkCommandBuffer command_buffer_;
    // ... other Vulkan handles ...

    void create_instance();
    void create_device();
    void create_swapchain(uint32_t width, uint32_t height);
    void create_pipeline();
    void create_buffers();
};
}