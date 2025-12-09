// =============================================================================
// vkHomeGrown.h - Vulkan Rendering API
// =============================================================================

#ifndef VKHOMEGROWN_H
#define VKHOMEGROWN_H

#include <vulkan/vulkan.h>
#include <GLFW/glfw3.h>
#include <stdlib.h>
#include <stdbool.h>

// =============================================================================
// MACROS
// =============================================================================

#define VULKAN_CHECK(result) if((result) != VK_SUCCESS) { \
    printf("vulkan error at %s:%d: %d\n", __FILE__, __LINE__, (result)); \
    exit(1); \
}

// =============================================================================
// CORE TYPES - Used throughout the API
// =============================================================================


typedef struct _hgVertex
{
    float x, y;       // position
    float r, g, b, a; // color
    float u, v;       // texture coords
} hgVertex;

typedef struct _hgTexture
{
    VkImage        tImage;
    VkImageView    tImageView;
    VkDeviceMemory tMemory;
    int            iWidth;
    int            iHeight;
} hgTexture;

typedef struct _hgVertexBuffer
{
    VkBuffer       tBuffer;
    VkDeviceMemory tMemory;
    size_t         szSize;
    uint32_t       uVertexCount;
} hgVertexBuffer;

typedef struct _hgIndexBuffer
{
    VkBuffer       tBuffer;
    VkDeviceMemory tMemory;
    size_t         szSize;
    uint32_t       uIndexCount;
} hgIndexBuffer;

typedef struct _hgPipeline
{
    VkPipeline       tPipeline;
    VkPipelineLayout tPipelineLayout;
} hgPipeline;

// =============================================================================
// CONFIGURATION STRUCTS - Used to configure creation functions
// =============================================================================

typedef struct _hgRenderPassConfig
{
    VkAttachmentLoadOp  tLoadOp;
    VkAttachmentStoreOp tStoreOp;
    float               afClearColor[4];
    // bool                bDepthAttachment; for future use 
} hgRenderPassConfig;

typedef struct _hgPipelineConfig
{
    // shaders
    const char* pcVertexShaderPath;
    const char* pcFragmentShaderPath;

    // vertex input
    VkVertexInputAttributeDescription* ptAttributeDescriptions;
    uint32_t                           uAttributeCount;
    uint32_t                           uVertexStride;

    // rasterization state
    VkCullModeFlags     tCullMode;   // VK_CULL_MODE_NONE, _BACK_BIT, _FRONT_BIT
    VkFrontFace         tFrontFace;  // VK_FRONT_FACE_CLOCKWISE, _COUNTER_CLOCKWISE
    VkPrimitiveTopology tTopology;   // VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, etc.

    // descriptors
    VkDescriptorSetLayout* ptDescriptorSetLayouts;
    uint32_t               uDescriptorSetLayoutCount;

    // blend -> for future use 
    VkBool32 bBlendEnable;

    // push constants -> for future use 
    VkPushConstantRange* ptPushConstantRanges;
    uint32_t             uPushConstantRangeCount;
} hgPipelineConfig;

// =============================================================================
// INTERNAL STATE - Do not access directly, use API functions
// =============================================================================

// core Vulkan context (application lifetime)
typedef struct _hgVulkanContext
{
    VkInstance       tInstance;
    VkPhysicalDevice tPhysicalDevice;
    VkDevice         tDevice;
    VkQueue          tGraphicsQueue;
    uint32_t         tGraphicsQueueFamily;
} hgVulkanContext;

// swapchain (recreated on resize)
typedef struct _hgSwapchain
{
    VkSurfaceKHR   tSurface;
    VkSwapchainKHR tSwapchain;
    VkFormat       tFormat;
    VkExtent2D     tExtent;
    VkImage*       tSwapchainImages;
    VkImageView*   tSwapchainImageViews;
    uint32_t       uSwapchainImageCount;
} hgSwapchain;

// render pipeline (tied to swapchain)
typedef struct _hgRenderPipeline
{
    VkRenderPass   tRenderPass;
    VkFramebuffer* tFramebuffers;
    float          afClearColor[4];  // stored here for convenience
} hgRenderPipeline;

// command recording tools (per-frame)
typedef struct _hgCommandResources
{
    VkCommandPool    tCommandPool;
    VkCommandBuffer* tCommandBuffers;  // one per swapchain image
    uint32_t         uCurrentImageIndex;
} hgCommandResources;

// synch objects (per-frame)
typedef struct _hgFrameSync
{
    VkSemaphore tImageAvailable;
    VkSemaphore tRenderFinished;
    VkFence     tInFlight;
} hgFrameSync;

// main application state
typedef struct _hgAppData{
    // Window
    GLFWwindow* pWindow;
    int         width;
    int         height;

    // Vulkan subsystems
    hgVulkanContext    tContextComponents;
    hgSwapchain        tSwapchainComponents;
    hgRenderPipeline   tPipelineComponents;
    hgCommandResources tCommandComponents;
    hgFrameSync        tSyncComponents;
} hgAppData;

// =============================================================================
// INITIALIZATION & SETUP (Call once at startup)
// =============================================================================

void hg_create_instance(hgAppData* ptState, const char* pcAppName, uint32_t uAppVersion, bool bEnableValidation);
void hg_create_surface(hgAppData* ptState);
void hg_pick_physical_device(hgAppData* ptState);
void hg_create_logical_device(hgAppData* ptState);
void hg_create_command_pool(hgAppData* ptState);
void hg_create_sync_objects(hgAppData* ptState);

// =============================================================================
// SWAPCHAIN & RENDER PASS (Recreate on window resize)
// =============================================================================

void hg_create_swapchain(hgAppData* ptState, VkPresentModeKHR preferredPresentMode);
void hg_create_render_pass(hgAppData* ptState, hgRenderPassConfig* config);
void hg_create_framebuffers(hgAppData* ptState);
// TODO: void hg_recreate_swapchain(hgAppData* ptState);

// =============================================================================
// RESOURCE CREATION (User manages returned handles)
// =============================================================================

// Buffers
hgVertexBuffer hg_create_vertex_buffer(hgAppData* ptState, void* data, size_t size, size_t stride);
hgIndexBuffer  hg_create_index_buffer(hgAppData* ptState, uint16_t* indices, uint32_t count);
// TODO: void hg_destroy_vertex_buffer(hgAppData* ptState, hgVertexBuffer* buffer);
// TODO: void hg_destroy_index_buffer(hgAppData* ptState, hgIndexBuffer* buffer);

// Textures
unsigned char* hg_load_texture_data(const char* filename, int* widthOut, int* heightOut);
hgTexture      hg_create_texture(hgAppData* ptState, const unsigned char* data, int width, int height);
void           hg_destroy_texture(hgAppData* ptState, hgTexture* texture);

// Pipelines
hgPipeline hg_create_graphics_pipeline(hgAppData* ptState, hgPipelineConfig* config);
void       hg_destroy_pipeline(hgAppData* ptState, hgPipeline* pipeline);

// descriptors (user manages these directly for now)
// TODO: Add descriptor helper functions if needed

// =============================================================================
// FRAME RENDERING (Call every frame)
// =============================================================================

// frame lifecycle
uint32_t hg_begin_frame(hgAppData* ptState);
// wait for fence, acquire swapchain image, reset and begin command buffer -> returns: swapchain image index to use for this frame
void hg_end_frame(hgAppData* ptState, uint32_t uImageIndex);
// end command buffer, submit to queue with semaphores, present to screen


// Render pass
void hg_begin_render_pass(hgAppData* ptState, uint32_t uImageIndex);
// begin render pass, set viewport/scissor, apply clear values
void hg_end_render_pass(hgAppData* ptState);
// end render pass (must be called before end_frame)


// bind state (must be called between begin/end render pass)
void hg_cmd_bind_pipeline(hgAppData* ptState, hgPipeline* tPipeline);
// bind graphics pipeline (shaders, rasterization state, etc.)
void hg_cmd_bind_vertex_buffer(hgAppData* ptState, hgVertexBuffer* tVertexBuffer);
// bind vertex buffer for subsequent draw calls
void hg_cmd_bind_index_buffer(hgAppData* ptState, hgIndexBuffer* tIndexBuffer);
// bind index buffer for subsequent indexed draw calls
void hg_cmd_bind_descriptor_sets(hgAppData* ptState, hgPipeline* tPipeline, VkDescriptorSet* tDescriptorSets, uint32_t uSetCount);
// bind descriptor sets (textures, uniforms) for shaders


// Draw commands (must be called between begin/end render pass)
void hg_cmd_draw(hgAppData* ptState, uint32_t uVertexCount, uint32_t uFirstVertex);
// draw non-indexed geometry using currently bound vertex buffer
void hg_cmd_draw_indexed(hgAppData* ptState, uint32_t uIndexCount, uint32_t uFirstIndex, int32_t iVertexOffset);
// draw indexed geometry using currently bound vertex and index buffers

// convenience wrappers (optional - combines bind + draw)
void hg_draw_mesh(hgAppData* ptState, hgVertexBuffer* tVertexBuffer, hgIndexBuffer* tIndexBuffer, uint32_t uIndexCount);
// convenience function - binds vertex/index buffers and draws in one call

// =============================================================================
// CLEANUP (Call at shutdown) -> will be replaced with more granular functions 
// =============================================================================

void hg_cleanup(hgAppData* ptState);

// =============================================================================
// INTERNAL/ADVANCED - Most users won't need these
// =============================================================================

// low-level buffer operations
uint32_t hg_find_memory_type(hgVulkanContext* context, uint32_t typeFilter, VkMemoryPropertyFlags properties);
void     hg_create_buffer(hgVulkanContext* context, VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer* buffer, VkDeviceMemory* memory);
void     hg_copy_buffer(hgVulkanContext* context, hgCommandResources* commands, VkBuffer src, VkBuffer dst, VkDeviceSize size);

// one-time command helpers
VkCommandBuffer hg_begin_single_time_commands(hgAppData* ptState);
void            hg_end_single_time_commands(hgAppData* ptState, VkCommandBuffer cmdBuffer);

// image operations
void hg_transition_image_layout(VkCommandBuffer cmdBuffer, VkImage image, VkImageLayout oldLayout, VkImageLayout newLayout, VkImageSubresourceRange subresourceRange, VkPipelineStageFlags srcStage, VkPipelineStageFlags dstStage);
void hg_upload_to_image(hgAppData* ptState, VkImage image, const unsigned char* data, int width, int height);

// shader loading
VkShaderModule hg_create_shader_module(hgAppData* ptState, const char* filename);

// command buffer access (for advanced users)
VkCommandBuffer hg_get_current_command_buffer(hgAppData* ptState);

#endif // VKHOMEGROWN_H