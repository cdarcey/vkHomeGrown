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
// TABLE OF CONTENTS
// =============================================================================
/*
    -> [SECTION] MACROS
    -> [SECTION] CORE TYPES
    -> [SECTION] CONFIGURATION STRUCTS
    -> [SECTION] INTERNAL STATE
    -> [SECTION] INITIALIZATION & SETUP
    -> [SECTION] SWAPCHAIN & RENDER PASS
    -> [SECTION] RESOURCE CREATION
    -> [SECTION] FRAME RENDERING
    -> [SECTION] CLEANUP
*/

// =============================================================================
// MACROS
// =============================================================================

#define VULKAN_CHECK(result) if((result) != VK_SUCCESS) { \
    printf("vulkan error at %s:%d: %d\n", __FILE__, __LINE__, (result)); \
    exit(1); \
}

// =============================================================================
// CORE TYPES
// =============================================================================


typedef struct _hgVertex
{
    float x, y, z;    // position
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

// Add to header
typedef struct _hgUniformBuffer
{
    VkBuffer       tBuffer;
    VkDeviceMemory tMemory;
    void*          pMapped;  // keep mapped for updates
    size_t         szSize;
} hgUniformBuffer;

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
    VkPipeline             tPipeline;
    VkPipelineLayout       tPipelineLayout;
    VkPipelineBindPoint    tPipelineBindPoint; // will always be VK_PIPELINE_BIND_POINT_GRAPHICS right now but leaving the door open to compute, raytracing, etc..
} hgPipeline;

// =============================================================================
// CONFIGURATION STRUCTS
// =============================================================================

typedef struct _hgRenderPassConfig
{
    VkAttachmentLoadOp  tLoadOp;
    VkAttachmentStoreOp tStoreOp;
    float               afClearColor[4];
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

    // descriptors
    VkDescriptorSetLayout* ptDescriptorSetLayouts;
    uint32_t               uDescriptorSetLayoutCount;

    VkPipelineBindPoint    tPipelineBindPoint; // passthrough to hgPipeline

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

    // depth attachments 
    VkImage        tDepthImage;
    VkDeviceMemory tDepthMemory;
    VkImageView    tDepthImageView;
    VkFormat       tDepthFormat;  // store the format we choose

    float          afClearColor[4];   // stored here for convenience
    float          afStencilClear[2];
} hgRenderPipeline;

// command recording tools
typedef struct _hgCommandResources
{
    VkCommandPool    tCommandPool;
    VkCommandBuffer* tCommandBuffers;  // one per swapchain image
    uint32_t         uCurrentImageIndex;
} hgCommandResources;

// synch objects
typedef struct _hgFrameSync
{
    VkSemaphore tImageAvailable;
    VkSemaphore tRenderFinished;
    VkFence     tInFlight;
} hgFrameSync;

// main application state
typedef struct _hgAppData
{
    // window
    GLFWwindow* pWindow;
    int         width;
    int         height;

    // vulkan subsystems
    hgVulkanContext    tContextComponents;
    hgSwapchain        tSwapchainComponents;
    hgRenderPipeline   tPipelineComponents;
    hgCommandResources tCommandComponents;
    hgFrameSync        tSyncComponents;

    // settings
    bool bDepthEnabled; // should be set on intialization 
} hgAppData;

// =============================================================================
// INITIALIZATION & SETUP
// =============================================================================

void hg_create_instance(hgAppData* ptState, const char* pcAppName, uint32_t uAppVersion, bool bEnableValidation);
void hg_create_surface(hgAppData* ptState);
void hg_pick_physical_device(hgAppData* ptState);
void hg_create_logical_device(hgAppData* ptState);
void hg_create_command_pool(hgAppData* ptState);
void hg_create_sync_objects(hgAppData* ptState);

// specific to frame command buffers only -> used on swapchain recreation as well
void hg_allocate_frame_cmd_buffers(hgAppData* ptState);

// =============================================================================
// SWAPCHAIN & RENDER PASS
// =============================================================================

void hg_create_swapchain(hgAppData* ptState, VkPresentModeKHR preferredPresentMode);
void hg_create_render_pass(hgAppData* ptState, hgRenderPassConfig* config);
void hg_create_framebuffers(hgAppData* ptState);
void hg_recreate_swapchain(hgAppData* ptState);
void hg_create_depth_resources(hgAppData* ptState);

// =============================================================================
// RESOURCE CREATION
// =============================================================================

// buffers
hgVertexBuffer hg_create_vertex_buffer(hgAppData* ptState, void* data, size_t size, size_t stride);
hgIndexBuffer  hg_create_index_buffer(hgAppData* ptState, uint16_t* indices, uint32_t count);

// textures
unsigned char* hg_load_texture_data(const char* filename, int* widthOut, int* heightOut);
hgTexture      hg_create_texture(hgAppData* ptState, const unsigned char* data, int width, int height);

// pipelines
hgPipeline hg_create_graphics_pipeline(hgAppData* ptState, hgPipelineConfig* config);

// descriptors 
VkDescriptorPool hg_create_descriptor_pool(hgAppData* ptState, uint32_t uMaxSets, VkDescriptorPoolSize* atPoolSizes, uint32_t uPoolSizeCount);
void             hg_update_texture_descriptor(hgAppData* ptState, VkDescriptorSet tDescriptorSet, uint32_t uBinding, hgTexture* tTexture, VkSampler tSampler);

// uniform buffers
hgUniformBuffer hg_create_uniform_buffer(hgAppData* ptState, size_t size);
void            hg_update_uniform_buffer(hgAppData* ptState, hgUniformBuffer* buffer, void* data, size_t size);
void            hg_destroy_uniform_buffer(hgAppData* ptState, hgUniformBuffer* buffer);

// =============================================================================
// FRAME RENDERING
// =============================================================================

// frame lifecycle
uint32_t hg_begin_frame(hgAppData* ptState);
void     hg_end_frame(hgAppData* ptState, uint32_t uImageIndex);


// Render pass
void hg_begin_render_pass(hgAppData* ptState, uint32_t uImageIndex);
void hg_end_render_pass(hgAppData* ptState);


// bind state (must be called between begin/end render pass)
// may not do these functions as it doesnt really make sense to stash away the vulkan code on these
void hg_cmd_bind_pipeline(hgAppData* ptState, hgPipeline* tPipeline);
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

// convenience wrappers -> not sure if this is worth it or not yet
void hg_draw_mesh(hgAppData* ptState, hgVertexBuffer* tVertexBuffer, hgIndexBuffer* tIndexBuffer, uint32_t uIndexCount);
// convenience function - binds vertex/index buffers and draws in one call

// =============================================================================
// CLEANUP
// =============================================================================

void hg_core_cleanup(hgAppData* ptState); // should be called after all other cleanup
void hg_cleanup_swapchain_resources(hgAppData* ptState); // for swapchain rebuilding on window resize
void hg_destroy_texture(hgAppData* ptState, hgTexture* texture);
void hg_destroy_vertex_buffer(hgAppData* ptState, hgVertexBuffer* tVertexBuffer);
void hg_destroy_index_buffer(hgAppData* ptState, hgIndexBuffer* tIndexBuffer);
void hg_destroy_pipeline(hgAppData* ptState, hgPipeline* tPipeline);

#endif // VKHOMEGROWN_H