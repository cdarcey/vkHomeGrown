// =============================================================================
// TABLE OF CONTENTS
// =============================================================================
/*
    -> [SECTION] INTERNAL API DECLARATIONS
    -> [SECTION] INITIALIZATION & SETUP
    -> [SECTION] SWAPCHAIN & RENDER PASS
    -> [SECTION] RESOURCE CREATION
    -> [SECTION] FRAME RENDERING
    -> [SECTION] CLEANUP
    -> [SECTION] INTERNAL HELPERS
*/

#include "vkHomeGrown.h"
#include <assert.h>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

// =============================================================================
// INTERNAL API DECLARATIONS
// =============================================================================

// low level buffer operations
uint32_t hg_find_memory_type(hgVulkanContext* context, uint32_t typeFilter, VkMemoryPropertyFlags properties);
void     hg_create_buffer(hgVulkanContext* context, VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer* buffer, VkDeviceMemory* memory);
void     hg_copy_buffer(hgVulkanContext* context, hgCommandResources* commands, VkBuffer src, VkBuffer dst, VkDeviceSize size);

// one time command helpers
VkCommandBuffer hg_begin_single_time_commands(hgAppData* ptState);
void            hg_end_single_time_commands(hgAppData* ptState, VkCommandBuffer cmdBuffer);

// image operations
void hg_transition_image_layout(VkCommandBuffer cmdBuffer, VkImage image, VkImageLayout oldLayout, VkImageLayout newLayout, VkImageSubresourceRange subresourceRange, VkPipelineStageFlags srcStage, VkPipelineStageFlags dstStage);
void hg_upload_to_image(hgAppData* ptState, VkImage image, const unsigned char* data, int width, int height);

// shader loading
VkShaderModule hg_create_shader_module(hgAppData* ptState, const char* filename);

// command buffer access
VkCommandBuffer hg_get_current_frame_cmd_buffer(hgAppData* ptState);

// =============================================================================
// INITIALIZATION & SETUP (Call once at startup)
// =============================================================================

void
hg_create_instance(hgAppData* ptAppData, const char* pcAppName, uint32_t uAppVersion, bool bEnableValidation)
{
    VkApplicationInfo tAppInfo = {
        .sType              = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pApplicationName   = pcAppName,
        .applicationVersion = uAppVersion,
        .pEngineName        = "HomeGrown Engine",
        .engineVersion      = VK_MAKE_VERSION(1, 0, 0),
        .apiVersion         = VK_API_VERSION_1_0,
    };

    // get required extensions from GLFW
    uint32_t glfwExtensionCount = 0;
    const char** glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

    VkInstanceCreateInfo tCreateInfo = {
        .sType                   = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pApplicationInfo        = &tAppInfo,
        .enabledExtensionCount   = glfwExtensionCount,  // Use GLFW's count
        .ppEnabledExtensionNames = glfwExtensions       // Use GLFW's extensions
    };

    // TODO: figure this system out 
    // optional: Add validation layers if needed 
    if(bEnableValidation)
    {
        const char* validationLayers[] = {"VK_LAYER_KHRONOS_validation"};
        tCreateInfo.enabledLayerCount = 1;
        tCreateInfo.ppEnabledLayerNames = validationLayers;
    }

    VULKAN_CHECK(vkCreateInstance(&tCreateInfo, NULL, &ptAppData->tContextComponents.tInstance));
}

void
hg_create_surface(hgAppData* ptAppData) 
{
    // GLFW handles platform specific surface creation
    VULKAN_CHECK(glfwCreateWindowSurface(
        ptAppData->tContextComponents.tInstance,
        ptAppData->pWindow,
        NULL,
        &ptAppData->tSwapchainComponents.tSurface
    ));
}

void 
hg_pick_physical_device(hgAppData* ptAppData) 
{
    uint32_t uDeviceCount = 0;
    vkEnumeratePhysicalDevices(ptAppData->tContextComponents.tInstance, &uDeviceCount, NULL);
    assert(uDeviceCount > 0);

    VkPhysicalDevice* ptDevices = malloc(uDeviceCount * sizeof(VkPhysicalDevice));
    vkEnumeratePhysicalDevices(ptAppData->tContextComponents.tInstance, &uDeviceCount, ptDevices);

    // just pick the first gpu
    ptAppData->tContextComponents.tPhysicalDevice = ptDevices[0];

    free(ptDevices);
}

void 
hg_create_logical_device(hgAppData* ptAppData) 
{
    // find queue family that supports graphics and presentation
    uint32_t uQueueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(ptAppData->tContextComponents.tPhysicalDevice, &uQueueFamilyCount, NULL);
    VkQueueFamilyProperties* pQueueFamilies = malloc(uQueueFamilyCount * sizeof(VkQueueFamilyProperties));
    vkGetPhysicalDeviceQueueFamilyProperties(ptAppData->tContextComponents.tPhysicalDevice, &uQueueFamilyCount, pQueueFamilies);

    // find graphics queue family
    ptAppData->tContextComponents.tGraphicsQueueFamily = UINT32_MAX;
    for(uint32_t i = 0; i < uQueueFamilyCount; i++) 
    {
        if(pQueueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) 
        {
            VkBool32 bPresentSupport = VK_FALSE;
            vkGetPhysicalDeviceSurfaceSupportKHR(ptAppData->tContextComponents.tPhysicalDevice, i, ptAppData->tSwapchainComponents.tSurface, &bPresentSupport);
            if(bPresentSupport) 
            {
                ptAppData->tContextComponents.tGraphicsQueueFamily = i;
                break;
            }
        }
    }
    assert(ptAppData->tContextComponents.tGraphicsQueueFamily != UINT32_MAX);

    free(pQueueFamilies);

    float fQueuePriority = 1.0f;
    VkDeviceQueueCreateInfo tQueueCreateInfo = {
        .sType            = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
        .queueFamilyIndex = ptAppData->tContextComponents.tGraphicsQueueFamily,
        .queueCount       = 1,
        .pQueuePriorities = &fQueuePriority
    };

    const char* deviceExtensions[] = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};
    VkDeviceCreateInfo tDeviceCreateInfo = {
        .sType                   = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .queueCreateInfoCount    = 1,
        .pQueueCreateInfos       = &tQueueCreateInfo,
        .enabledExtensionCount   = 1,
        .ppEnabledExtensionNames = deviceExtensions
    };

    VULKAN_CHECK(vkCreateDevice(ptAppData->tContextComponents.tPhysicalDevice, &tDeviceCreateInfo, NULL, &ptAppData->tContextComponents.tDevice));
    vkGetDeviceQueue(ptAppData->tContextComponents.tDevice, ptAppData->tContextComponents.tGraphicsQueueFamily, 0, &ptAppData->tContextComponents.tGraphicsQueue);
}

void 
hg_create_command_pool(hgAppData* ptAppData) 
{
    VkCommandPoolCreateInfo tPoolInfo = {
        .sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .flags            = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
        .queueFamilyIndex = ptAppData->tContextComponents.tGraphicsQueueFamily
    };

    VULKAN_CHECK(vkCreateCommandPool(ptAppData->tContextComponents.tDevice, &tPoolInfo, NULL, &ptAppData->tCommandComponents.tCommandPool));
}

void 
hg_create_sync_objects(hgAppData* ptAppData) 
{
    VkSemaphoreCreateInfo tSemaphoreInfo = {
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO
    };

    VkFenceCreateInfo tFenceInfo = {
        .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
        .flags = VK_FENCE_CREATE_SIGNALED_BIT
    };

    VULKAN_CHECK(vkCreateSemaphore(ptAppData->tContextComponents.tDevice, &tSemaphoreInfo, NULL, &ptAppData->tSyncComponents.tImageAvailable));
    VULKAN_CHECK(vkCreateSemaphore(ptAppData->tContextComponents.tDevice, &tSemaphoreInfo, NULL, &ptAppData->tSyncComponents.tRenderFinished));
    VULKAN_CHECK(vkCreateFence(ptAppData->tContextComponents.tDevice, &tFenceInfo, NULL, &ptAppData->tSyncComponents.tInFlight));
}

// specific to frame command buffers only -> used on swapchain recreation as well
void 
hg_allocate_frame_cmd_buffers(hgAppData* ptState)
{
    ptState->tCommandComponents.tCommandBuffers = malloc(ptState->tSwapchainComponents.uSwapchainImageCount * sizeof(VkCommandBuffer));

    VkCommandBufferAllocateInfo tAllocInfo = {
        .sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool        = ptState->tCommandComponents.tCommandPool,
        .level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = ptState->tSwapchainComponents.uSwapchainImageCount
    };
    VULKAN_CHECK(vkAllocateCommandBuffers(ptState->tContextComponents.tDevice, &tAllocInfo, ptState->tCommandComponents.tCommandBuffers));
}

// =============================================================================
// SWAPCHAIN & RENDER PASS (recreate on window resize)
// =============================================================================

void
hg_create_swapchain(hgAppData* ptAppData, VkPresentModeKHR tPreferredPresentMode)
{
    // get surface capabilities
    VkSurfaceCapabilitiesKHR tCapabilities;
    VULKAN_CHECK(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(ptAppData->tContextComponents.tPhysicalDevice, ptAppData->tSwapchainComponents.tSurface, &tCapabilities));

    // get surface formats
    uint32_t uFormatCount;
    VULKAN_CHECK(vkGetPhysicalDeviceSurfaceFormatsKHR(ptAppData->tContextComponents.tPhysicalDevice, ptAppData->tSwapchainComponents.tSurface, &uFormatCount, NULL));
    VkSurfaceFormatKHR* pFormats = malloc(uFormatCount * sizeof(VkSurfaceFormatKHR));
    VULKAN_CHECK(vkGetPhysicalDeviceSurfaceFormatsKHR(ptAppData->tContextComponents.tPhysicalDevice, ptAppData->tSwapchainComponents.tSurface, &uFormatCount, pFormats));

    // choose surface format (prefer B8G8R8A8_UNORM with sRGB)
    ptAppData->tSwapchainComponents.tFormat = VK_FORMAT_B8G8R8A8_UNORM;
    for(uint32_t i = 0; i < uFormatCount; i++) 
    {
        if(pFormats[i].format == VK_FORMAT_B8G8R8A8_UNORM && pFormats[i].colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) 
        {
            ptAppData->tSwapchainComponents.tFormat = pFormats[i].format;
            break;
        }
    }

    // choose swap extent
    if(tCapabilities.currentExtent.width != UINT32_MAX) 
    {
        ptAppData->tSwapchainComponents.tExtent = tCapabilities.currentExtent;
    } 
    else 
    {
        VkExtent2D tActualExtent = {ptAppData->width, ptAppData->height};
        tActualExtent.width = (tActualExtent.width < tCapabilities.minImageExtent.width) ? 
            tCapabilities.minImageExtent.width : (tActualExtent.width > tCapabilities.maxImageExtent.width) ? 
            tCapabilities.maxImageExtent.width : tActualExtent.width;
        tActualExtent.height = (tActualExtent.height < tCapabilities.minImageExtent.height) ? 
            tCapabilities.minImageExtent.height : (tActualExtent.height > tCapabilities.maxImageExtent.height) ? 
            tCapabilities.maxImageExtent.height : tActualExtent.height;
        ptAppData->tSwapchainComponents.tExtent = tActualExtent;
    }

    // choose image count
    uint32_t uImageCount = tCapabilities.minImageCount + 1;
    if(tCapabilities.maxImageCount > 0 && uImageCount > tCapabilities.maxImageCount) 
    {
        uImageCount = tCapabilities.maxImageCount;
    }

    // see if requested mode is available
    uint32_t uPresentModeCount;
    vkGetPhysicalDeviceSurfacePresentModesKHR(ptAppData->tContextComponents.tPhysicalDevice, 
        ptAppData->tSwapchainComponents.tSurface, &uPresentModeCount, NULL);

    VkPresentModeKHR pPresentModes[8];  // 8 is more than enough
    vkGetPhysicalDeviceSurfacePresentModesKHR(ptAppData->tContextComponents.tPhysicalDevice, 
        ptAppData->tSwapchainComponents.tSurface, &uPresentModeCount, pPresentModes);

    // FIFO as the default
    VkPresentModeKHR tSelectedPresentMode = VK_PRESENT_MODE_FIFO_KHR;
    for(uint32_t i = 0; i < uPresentModeCount; i++) 
    {
        if(pPresentModes[i] == tPreferredPresentMode) 
        {
            tSelectedPresentMode = tPreferredPresentMode;
            break;
        }
    }

    // create swapchain
    VkSwapchainCreateInfoKHR tCreateInfo = {
        .sType            = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
        .surface          = ptAppData->tSwapchainComponents.tSurface,
        .minImageCount    = uImageCount,
        .imageFormat      = ptAppData->tSwapchainComponents.tFormat,
        .imageColorSpace  = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR,
        .imageExtent      = ptAppData->tSwapchainComponents.tExtent,
        .imageArrayLayers = 1,
        .imageUsage       = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
        .imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .preTransform     = tCapabilities.currentTransform,
        .compositeAlpha   = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
        .presentMode      = tSelectedPresentMode,
        .clipped          = VK_TRUE
    };
    VULKAN_CHECK(vkCreateSwapchainKHR(ptAppData->tContextComponents.tDevice, &tCreateInfo, NULL, &ptAppData->tSwapchainComponents.tSwapchain));

    // get swapchain images
    VULKAN_CHECK(vkGetSwapchainImagesKHR(ptAppData->tContextComponents.tDevice, ptAppData->tSwapchainComponents.tSwapchain, &ptAppData->tSwapchainComponents.uSwapchainImageCount, NULL));
    ptAppData->tSwapchainComponents.tSwapchainImages = malloc(ptAppData->tSwapchainComponents.uSwapchainImageCount * sizeof(VkImage));
    VULKAN_CHECK(vkGetSwapchainImagesKHR(ptAppData->tContextComponents.tDevice, ptAppData->tSwapchainComponents.tSwapchain, &ptAppData->tSwapchainComponents.uSwapchainImageCount, ptAppData->tSwapchainComponents.tSwapchainImages));

    // create image views
    ptAppData->tSwapchainComponents.tSwapchainImageViews = malloc(ptAppData->tSwapchainComponents.uSwapchainImageCount * sizeof(VkImageView));
    for(uint32_t i = 0; i < ptAppData->tSwapchainComponents.uSwapchainImageCount; i++) 
    {
        VkImageViewCreateInfo tViewCreateInfo = {
            .sType      = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .image      = ptAppData->tSwapchainComponents.tSwapchainImages[i],
            .viewType   = VK_IMAGE_VIEW_TYPE_2D,
            .format     = ptAppData->tSwapchainComponents.tFormat,
            .components = {
                .r = VK_COMPONENT_SWIZZLE_IDENTITY,
                .g = VK_COMPONENT_SWIZZLE_IDENTITY,
                .b = VK_COMPONENT_SWIZZLE_IDENTITY,
                .a = VK_COMPONENT_SWIZZLE_IDENTITY
            },
            .subresourceRange = {
                .aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT,
                .baseMipLevel   = 0,
                .levelCount     = 1,
                .baseArrayLayer = 0,
                .layerCount     = 1
            }
        };
        VULKAN_CHECK(vkCreateImageView(ptAppData->tContextComponents.tDevice, &tViewCreateInfo, NULL, &ptAppData->tSwapchainComponents.tSwapchainImageViews[i]));
    }
    free(pFormats);
}

void 
hg_create_render_pass(hgAppData* ptAppData, hgRenderPassConfig* ptConfig)
{
    // set clear color from config
    memcpy(ptAppData->tPipelineComponents.afClearColor, &ptConfig->afClearColor, sizeof(float) * 4);

    VkAttachmentDescription tColorAttachment = {
        .format         = ptAppData->tSwapchainComponents.tFormat,
        .samples        = VK_SAMPLE_COUNT_1_BIT,
        .loadOp         = ptConfig->tLoadOp,
        .storeOp        = ptConfig->tStoreOp,
        .stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
        .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
        .initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED,
        .finalLayout    = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR
    };

    VkAttachmentReference tColorAttachmentRef = {
        .attachment = 0,
        .layout     = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
    };

    VkSubpassDescription tSubpass = {
        .pipelineBindPoint    = VK_PIPELINE_BIND_POINT_GRAPHICS,
        .colorAttachmentCount = 1,
        .pColorAttachments    = &tColorAttachmentRef
    };

    VkRenderPassCreateInfo tRenderPassInfo = {
        .sType           = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
        .attachmentCount = 1,
        .pAttachments    = &tColorAttachment,
        .subpassCount    = 1,
        .pSubpasses      = &tSubpass
    };

    VULKAN_CHECK(vkCreateRenderPass(ptAppData->tContextComponents.tDevice, &tRenderPassInfo, NULL, &ptAppData->tPipelineComponents.tRenderPass));
}

void 
hg_create_framebuffers(hgAppData* ptAppData) 
{
    ptAppData->tPipelineComponents.tFramebuffers = malloc(ptAppData->tSwapchainComponents.uSwapchainImageCount * sizeof(VkFramebuffer));

    for(uint32_t i = 0; i < ptAppData->tSwapchainComponents.uSwapchainImageCount; i++) 
    {
        VkImageView attachments[] = {ptAppData->tSwapchainComponents.tSwapchainImageViews[i]};

        VkFramebufferCreateInfo tFramebufferInfo = {
            .sType           = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
            .renderPass      = ptAppData->tPipelineComponents.tRenderPass,
            .attachmentCount = 1,
            .pAttachments    = attachments,
            .width           = ptAppData->tSwapchainComponents.tExtent.width,
            .height          = ptAppData->tSwapchainComponents.tExtent.height,
            .layers          = 1
        };

        VULKAN_CHECK(vkCreateFramebuffer(ptAppData->tContextComponents.tDevice, &tFramebufferInfo, NULL, &ptAppData->tPipelineComponents.tFramebuffers[i]));
    }
}

void
hg_recreate_swapchain(hgAppData* ptState)
{
    // wait for device to be idle before destroying resources then clean up old resources
    vkDeviceWaitIdle(ptState->tContextComponents.tDevice);
    hg_cleanup_swapchain_resources(ptState);

    // get new window dimensions (GLFW handles) && handle minimization (window size is 0)
    int newWidth = 0, newHeight = 0;
    glfwGetFramebufferSize(ptState->pWindow, &newWidth, &newHeight);
    while (newWidth == 0 || newHeight == 0) 
    {
        glfwGetFramebufferSize(ptState->pWindow, &newWidth, &newHeight);
        glfwWaitEvents();
    }

    // recreate swapchain with new size
    hg_create_swapchain(ptState, VK_PRESENT_MODE_FIFO_KHR);

    // recreate all swapchain dependent resources
    hg_create_framebuffers(ptState);        // framebuffers depend on swapchain images
    hg_allocate_frame_cmd_buffers(ptState); // command buffers should be recreated

    // update state with new dimensions
    ptState->width = newWidth;
    ptState->height = newHeight;
}

// =============================================================================
// RESOURCE CREATION
// =============================================================================

// -------------------------------
// buffers
// -------------------------------
hgVertexBuffer 
hg_create_vertex_buffer(hgAppData* ptAppData, void* data, size_t size, size_t stride)
{
    hgVertexBuffer tNewBuffer = {0};

    tNewBuffer.szSize = size;
    tNewBuffer.uVertexCount = size / stride;

    // create staging buffer
    VkBuffer tStagingBuffer;
    VkDeviceMemory tStagingMemory;
    hg_create_buffer(&ptAppData->tContextComponents, (VkDeviceSize)size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, 
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &tStagingBuffer, &tStagingMemory);

    // map and copy data
    void* pMapped;
    vkMapMemory(ptAppData->tContextComponents.tDevice, tStagingMemory, 0, size, 0, &pMapped);
    memcpy(pMapped, data, size);
    vkUnmapMemory(ptAppData->tContextComponents.tDevice, tStagingMemory);

    // create device local buffer
    hg_create_buffer(&ptAppData->tContextComponents, (VkDeviceSize)size, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, 
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &tNewBuffer.tBuffer, &tNewBuffer.tMemory);

    // copy staging to device
    hg_copy_buffer(&ptAppData->tContextComponents, &ptAppData->tCommandComponents, 
        tStagingBuffer, tNewBuffer.tBuffer, size);

    // cleanup staging
    vkDestroyBuffer(ptAppData->tContextComponents.tDevice, tStagingBuffer, NULL);
    vkFreeMemory(ptAppData->tContextComponents.tDevice, tStagingMemory, NULL);

    return tNewBuffer;
}

hgIndexBuffer 
hg_create_index_buffer(hgAppData* ptAppData, uint16_t* indices, uint32_t count)
{
    hgIndexBuffer tNewBuffer = {0};

    size_t szSize = sizeof(uint16_t) * count;
    tNewBuffer.szSize = szSize;
    tNewBuffer.uIndexCount = count;

    // create staging buffer
    VkBuffer tStagingBuffer;
    VkDeviceMemory tStagingMemory;
    hg_create_buffer(&ptAppData->tContextComponents, (VkDeviceSize)szSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, 
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &tStagingBuffer, &tStagingMemory);

    // map and copy
    void* pMapped;
    vkMapMemory(ptAppData->tContextComponents.tDevice, tStagingMemory, 0, szSize, 0, &pMapped);
    memcpy(pMapped, indices, szSize);
    vkUnmapMemory(ptAppData->tContextComponents.tDevice, tStagingMemory);

    // create device buffer
    hg_create_buffer(&ptAppData->tContextComponents, (VkDeviceSize)szSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT, 
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &tNewBuffer.tBuffer, &tNewBuffer.tMemory);

    // copy
    hg_copy_buffer(&ptAppData->tContextComponents, &ptAppData->tCommandComponents, 
        tStagingBuffer, tNewBuffer.tBuffer, szSize);

    // cleanup staging
    vkDestroyBuffer(ptAppData->tContextComponents.tDevice, tStagingBuffer, NULL);
    vkFreeMemory(ptAppData->tContextComponents.tDevice, tStagingMemory, NULL);

    return tNewBuffer;
}

// -------------------------------
// textures
// -------------------------------
unsigned char*
hg_load_texture_data(const char* pcFileName, int* iWidthOut, int* iHeightOut)
{
    int iComponentsInFile = 0;
    return stbi_load(pcFileName, iWidthOut, iHeightOut, &iComponentsInFile, 4);
}

hgTexture 
hg_create_texture(hgAppData* ptAppData, const unsigned char* pucData, int iWidth, int iHeight)
{
    hgTexture tTexture = {0};
    tTexture.iWidth    = iWidth;
    tTexture.iHeight   = iHeight;

    // create image
    VkImageCreateInfo tImageInfo = {
        .sType         = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .imageType     = VK_IMAGE_TYPE_2D,
        .format        = VK_FORMAT_R8G8B8A8_UNORM,
        .extent        = {iWidth, iHeight, 1},
        .mipLevels     = 1,
        .arrayLayers   = 1,
        .samples       = VK_SAMPLE_COUNT_1_BIT,
        .tiling        = VK_IMAGE_TILING_OPTIMAL,
        .usage         = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        .sharingMode   = VK_SHARING_MODE_EXCLUSIVE,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
    };
    VULKAN_CHECK(vkCreateImage(ptAppData->tContextComponents.tDevice, &tImageInfo, NULL, &tTexture.tImage));

    // allocate memory
    VkMemoryRequirements tMemRequirements;
    vkGetImageMemoryRequirements(ptAppData->tContextComponents.tDevice, tTexture.tImage, &tMemRequirements);

    VkMemoryAllocateInfo tAllocInfo = {
        .sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize  = tMemRequirements.size,
        .memoryTypeIndex = hg_find_memory_type(&ptAppData->tContextComponents, tMemRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)
    };
    VULKAN_CHECK(vkAllocateMemory(ptAppData->tContextComponents.tDevice, &tAllocInfo, NULL, &tTexture.tMemory));
    VULKAN_CHECK(vkBindImageMemory(ptAppData->tContextComponents.tDevice, tTexture.tImage, tTexture.tMemory, 0));

    // upload texture data (using staging buffer)
    hg_upload_to_image(ptAppData, tTexture.tImage, pucData, iWidth, iHeight);

    // create image view
    VkImageViewCreateInfo tViewInfo = {
        .sType      = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image      = tTexture.tImage,
        .viewType   = VK_IMAGE_VIEW_TYPE_2D,
        .format     = VK_FORMAT_R8G8B8A8_UNORM,
        .components = {
            .r = VK_COMPONENT_SWIZZLE_IDENTITY,
            .g = VK_COMPONENT_SWIZZLE_IDENTITY,
            .b = VK_COMPONENT_SWIZZLE_IDENTITY,
            .a = VK_COMPONENT_SWIZZLE_IDENTITY
        },
        .subresourceRange   = {
            .aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT,
            .baseMipLevel   = 0,
            .levelCount     = 1,
            .baseArrayLayer = 0,
            .layerCount     = 1
        }
    };
    VULKAN_CHECK(vkCreateImageView(ptAppData->tContextComponents.tDevice, &tViewInfo, NULL, &tTexture.tImageView));

    return tTexture;
}

// -------------------------------
// pipelines
// -------------------------------
hgPipeline
hg_create_graphics_pipeline(hgAppData* ptAppData, hgPipelineConfig* ptConfig)
{
    // create shader modules
    VkShaderModule tVertShaderModule = hg_create_shader_module(ptAppData, ptConfig->pcVertexShaderPath);
    VkShaderModule tFragShaderModule = hg_create_shader_module(ptAppData, ptConfig->pcFragmentShaderPath);

    VkPipelineShaderStageCreateInfo tVertShaderStageInfo = {
        .sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        .stage  = VK_SHADER_STAGE_VERTEX_BIT,
        .module = tVertShaderModule,
        .pName  = "main"
    };

    VkPipelineShaderStageCreateInfo tFragShaderStageInfo = {
        .sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        .stage  = VK_SHADER_STAGE_FRAGMENT_BIT,
        .module = tFragShaderModule,
        .pName  = "main"
    };
    VkPipelineShaderStageCreateInfo tShaderStages[] = {tVertShaderStageInfo, tFragShaderStageInfo};

    // vertex input state using config
    VkVertexInputBindingDescription tBindingDescription = {
        .binding   = 0,
        .stride    = ptConfig->uVertexStride,
        .inputRate = VK_VERTEX_INPUT_RATE_VERTEX
    };

    VkPipelineVertexInputStateCreateInfo tVertexInputInfo = {
        .sType                           = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        .vertexBindingDescriptionCount   = (ptConfig->uVertexStride > 0) ? 1 : 0,
        .pVertexBindingDescriptions      = (ptConfig->uVertexStride > 0) ? &tBindingDescription : NULL,
        .vertexAttributeDescriptionCount = ptConfig->uAttributeCount,
        .pVertexAttributeDescriptions    = ptConfig->ptAttributeDescriptions
    };

    VkPipelineInputAssemblyStateCreateInfo tInputAssembly = {
        .sType                  = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
        .topology               = ptConfig->tTopology,
        .primitiveRestartEnable = VK_FALSE
    };

    VkViewport tViewport = {
        .x        = 0.0f,
        .y        = 0.0f,
        .width    = (float)ptAppData->tSwapchainComponents.tExtent.width,
        .height   = (float)ptAppData->tSwapchainComponents.tExtent.height,
        .minDepth = 0.0f,
        .maxDepth = 1.0f
    };

    VkRect2D tScissor = {
        .offset = {0, 0},
        .extent = ptAppData->tSwapchainComponents.tExtent
    };

    VkPipelineViewportStateCreateInfo tViewportState = {
        .sType         = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
        .viewportCount = 1,
        .pViewports    = &tViewport,
        .scissorCount  = 1,
        .pScissors     = &tScissor
    };

    VkPipelineRasterizationStateCreateInfo tRasterizer = {
        .sType                   = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
        .depthClampEnable        = VK_FALSE,
        .rasterizerDiscardEnable = VK_FALSE,
        .polygonMode             = VK_POLYGON_MODE_FILL,
        .lineWidth               = 1.0f,
        .cullMode                = ptConfig->tCullMode,
        .frontFace               = ptConfig->tFrontFace,
        .depthBiasEnable         = VK_FALSE
    };

    VkPipelineMultisampleStateCreateInfo tMultisampling = {
        .sType                = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
        .sampleShadingEnable  = VK_FALSE,
        .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT
    };

    VkPipelineColorBlendAttachmentState tColorBlendAttachment = {
        .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | 
                          VK_COLOR_COMPONENT_G_BIT | 
                          VK_COLOR_COMPONENT_B_BIT | 
                          VK_COLOR_COMPONENT_A_BIT,
        .blendEnable    = ptConfig->bBlendEnable
    };

    VkPipelineColorBlendStateCreateInfo tColorBlending = {
        .sType           = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
        .logicOpEnable   = VK_FALSE,
        .logicOp         = VK_LOGIC_OP_COPY,
        .attachmentCount = 1,
        .pAttachments    = &tColorBlendAttachment,
        .blendConstants  = {0.0f, 0.0f, 0.0f, 0.0f}
    };

    // create pipeline layout using config
    VkPipelineLayoutCreateInfo tPipelineLayoutInfo = {
        .sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount         = ptConfig->uDescriptorSetLayoutCount,
        .pSetLayouts            = ptConfig->ptDescriptorSetLayouts,
        .pushConstantRangeCount = ptConfig->uPushConstantRangeCount,
        .pPushConstantRanges    = ptConfig->ptPushConstantRanges
    };

    hgPipeline tPipelineResult = {0};

    VULKAN_CHECK(vkCreatePipelineLayout(ptAppData->tContextComponents.tDevice, &tPipelineLayoutInfo, NULL, 
            &tPipelineResult.tPipelineLayout));

    // create graphics pipeline
    VkGraphicsPipelineCreateInfo tPipelineInfo = {
        .sType               = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        .stageCount          = 2,
        .pStages             = tShaderStages,
        .pVertexInputState   = &tVertexInputInfo,
        .pInputAssemblyState = &tInputAssembly,
        .pViewportState      = &tViewportState,
        .pRasterizationState = &tRasterizer,
        .pMultisampleState   = &tMultisampling,
        .pColorBlendState    = &tColorBlending,
        .pDepthStencilState  = NULL, // TODO: do we want this in the future?
        .layout              = tPipelineResult.tPipelineLayout,
        .renderPass          = ptAppData->tPipelineComponents.tRenderPass,
        .subpass             = 0,
        .basePipelineHandle  = VK_NULL_HANDLE,
        .basePipelineIndex   = -1
    };

    VULKAN_CHECK(vkCreateGraphicsPipelines(ptAppData->tContextComponents.tDevice, 
            VK_NULL_HANDLE, 1, &tPipelineInfo, NULL, &tPipelineResult.tPipeline));

    // cleanup shader modules
    vkDestroyShaderModule(ptAppData->tContextComponents.tDevice, tVertShaderModule, NULL);
    vkDestroyShaderModule(ptAppData->tContextComponents.tDevice, tFragShaderModule, NULL);

    return tPipelineResult;
}

// =============================================================================
// FRAME RENDERING
// =============================================================================

// -------------------------------
// frame lifecycle
// -------------------------------
uint32_t 
hg_begin_frame(hgAppData* ptState)
{
    vkWaitForFences(ptState->tContextComponents.tDevice, 1, &ptState->tSyncComponents.tInFlight, VK_TRUE, UINT64_MAX);
    vkResetFences(ptState->tContextComponents.tDevice, 1, &ptState->tSyncComponents.tInFlight);

    uint32_t uImageIndex = 0;
    vkAcquireNextImageKHR(ptState->tContextComponents.tDevice, ptState->tSwapchainComponents.tSwapchain, UINT64_MAX, 
        ptState->tSyncComponents.tImageAvailable, VK_NULL_HANDLE, &uImageIndex);

    // set current frame index
    ptState->tCommandComponents.uCurrentImageIndex = uImageIndex;

    // get, reset, and begin command buffer
    VkCommandBuffer tCommandBuffer = hg_get_current_frame_cmd_buffer(ptState);
    vkResetCommandBuffer(tCommandBuffer, 0);

    VkCommandBufferBeginInfo tBeginInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO
    };
    VULKAN_CHECK(vkBeginCommandBuffer(ptState->tCommandComponents.tCommandBuffers[uImageIndex], &tBeginInfo));

    return uImageIndex;
}

void 
hg_end_frame(hgAppData* ptState, uint32_t uImageIndex)
{
    // get current command buffer
    VkCommandBuffer tCommandBuffer = hg_get_current_frame_cmd_buffer(ptState);
    VULKAN_CHECK(vkEndCommandBuffer(tCommandBuffer));

    // submit command buffer
    VkSubmitInfo tSubmitInfo = {
        .sType                = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .waitSemaphoreCount   = 1,
        .pWaitSemaphores      = &ptState->tSyncComponents.tImageAvailable,
        .pWaitDstStageMask    = (VkPipelineStageFlags[]){VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT},
        .commandBufferCount   = 1,
        .pCommandBuffers      = &ptState->tCommandComponents.tCommandBuffers[uImageIndex],
        .signalSemaphoreCount = 1,
        .pSignalSemaphores    = &ptState->tSyncComponents.tRenderFinished
    };

    VULKAN_CHECK(vkQueueSubmit(ptState->tContextComponents.tGraphicsQueue, 1, &tSubmitInfo, ptState->tSyncComponents.tInFlight));

    // present
    VkPresentInfoKHR tPresentInfo = {
        .sType              = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores    = &ptState->tSyncComponents.tRenderFinished,
        .swapchainCount     = 1,
        .pSwapchains        = &ptState->tSwapchainComponents.tSwapchain,
        .pImageIndices      = &uImageIndex
    };
    VULKAN_CHECK(vkQueuePresentKHR(ptState->tContextComponents.tGraphicsQueue, &tPresentInfo));
}

// -------------------------------
// render pass
// -------------------------------
void 
hg_begin_render_pass(hgAppData* ptState, uint32_t uImageIndex) 
{
    // get command buffer
    VkCommandBuffer tCommandBuffer = hg_get_current_frame_cmd_buffer(ptState);

    // grab clear color from state
    VkClearValue clearColor = {{{
        ptState->tPipelineComponents.afClearColor[0],
        ptState->tPipelineComponents.afClearColor[1],
        ptState->tPipelineComponents.afClearColor[2],
        ptState->tPipelineComponents.afClearColor[3]
    }}};

    // begin render pass
    VkRenderPassBeginInfo renderPassInfo = {
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
        .renderPass = ptState->tPipelineComponents.tRenderPass,
        .framebuffer = ptState->tPipelineComponents.tFramebuffers[uImageIndex],
        .renderArea = {
            .offset = {0, 0},
            .extent = ptState->tSwapchainComponents.tExtent
        },
        .clearValueCount = 1,
        .pClearValues = &clearColor
    };
    vkCmdBeginRenderPass(tCommandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
}

void
hg_end_render_pass(hgAppData* ptState)
{
    vkCmdEndRenderPass(ptState->tCommandComponents.tCommandBuffers[ptState->tCommandComponents.uCurrentImageIndex]);
}

// -------------------------------
// bind state
// -------------------------------
// TODO: implement hg_cmd_bind_pipeline, hg_cmd_bind_vertex_buffer, etc.

// -------------------------------
// draw commands
// -------------------------------
// TODO: implement hg_cmd_draw, hg_cmd_draw_indexed, etc.

// -------------------------------
// convenience wrappers
// -------------------------------
// TODO: implement hg_draw_mesh

// =============================================================================
// CLEANUP
// =============================================================================

void 
hg_cleanup(hgAppData* ptAppData) 
{
    // TODO: cleanup function is temporary and alternate clean up scenario needed
    vkDeviceWaitIdle(ptAppData->tContextComponents.tDevice);

    if(ptAppData->tCommandComponents.tCommandBuffers) 
    {
        vkFreeCommandBuffers(ptAppData->tContextComponents.tDevice, 
                           ptAppData->tCommandComponents.tCommandPool,
                           ptAppData->tSwapchainComponents.uSwapchainImageCount,
                           ptAppData->tCommandComponents.tCommandBuffers);
        free(ptAppData->tCommandComponents.tCommandBuffers);
        ptAppData->tCommandComponents.tCommandBuffers = NULL;
    }

    if(ptAppData->tCommandComponents.tCommandPool != VK_NULL_HANDLE) 
    {
        vkDestroyCommandPool(ptAppData->tContextComponents.tDevice, ptAppData->tCommandComponents.tCommandPool, NULL);
        ptAppData->tCommandComponents.tCommandPool = VK_NULL_HANDLE;
    }


    if(ptAppData->tSyncComponents.tImageAvailable != VK_NULL_HANDLE) 
    {
        vkDestroySemaphore(ptAppData->tContextComponents.tDevice, ptAppData->tSyncComponents.tImageAvailable, NULL);
        ptAppData->tSyncComponents.tImageAvailable = VK_NULL_HANDLE;
    }
    if(ptAppData->tSyncComponents.tRenderFinished != VK_NULL_HANDLE) 
    {
        vkDestroySemaphore(ptAppData->tContextComponents.tDevice, ptAppData->tSyncComponents.tRenderFinished, NULL);
        ptAppData->tSyncComponents.tRenderFinished = VK_NULL_HANDLE;
    }
    if(ptAppData->tSyncComponents.tInFlight != VK_NULL_HANDLE) 
    {
        vkDestroyFence(ptAppData->tContextComponents.tDevice, ptAppData->tSyncComponents.tInFlight, NULL);
        ptAppData->tSyncComponents.tInFlight = VK_NULL_HANDLE;
    }

    if(ptAppData->tPipelineComponents.tFramebuffers) 
    {
        for(uint32_t i = 0; i < ptAppData->tSwapchainComponents.uSwapchainImageCount; i++) 
        {
            if(ptAppData->tPipelineComponents.tFramebuffers[i] != VK_NULL_HANDLE) 
            {
                vkDestroyFramebuffer(ptAppData->tContextComponents.tDevice, ptAppData->tPipelineComponents.tFramebuffers[i], NULL);
                ptAppData->tPipelineComponents.tFramebuffers[i] = VK_NULL_HANDLE;
            }
        }
        free(ptAppData->tPipelineComponents.tFramebuffers);
        ptAppData->tPipelineComponents.tFramebuffers = NULL;
    }

    if(ptAppData->tPipelineComponents.tRenderPass != VK_NULL_HANDLE) 
    {
        vkDestroyRenderPass(ptAppData->tContextComponents.tDevice, ptAppData->tPipelineComponents.tRenderPass, NULL);
        ptAppData->tPipelineComponents.tRenderPass = VK_NULL_HANDLE;
    }

    if(ptAppData->tSwapchainComponents.tSwapchainImageViews) 
    {
        for(uint32_t i = 0; i < ptAppData->tSwapchainComponents.uSwapchainImageCount; i++)
        {
            if(ptAppData->tSwapchainComponents.tSwapchainImageViews[i] != VK_NULL_HANDLE)
            {
                vkDestroyImageView(ptAppData->tContextComponents.tDevice, ptAppData->tSwapchainComponents.tSwapchainImageViews[i], NULL);
                ptAppData->tSwapchainComponents.tSwapchainImageViews[i] = VK_NULL_HANDLE;
            }
        }
        free(ptAppData->tSwapchainComponents.tSwapchainImageViews);
        ptAppData->tSwapchainComponents.tSwapchainImageViews = NULL;
    }

    if(ptAppData->tSwapchainComponents.tSwapchainImages) 
    {
        free(ptAppData->tSwapchainComponents.tSwapchainImages);
        ptAppData->tSwapchainComponents.tSwapchainImages = NULL;
    }

    if(ptAppData->tSwapchainComponents.tSwapchain != VK_NULL_HANDLE) 
    {
        vkDestroySwapchainKHR(ptAppData->tContextComponents.tDevice, ptAppData->tSwapchainComponents.tSwapchain, NULL);
        ptAppData->tSwapchainComponents.tSwapchain = VK_NULL_HANDLE;
    }

    if(ptAppData->tSwapchainComponents.tSurface != VK_NULL_HANDLE) 
    {
        vkDestroySurfaceKHR(ptAppData->tContextComponents.tInstance, ptAppData->tSwapchainComponents.tSurface, NULL);
        ptAppData->tSwapchainComponents.tSurface = VK_NULL_HANDLE;
    }

    if(ptAppData->tContextComponents.tDevice != VK_NULL_HANDLE) {
        vkDestroyDevice(ptAppData->tContextComponents.tDevice, NULL);
        ptAppData->tContextComponents.tDevice = VK_NULL_HANDLE;
    }

    if(ptAppData->tContextComponents.tInstance != VK_NULL_HANDLE) 
    {
        vkDestroyInstance(ptAppData->tContextComponents.tInstance, NULL);
        ptAppData->tContextComponents.tInstance = VK_NULL_HANDLE;
    }
}

void
hg_cleanup_swapchain_resources(hgAppData* ptState) 
{
    // free command buffers
    if (ptState->tCommandComponents.tCommandBuffers) {
        vkFreeCommandBuffers(ptState->tContextComponents.tDevice, ptState->tCommandComponents.tCommandPool, 
            ptState->tSwapchainComponents.uSwapchainImageCount, ptState->tCommandComponents.tCommandBuffers);
        free(ptState->tCommandComponents.tCommandBuffers);
        ptState->tCommandComponents.tCommandBuffers = NULL;
    }

    // destroy framebuffers
    if (ptState->tPipelineComponents.tFramebuffers) 
    {
        for (uint32_t i = 0; i < ptState->tSwapchainComponents.uSwapchainImageCount; i++) 
        {
            vkDestroyFramebuffer(ptState->tContextComponents.tDevice, ptState->tPipelineComponents.tFramebuffers[i], NULL);
        }
        free(ptState->tPipelineComponents.tFramebuffers);
        ptState->tPipelineComponents.tFramebuffers = NULL;
    }

    // destroy image views
    if (ptState->tSwapchainComponents.tSwapchainImageViews) 
    {
        for (uint32_t i = 0; i < ptState->tSwapchainComponents.uSwapchainImageCount; i++) 
        {
            vkDestroyImageView(ptState->tContextComponents.tDevice,ptState->tSwapchainComponents.tSwapchainImageViews[i], NULL);
        }
        free(ptState->tSwapchainComponents.tSwapchainImageViews);
        ptState->tSwapchainComponents.tSwapchainImageViews = NULL;
    }

    // destroy old swapchain
    if (ptState->tSwapchainComponents.tSwapchain != VK_NULL_HANDLE) 
    {
        vkDestroySwapchainKHR(ptState->tContextComponents.tDevice, ptState->tSwapchainComponents.tSwapchain, NULL);
        ptState->tSwapchainComponents.tSwapchain = VK_NULL_HANDLE;
    }
}

void 
hg_destroy_pipeline(hgAppData* ptAppData, hgPipeline* tPipeline)
{
    if(tPipeline->tPipeline != VK_NULL_HANDLE)       vkDestroyPipeline      (ptAppData->tContextComponents.tDevice, tPipeline->tPipeline, NULL);
    if(tPipeline->tPipelineLayout != VK_NULL_HANDLE) vkDestroyPipelineLayout(ptAppData->tContextComponents.tDevice, tPipeline->tPipelineLayout, NULL);

};

void 
hg_destroy_texture(hgAppData* ptAppData, hgTexture* tTexture)
{
    if(tTexture->tImageView != VK_NULL_HANDLE) vkDestroyImageView(ptAppData->tContextComponents.tDevice, tTexture->tImageView, NULL);
    if(tTexture->tImage != VK_NULL_HANDLE)     vkDestroyImage(ptAppData->tContextComponents.tDevice, tTexture->tImage, NULL);
    if(tTexture->tMemory != VK_NULL_HANDLE)    vkFreeMemory(ptAppData->tContextComponents.tDevice, tTexture->tMemory, NULL);

    memset(tTexture, 0, sizeof(hgTexture));
}

// =============================================================================
// INTERNAL HELPERS
// =============================================================================

// -------------------------------
// memory & buffer helpers
// -------------------------------
uint32_t 
hg_find_memory_type(hgVulkanContext* ptContext, uint32_t uTypeFilter, VkMemoryPropertyFlags tProperties)
{
    VkPhysicalDeviceMemoryProperties tMemProperties;
    vkGetPhysicalDeviceMemoryProperties(ptContext->tPhysicalDevice, &tMemProperties);

    for(uint32_t i = 0; i < tMemProperties.memoryTypeCount; i++) 
    {
        if((uTypeFilter & (1 << i)) && (tMemProperties.memoryTypes[i].propertyFlags & tProperties) == tProperties)
        {
            return i;
        }
    }
    printf("Failed to find suitable memory type!\n");
    exit(1);
}

void
hg_create_buffer(hgVulkanContext* ptContext, VkDeviceSize tSize, VkBufferUsageFlags tFlags, VkMemoryPropertyFlags tProperties, VkBuffer* ptBuffer, VkDeviceMemory* pMemory)
{
    // create buffer
    VkBufferCreateInfo tBufferCreateInfo = {
        .sType       = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size        = tSize,
        .usage       = tFlags,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE
    };
    VULKAN_CHECK(vkCreateBuffer(ptContext->tDevice, &tBufferCreateInfo, NULL, ptBuffer));

    // get memory requirements
    VkMemoryRequirements tMemRequirements;
    vkGetBufferMemoryRequirements(ptContext->tDevice, *ptBuffer, &tMemRequirements);

    // find suitable memory
    uint32_t tMemTypeIndex = hg_find_memory_type(ptContext, tMemRequirements.memoryTypeBits, tProperties);

    // allocate and bind memory
    VkMemoryAllocateInfo tMemAllocInfo = {
        .sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize  = tMemRequirements.size,
        .memoryTypeIndex = tMemTypeIndex
    };
    VULKAN_CHECK(vkAllocateMemory(ptContext->tDevice, &tMemAllocInfo, NULL, pMemory));
    VULKAN_CHECK(vkBindBufferMemory(ptContext->tDevice, *ptBuffer, *pMemory, 0));
}

void hg_copy_buffer(hgVulkanContext* ptContext, hgCommandResources* ptCommands, VkBuffer tSrcBuffer, VkBuffer tDstBuffer, VkDeviceSize tSize)
{
    // create a temporary command buffer for the copy
    VkCommandBufferAllocateInfo tBufferAllocInfo = {
        .sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandPool        = ptCommands->tCommandPool,
        .commandBufferCount = 1
    };

    VkCommandBuffer tCommandBuffer;
    VULKAN_CHECK(vkAllocateCommandBuffers(ptContext->tDevice, &tBufferAllocInfo, &tCommandBuffer));

    // Record copy command
    VkCommandBufferBeginInfo tBeginCommandBufferInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
    };
    VULKAN_CHECK(vkBeginCommandBuffer(tCommandBuffer, &tBeginCommandBufferInfo));

    VkBufferCopy tCopyRegion = {
        .srcOffset = 0,
        .dstOffset = 0,
        .size      = tSize
    };
    vkCmdCopyBuffer(tCommandBuffer, tSrcBuffer, tDstBuffer, 1, &tCopyRegion);

    VULKAN_CHECK(vkEndCommandBuffer(tCommandBuffer));

    // submit and wait for completion
    VkSubmitInfo submitInfo = {
        .sType              = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .commandBufferCount = 1,
        .pCommandBuffers    = &tCommandBuffer
    };
    VULKAN_CHECK(vkQueueSubmit(ptContext->tGraphicsQueue, 1, &submitInfo, VK_NULL_HANDLE));
    VULKAN_CHECK(vkQueueWaitIdle(ptContext->tGraphicsQueue));  // wait for copy to finish

    vkFreeCommandBuffers(ptContext->tDevice, ptCommands->tCommandPool, 1, &tCommandBuffer);
}

// -------------------------------
// single time commands
// -------------------------------
VkCommandBuffer 
hg_begin_single_time_commands(hgAppData* ptAppData) 
{
    VkCommandBufferAllocateInfo tAllocInfo = {
        .sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandPool        = ptAppData->tCommandComponents.tCommandPool,
        .commandBufferCount = 1
    };
    VkCommandBuffer tCommandBuffer;
    VULKAN_CHECK(vkAllocateCommandBuffers(ptAppData->tContextComponents.tDevice, &tAllocInfo, &tCommandBuffer));

    VkCommandBufferBeginInfo tBeginInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
    };
    VULKAN_CHECK(vkBeginCommandBuffer(tCommandBuffer, &tBeginInfo));
    return tCommandBuffer;
}

void 
hg_end_single_time_commands(hgAppData* ptAppData, VkCommandBuffer tCommandBuffer) 
{
    VULKAN_CHECK(vkEndCommandBuffer(tCommandBuffer));

    VkSubmitInfo tSubmitInfo = {
        .sType              = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .commandBufferCount = 1,
        .pCommandBuffers    = &tCommandBuffer
    };

    // submit and wait for completion
    VULKAN_CHECK(vkQueueSubmit(ptAppData->tContextComponents.tGraphicsQueue, 1, &tSubmitInfo, VK_NULL_HANDLE));
    VULKAN_CHECK(vkQueueWaitIdle(ptAppData->tContextComponents.tGraphicsQueue));

    // free the command buffer
    vkFreeCommandBuffers(ptAppData->tContextComponents.tDevice, ptAppData->tCommandComponents.tCommandPool, 1, &tCommandBuffer);
}

// -------------------------------
// image operations
// -------------------------------
void 
hg_transition_image_layout(VkCommandBuffer tCommandBuffer, VkImage tImage, VkImageLayout tOldLayout, 
    VkImageLayout tNewLayout, VkImageSubresourceRange tSubresourceRange, VkPipelineStageFlags tSrcStageMask, 
    VkPipelineStageFlags tDstStageMask) 
{
    VkImageMemoryBarrier tBarrier = {
        .sType               = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .oldLayout           = tOldLayout,
        .newLayout           = tNewLayout,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image               = tImage,
        .subresourceRange    = tSubresourceRange,
    };

    // source layouts (old)
    switch (tOldLayout) 
    {
        case VK_IMAGE_LAYOUT_UNDEFINED:
            tBarrier.srcAccessMask = 0;
            break;

        case VK_IMAGE_LAYOUT_PREINITIALIZED:
            tBarrier.srcAccessMask = VK_ACCESS_HOST_WRITE_BIT;
            break;

        case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
            tBarrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
            break;

        case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
            tBarrier.srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
            break;

        case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
            tBarrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
            break;

        case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
            tBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            break;

        case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
            tBarrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
            break;

        default:
            break;
    }

    // target layouts (new)
    switch (tNewLayout) 
    {
        case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
            tBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            break;

        case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
            tBarrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
            break;

        case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
            tBarrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
            break;

        case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
            tBarrier.dstAccessMask |= VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
            break;

        case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
            if (tBarrier.srcAccessMask == 0) 
            {
                tBarrier.srcAccessMask = VK_ACCESS_HOST_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT;
            }
            tBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
            break;

        default:
            break;
    }

    vkCmdPipelineBarrier(tCommandBuffer, tSrcStageMask, tDstStageMask, 0, 0, NULL, 0, NULL, 1, &tBarrier);
}

void 
hg_upload_to_image(hgAppData* ptAppData, VkImage tImage, const unsigned char* pData, int iWidth, int iHeight)
{
    VkDeviceSize imageSize = iWidth * iHeight * 4; // RGBA8

    // create staging buffer
    VkBuffer       tStagingBuffer;
    VkDeviceMemory tStagingBufferMemory;
    hg_create_buffer(&ptAppData->tContextComponents, imageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, 
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &tStagingBuffer, &tStagingBufferMemory);

    // copy data to staging buffer
    void* pMapped;
    vkMapMemory(ptAppData->tContextComponents.tDevice, tStagingBufferMemory, 0, imageSize, 0, &pMapped);
    memcpy(pMapped, pData, imageSize);
    vkUnmapMemory(ptAppData->tContextComponents.tDevice, tStagingBufferMemory);

    // record copy commands
    VkCommandBuffer tCmdBuffer = hg_begin_single_time_commands(ptAppData);

    // transition to transfer dst
    VkImageSubresourceRange tSubResRan = {
        .aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT,
        .baseMipLevel   = 0,
        .levelCount     = 1,
        .baseArrayLayer = 0,
        .layerCount     = 1
    };

    hg_transition_image_layout(tCmdBuffer, tImage, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 
        tSubResRan, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);

    // copy buffer to image
    VkBufferImageCopy tRegion = {
        .bufferOffset       = 0,
        .bufferRowLength    = 0,
        .bufferImageHeight  = 0,
        .imageSubresource   = {
            .aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT,
            .mipLevel       = 0,
            .baseArrayLayer = 0,
            .layerCount     = 1
        },
        .imageOffset        = {0, 0, 0},
        .imageExtent        = {iWidth, iHeight, 1}
    };
    vkCmdCopyBufferToImage(tCmdBuffer, tStagingBuffer, tImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &tRegion);

    // transition to shader read
    hg_transition_image_layout(tCmdBuffer, tImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 
        tSubResRan, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);

    hg_end_single_time_commands(ptAppData, tCmdBuffer);

    // cleanup staging
    vkDestroyBuffer(ptAppData->tContextComponents.tDevice, tStagingBuffer, NULL);
    vkFreeMemory(ptAppData->tContextComponents.tDevice, tStagingBufferMemory, NULL);
}

// -------------------------------
// shader loading
// -------------------------------
VkShaderModule 
hg_create_shader_module(hgAppData* ptAppData, const char* pcFilename) 
{
    // TODO: clean up paths
    const char* apcPossiblePaths[] = {
        pcFilename,                         // absolute path
        "../out/shaders/vert.spv",          // relative from src folder
        "../../out/shaders/vert.spv",       // relative if running from project root
        "out/shaders/vert.spv",             // relative if running from project root
        NULL
    };

    FILE* pFile = NULL;
    const char* pcFoundPath = NULL;

    for(int i = 0; apcPossiblePaths[i] != NULL; i++) 
    {
        pFile = fopen(apcPossiblePaths[i], "rb");
        if(pFile) 
        {
            pcFoundPath = apcPossiblePaths[i];
            printf("Found shader at: %s\n", pcFoundPath);
            break;
        }
    }

    if(!pFile) 
    {
        printf("Failed to open shader file: %s\n", pcFilename);
        printf("Tried paths:\n");
        for(int i = 0; apcPossiblePaths[i] != NULL; i++) 
        {
            printf("  %s\n", apcPossiblePaths[i]);
        }
        return VK_NULL_HANDLE;
    }

    fseek(pFile, 0, SEEK_END);
    long lSize = ftell(pFile);
    rewind(pFile);

    uint32_t* puCode = malloc(lSize);
    fread(puCode, lSize, 1, pFile);
    fclose(pFile);

    VkShaderModuleCreateInfo tCreateInfo = {
        .sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize = lSize,
        .pCode    = puCode
    };

    VkShaderModule tShaderModule;
    VULKAN_CHECK(vkCreateShaderModule(ptAppData->tContextComponents.tDevice, &tCreateInfo, NULL, &tShaderModule));

    free(puCode);
    return tShaderModule;
}

// -------------------------------
// command buffer access
// -------------------------------
VkCommandBuffer 
hg_get_current_frame_cmd_buffer(hgAppData* ptState) 
{
    return ptState->tCommandComponents.tCommandBuffers[ptState->tCommandComponents.uCurrentImageIndex];
}

// -------------------------------
// descriptor management
// -------------------------------
void hg_create_descriptor_set(hgAppData* ptAppData)
{
    // TODO: Implement descriptor set creation
};