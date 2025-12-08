// =============================================================================
// TABLE OF CONTENTS
// =============================================================================
/*
    -> defines and macros
    -> structs
    ->function declarations
        - Window & Platform Management
        - Core Vulkan Context Initialization
        - Swapchain Management
        - Render Pipeline (RenderPass + Pipeline)
        - Command Buffer Management
        - Synchronization Objects
        - Frame Rendering Loop
        - Resource Creation & Management
        - Shader Management
        - Cleanup & Resource Destruction
        - Helpers
    -> shaders
*/
// =============================================================================


#ifndef VKHOMEGROWN_H
#define VKHOMEGROWN_H


#include <vulkan/vulkan.h>
#include <GLFW/glfw3.h>
#include <stdlib.h>
#include <stdbool.h>


#define VULKAN_CHECK(result) if((result) != VK_SUCCESS) { \
    printf("vulkan error at %s:%d: %d\n", __FILE__, __LINE__, (result)); \
    exit(1); \
}


// -----------------------------------------------------------------------------
// structs 
// -----------------------------------------------------------------------------

// core vulkan device and queues 
typedef struct _hgVulkanContext{
    VkInstance       tInstance;
    VkPhysicalDevice tPhysicalDevice;
    VkDevice         tDevice;
    VkQueue          tGraphicsQueue;
    uint32_t         tGraphicsQueueFamily;

    // device properties
    VkPhysicalDeviceProperties       tDeviceProperties;
    VkPhysicalDeviceMemoryProperties tMemoryProperties;
} hgVulkanContext;

// window surface and swapchain (recreated on resize)
typedef struct _hgSwapchain
{
    VkSurfaceKHR   tSurface;
    VkSwapchainKHR tSwapchain;
    VkFormat       tFormat;
    VkExtent2D     tExtent;

    VkImage*     tSwapchainImages;
    VkImageView* tSwapchainImageViews;
    uint32_t     uSwapchainImageCount;
    uint32_t     uSwapchainCurrentImageIndex;
} hgSwapchain;

// rendering pipeline and render pass (tied to swapchain format)
typedef struct _hgRenderPipeline
{
    VkRenderPass     tRenderPass;
    VkPipelineLayout tPipelineLayout; // TODO: remove from struct - handled on an as needed basis by hgPipeline instead of globally
    VkPipeline       tPipeline;       // same as above
    VkFramebuffer*   tFramebuffers;
} hgRenderPipeline;

// command recording resources
typedef struct _hgCommandResources
{
    VkCommandPool    tCommandPool;
    VkCommandBuffer* tCommandBuffers;
} hgCommandResources;

// per-frame synchronization 
typedef struct _hgFrameSync
{
    VkSemaphore tImageAvailable;
    VkSemaphore tRenderFinished;
    VkFence     tInFlight;
} hgFrameSync;

// game-specific rendering resources
typedef struct _hgRenderResources
{
    // VkBuffer       tVertexBuffer;  -> to be managed by api user
    // VkDeviceMemory tVertexBufferMemory;
    // size_t         szVertexBufferSize;
    // uint32_t       uVertexCount;

    // VkBuffer       tIndexBuffer;
    // VkDeviceMemory tIndexBufferMemory;
    // size_t         szIndexBufferSize;
    // uint32_t       uIndexCount;

    // descriptor sets for textures/uniforms
    VkDescriptorPool      tDescriptorPool;
    VkDescriptorSetLayout tDescriptorSetLayout;
    VkDescriptorSet       tDescriptorSets[16]; // TODO: need to figure out how to handle this
} hgRenderResources;

// application state
typedef struct _hgAppData{
    // Windows stuff
    GLFWwindow* pWindow;
    int width;
    int height;

    // Vulkan subsystems
    hgVulkanContext    tContextComponents;
    hgSwapchain        tSwapchainComponents;
    hgRenderPipeline   tPipelineComponents;
    hgCommandResources tCommandComponents;
    hgFrameSync        tSyncComponents;
    hgRenderResources  tResources;
    float              afClearColor[4];
} hgAppData;

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


// TODO: do you really want to go down this road or just leave descriptors upto api user?
typedef struct _hgDescriptorPoolConfig 
{
    uint32_t                    uMaxSets;      // maximum number of descriptor sets
    uint32_t                    uNumPoolSizes; // number of different descriptor types
    VkDescriptorPoolSize*       pPoolSizes;    // array of descriptor type counts
    VkDescriptorPoolCreateFlags tFlags;        // creation flags
} hgDescriptorPoolConfig;

typedef struct _hgDescriptorLayoutConfig 
{
    uint32_t                      uNumBindings; // number of bindings in the layout
    VkDescriptorSetLayoutBinding* pBindings;    // array of bindings
} hgDescriptorLayoutConfig;

typedef struct _hgDescriptorAllocConfig 
{
    uint32_t               uNumSets;    // number of sets to allocate
    VkDescriptorSetLayout* pLayouts;    // array of layouts (one per set)
} hgDescriptorAllocConfig;

typedef struct _hgRenderPassConfig
{
    bool                bDepthAttatchment; // for future use
    VkAttachmentLoadOp  tLoadOp;
    VkAttachmentStoreOp tStoreOp;
    float               afClearColor[4];
} hgRenderPassConfig;

typedef struct _hgPipelineConfig
{
    // shader paths
    const char* pcVertexShaderPath;
    const char* pcFragmentShaderPath;

    // vertex input description
    VkVertexInputAttributeDescription* ptAttributeDescriptions;
    uint32_t                           uAttributeCount;
    uint32_t                           uVertexStride;

    // rasterization
    VkCullModeFlags tCullMode;  // VK_CULL_MODE_BACK_BIT, VK_CULL_MODE_FRONT_BIT, VK_CULL_MODE_NONE
    VkFrontFace     tFrontFace; // VK_FRONT_FACE_CLOCKWISE or VK_FRONT_FACE_COUNTER_CLOCKWISE

    // topology
    VkPrimitiveTopology tTopology; // VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, VK_PRIMITIVE_TOPOLOGY_LINE_LIST, etc.

    // descriptor set layouts
    VkDescriptorSetLayout* ptDescriptorSetLayouts;
    uint32_t               uDescriptorSetLayoutCount;

    // not currently used -> for future addition
    // push constants
    VkPushConstantRange* ptPushConstantRanges;
    uint32_t             uPushConstantRangeCount;
    // blend mode
    VkBool32 bBlendEnable;

} hgPipelineConfig;

typedef struct _hgPipeline
{
    VkPipeline       tPipeline;
    VkPipelineLayout tPipelineLayout;
} hgPipeline;

// -----------------------------------------------------------------------------
// function declarations
// -----------------------------------------------------------------------------

// TODO: order these in a more logical way i.e. group tasks and handle creation etc...

// -----------------------------------------------------------------------------
// function declarations 
// -----------------------------------------------------------------------------

    // Window & Platform Management

    // Core Vulkan Context Initialization
    void hg_create_instance(hgAppData* ptState, const char* pcAppName, uint32_t uAppVersion, bool bEnableValidation);
    void hg_create_surface(hgAppData* ptState);
    void hg_pick_physical_device(hgAppData* ptState);
    void hg_create_logical_device(hgAppData* ptState);

    // Swapchain Management
    void hg_create_swapchain(hgAppData* ptState, VkPresentModeKHR preferredPresentMode);
    // void hg_recreate_swapchain(hgAppData* ptState);  // For resize support

    // Render Pipeline (RenderPass + Pipeline)
    void       hg_create_render_pass(hgAppData* ptState, hgRenderPassConfig* tConfig);
    hgPipeline hg_create_graphics_pipeline(hgAppData* ptState, hgPipelineConfig* config);
    void       hg_create_framebuffers(hgAppData* ptState);

    // Command Buffer Management
    void hg_create_command_pool(hgAppData* ptState);
    // void hg_recreate_command_buffers(hgAppData* ptState);  // For swapchain recreation

    // Synchronization Objects
    void hg_create_sync_objects(hgAppData* ptState);

    // Frame Rendering Loop
    void hg_draw_frame(hgAppData* ptState);
    // void hg_wait_device_idle(hgAppData* ptState);  // For cleanup/synchronization

    // Resource Creation & Management
    // Buffers & Memory
    uint32_t hg_find_memory_type(hgVulkanContext* ptContextComponents, uint32_t uTypeFilter, VkMemoryPropertyFlags tProperties);
    void     hg_create_buffer(hgVulkanContext* ptContextComponents, VkDeviceSize tSize, VkBufferUsageFlags tUsage, VkMemoryPropertyFlags tProperties, VkBuffer* pBuffer, VkDeviceMemory* pMemory);
    void     hg_copy_buffer(hgVulkanContext* ptContextComponents, hgCommandResources* ptCommandComponents, VkBuffer tSrcBuffer, VkBuffer tDstBuffer, VkDeviceSize tSize);
    // Vertex/Index Buffer Creation
    hgVertexBuffer hg_create_vertex_buffer(hgAppData* ptState, void* data, size_t size, size_t stride);
    hgIndexBuffer  hg_create_index_buffer(hgAppData* ptState, uint16_t* indices, uint32_t count);
    // Descriptor Management
    void hg_create_descriptor_set(hgAppData* ptState); // TODO: do you really want to go down this road of descriptor managment or let api user manage them?
    // VkDescriptorSetLayoutBinding hg_create_binding(uint32_t uBinding, VkDescriptorType tType, VkShaderStageFlags tStageFlags, uint32_t uDescriptorCount);
    // void hg_create_descriptor_pool_from_config(hgAppData* ptState, const hgDescriptorPoolConfig* ptConfig);
    // void hg_create_descriptor_set_layout_from_config(hgAppData* ptState, const hgDescriptorLayoutConfig* ptConfig, VkDescriptorSetLayout* ptLayout);
    // void hg_allocate_descriptor_sets_from_config(hgAppData* ptState,const hgDescriptorAllocConfig* ptConfig, VkDescriptorSet* ptDescriptorSets);

    // Textures
    unsigned char* hg_load_texture_data(const char* pcFileName, int* iWidthOut, int* iHeightOut);
    hgTexture      hg_create_texture(hgAppData* ptState, const unsigned char* pucData, int iWidth, int iHeight);
    void           hg_destroy_texture(hgAppData* ptState, hgTexture* tTexture);

// -------------------------------
// Shader Management
// -------------------------------
    VkShaderModule hg_create_shader_module(hgAppData* ptState, const char* pcFilename);

// -------------------------------
// Cleanup & Resource Destruction
// -------------------------------
    void hg_cleanup(hgAppData* ptState); // TODO: come up with better clean up system -- possibly break up into sub system cleanups
    // void hg_cleanup_swapchain(hgAppData* ptState);  // For dynamic recreation
    // void hg_cleanup_pipeline(hgAppData* ptState);
    // void hg_cleanup_buffers(hgAppData* ptState);
    void hg_destroy_pipeline(hgAppData* ptState, hgPipeline* pipeline);

// -----------------------------------------------------------------------------
// Helpers
// -----------------------------------------------------------------------------
    VkCommandBuffer hg_begin_single_time_commands(hgAppData* ptState);
    void            hg_end_single_time_commands(hgAppData* ptState, VkCommandBuffer tCommandBuffer);
    void            hg_transition_image_layout(VkCommandBuffer tCommandBuffer, VkImage tImage, VkImageLayout tOldLayout, VkImageLayout tNewLayout, VkImageSubresourceRange tSubresourceRange, VkPipelineStageFlags tSrcStageMask, VkPipelineStageFlags tDstStageMask);
    void            hg_upload_to_image(hgAppData* ptState, VkImage tImage, const unsigned char* pData, int iWidth, int iHeight);

// -----------------------------------------------------------------------------
// shaders
// -----------------------------------------------------------------------------
    VkShaderModule hg_create_shader_module(hgAppData* ptState, const char* pcFilename);

#endif