// =============================================================================
// TABLE OF CONTENTS
// =============================================================================
/* Window & Platform Management
    ->Core Vulkan Context Initialization
    ->Swapchain Management
    ->Render Pipeline (RenderPass + Pipeline)
    ->Command Buffer Management
    ->Synchronization Objects
    ->Frame Rendering Loop
    ->Resource Creation & Management
        -Buffers & Memory
        -Vertex/Index Buffer Creation
        -Descriptor Management
        -Textures
    ->Shader Management
    ->Cleanup & Resource Destruction
    ->Helpers
*/
// =============================================================================

#include "vkHomeGrown.h"
#include <assert.h>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

// -----------------------------------------------------------------------------
// Window & Platform Management
// -----------------------------------------------------------------------------



// -----------------------------------------------------------------------------
// Core Vulkan Context Initialization
// -----------------------------------------------------------------------------
void
hg_create_instance(hgAppData* ptState) 
{
    VkApplicationInfo tAppInfo = {
        .sType              = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pApplicationName   = "Vulkan App",
        .applicationVersion = VK_MAKE_VERSION(1, 0, 0),
        .pEngineName        = "No Engine",
        .engineVersion      = VK_MAKE_VERSION(1, 0, 0),
        .apiVersion         = VK_API_VERSION_1_0
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

    // Optional: Add validation layers if needed
    #ifdef _DEBUG
    const char* validationLayers[] = {"VK_LAYER_KHRONOS_validation"};
    tCreateInfo.enabledLayerCount = 1;
    tCreateInfo.ppEnabledLayerNames = validationLayers;
    #endif

    VULKAN_CHECK(vkCreateInstance(&tCreateInfo, NULL, &ptState->tContextComponents.tInstance));
}

void
hg_create_surface(hgAppData* ptState) 
{
    // GLFW handles platform-specific surface creation
    VULKAN_CHECK(glfwCreateWindowSurface(
        ptState->tContextComponents.tInstance,
        ptState->pWindow,
        NULL,
        &ptState->tSwapchainComponents.tSurface
    ));
}

void 
hg_pick_physical_device(hgAppData* ptState) 
{
    uint32_t uDeviceCount = 0;
    vkEnumeratePhysicalDevices(ptState->tContextComponents.tInstance, &uDeviceCount, NULL);
    assert(uDeviceCount > 0);

    VkPhysicalDevice* ptDevices = malloc(uDeviceCount * sizeof(VkPhysicalDevice));
    vkEnumeratePhysicalDevices(ptState->tContextComponents.tInstance, &uDeviceCount, ptDevices);

    // just pick the first gpu
    ptState->tContextComponents.tPhysicalDevice = ptDevices[0];

    free(ptDevices);
}

void 
hg_create_logical_device(hgAppData* ptState) 
{
    // find queue family that supports graphics and presentation
    uint32_t uQueueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(ptState->tContextComponents.tPhysicalDevice, &uQueueFamilyCount, NULL);
    VkQueueFamilyProperties* pQueueFamilies = malloc(uQueueFamilyCount * sizeof(VkQueueFamilyProperties));
    vkGetPhysicalDeviceQueueFamilyProperties(ptState->tContextComponents.tPhysicalDevice, &uQueueFamilyCount, pQueueFamilies);

    // find graphics queue family
    ptState->tContextComponents.tGraphicsQueueFamily = UINT32_MAX;
    for(uint32_t i = 0; i < uQueueFamilyCount; i++) 
    {
        if(pQueueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) 
        {
            VkBool32 bPresentSupport = VK_FALSE;
            vkGetPhysicalDeviceSurfaceSupportKHR(ptState->tContextComponents.tPhysicalDevice, i, ptState->tSwapchainComponents.tSurface, &bPresentSupport);
            if(bPresentSupport) 
            {
                ptState->tContextComponents.tGraphicsQueueFamily = i;
                break;
            }
        }
    }
    assert(ptState->tContextComponents.tGraphicsQueueFamily != UINT32_MAX);

    free(pQueueFamilies);

    float fQueuePriority = 1.0f;
    VkDeviceQueueCreateInfo tQueueCreateInfo = {
        .sType            = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
        .queueFamilyIndex = ptState->tContextComponents.tGraphicsQueueFamily,
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

    VULKAN_CHECK(vkCreateDevice(ptState->tContextComponents.tPhysicalDevice, &tDeviceCreateInfo, NULL, &ptState->tContextComponents.tDevice));
    vkGetDeviceQueue(ptState->tContextComponents.tDevice, ptState->tContextComponents.tGraphicsQueueFamily, 0, &ptState->tContextComponents.tGraphicsQueue);
}

// -----------------------------------------------------------------------------
// Swapchain Management
// -----------------------------------------------------------------------------
void
hg_create_swapchain(hgAppData* ptState) 
{
    // get surface capabilities
    VkSurfaceCapabilitiesKHR tCapabilities;
    VULKAN_CHECK(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(ptState->tContextComponents.tPhysicalDevice, ptState->tSwapchainComponents.tSurface, &tCapabilities));

    // get surface formats
    uint32_t uFormatCount;
    VULKAN_CHECK(vkGetPhysicalDeviceSurfaceFormatsKHR(ptState->tContextComponents.tPhysicalDevice, ptState->tSwapchainComponents.tSurface, &uFormatCount, NULL));
    VkSurfaceFormatKHR* pFormats = malloc(uFormatCount * sizeof(VkSurfaceFormatKHR));
    VULKAN_CHECK(vkGetPhysicalDeviceSurfaceFormatsKHR(ptState->tContextComponents.tPhysicalDevice, ptState->tSwapchainComponents.tSurface, &uFormatCount, pFormats));

    // choose surface format (prefer B8G8R8A8_UNORM with sRGB)
    ptState->tSwapchainComponents.tFormat = VK_FORMAT_B8G8R8A8_UNORM;
    for(uint32_t i = 0; i < uFormatCount; i++) 
    {
        if(pFormats[i].format == VK_FORMAT_B8G8R8A8_UNORM && pFormats[i].colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) 
        {
            ptState->tSwapchainComponents.tFormat = pFormats[i].format;
            break;
        }
    }

    // choose swap extent
    if(tCapabilities.currentExtent.width != UINT32_MAX) 
    {
        ptState->tSwapchainComponents.tExtent = tCapabilities.currentExtent;
    } 
    else 
    {
        VkExtent2D tActualExtent = {ptState->width, ptState->height};
        tActualExtent.width = (tActualExtent.width < tCapabilities.minImageExtent.width) ? 
            tCapabilities.minImageExtent.width : (tActualExtent.width > tCapabilities.maxImageExtent.width) ? 
            tCapabilities.maxImageExtent.width : tActualExtent.width;
        tActualExtent.height = (tActualExtent.height < tCapabilities.minImageExtent.height) ? 
            tCapabilities.minImageExtent.height : (tActualExtent.height > tCapabilities.maxImageExtent.height) ? 
            tCapabilities.maxImageExtent.height : tActualExtent.height;
        ptState->tSwapchainComponents.tExtent = tActualExtent;
    }

    // choose image count
    uint32_t uImageCount = tCapabilities.minImageCount + 1;
    if(tCapabilities.maxImageCount > 0 && uImageCount > tCapabilities.maxImageCount) 
    {
        uImageCount = tCapabilities.maxImageCount;
    }

    // create swapchain
    VkSwapchainCreateInfoKHR tCreateInfo = {
        .sType            = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
        .surface          = ptState->tSwapchainComponents.tSurface,
        .minImageCount    = uImageCount,
        .imageFormat      = ptState->tSwapchainComponents.tFormat,
        .imageColorSpace  = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR,
        .imageExtent      = ptState->tSwapchainComponents.tExtent,
        .imageArrayLayers = 1,
        .imageUsage       = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
        .imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .preTransform     = tCapabilities.currentTransform,
        .compositeAlpha   = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
        .presentMode      = VK_PRESENT_MODE_FIFO_KHR,
        .clipped          = VK_TRUE
    };
    VULKAN_CHECK(vkCreateSwapchainKHR(ptState->tContextComponents.tDevice, &tCreateInfo, NULL, &ptState->tSwapchainComponents.tSwapchain));

    // get swapchain images
    VULKAN_CHECK(vkGetSwapchainImagesKHR(ptState->tContextComponents.tDevice, ptState->tSwapchainComponents.tSwapchain, &ptState->tSwapchainComponents.uSwapchainImageCount, NULL));
    ptState->tSwapchainComponents.tSwapchainImages = malloc(ptState->tSwapchainComponents.uSwapchainImageCount * sizeof(VkImage));
    VULKAN_CHECK(vkGetSwapchainImagesKHR(ptState->tContextComponents.tDevice, ptState->tSwapchainComponents.tSwapchain, &ptState->tSwapchainComponents.uSwapchainImageCount, ptState->tSwapchainComponents.tSwapchainImages));

    // create image views
    ptState->tSwapchainComponents.tSwapchainImageViews = malloc(ptState->tSwapchainComponents.uSwapchainImageCount * sizeof(VkImageView));
    for(uint32_t i = 0; i < ptState->tSwapchainComponents.uSwapchainImageCount; i++) 
    {
        VkImageViewCreateInfo tViewCreateInfo = {
            .sType      = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .image      = ptState->tSwapchainComponents.tSwapchainImages[i],
            .viewType   = VK_IMAGE_VIEW_TYPE_2D,
            .format     = ptState->tSwapchainComponents.tFormat,
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
        VULKAN_CHECK(vkCreateImageView(ptState->tContextComponents.tDevice, &tViewCreateInfo, NULL, &ptState->tSwapchainComponents.tSwapchainImageViews[i]));
    }
    free(pFormats);
}

// -----------------------------------------------------------------------------
// Render Pipeline (RenderPass + Pipeline)
// -----------------------------------------------------------------------------
void 
hg_create_render_pass(hgAppData* ptState) 
{
    VkAttachmentDescription tColorAttachment = {
        .format         = ptState->tSwapchainComponents.tFormat,
        .samples        = VK_SAMPLE_COUNT_1_BIT,
        .loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR,
        .storeOp        = VK_ATTACHMENT_STORE_OP_STORE,
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

    VULKAN_CHECK(vkCreateRenderPass(ptState->tContextComponents.tDevice, &tRenderPassInfo, NULL, &ptState->tPipelineComponents.tRenderPass));
}

void 
hg_create_graphics_pipeline(hgAppData* ptState) 
{
    printf("Loading shaders...\n");

    // try different paths TODO: cleanup shader paths once a consistent system is picked 
    VkShaderModule tVertShaderModule = hg_create_shader_module(ptState, "../out/shaders/vert.spv");
    if(tVertShaderModule == VK_NULL_HANDLE) 
    {
        tVertShaderModule = hg_create_shader_module(ptState, "../../out/shaders/vert.spv");
    }

    VkShaderModule tFragShaderModule = hg_create_shader_module(ptState, "../out/shaders/frag.spv");
    if(tFragShaderModule == VK_NULL_HANDLE) 
    {
        tFragShaderModule = hg_create_shader_module(ptState, "../../out/shaders/frag.spv");
    }

    if(tVertShaderModule == VK_NULL_HANDLE || tFragShaderModule == VK_NULL_HANDLE) 
    {
        printf("Failed to create shader modules!\n");
        exit(1);
    }

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


    VkVertexInputBindingDescription tBindingDescription = {
        .binding = 0,
        .stride = sizeof(hgVertex),
        .inputRate = VK_VERTEX_INPUT_RATE_VERTEX
    };

    VkVertexInputAttributeDescription atAttributeDescriptions[3] = {
        { // position
            .binding = 0,
            .location = 0,
            .format = VK_FORMAT_R32G32_SFLOAT,
            .offset = offsetof(hgVertex, x)
        },
        { // color
            .binding = 0,
            .location = 1,
            .format = VK_FORMAT_R32G32B32A32_SFLOAT,
            .offset = offsetof(hgVertex, r)
        },
        { // texcoord
            .binding = 0,
            .location = 2,
            .format = VK_FORMAT_R32G32_SFLOAT,
            .offset = offsetof(hgVertex, u)
        }
    };

    VkPipelineVertexInputStateCreateInfo tVertexInputInfo = {
        .sType                           = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        .vertexBindingDescriptionCount   = 1,
        .pVertexBindingDescriptions      = &tBindingDescription,
        .vertexAttributeDescriptionCount = 3,
        .pVertexAttributeDescriptions    = atAttributeDescriptions
    };

    VkPipelineInputAssemblyStateCreateInfo tInputAssembly = {
        .sType                  = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
        .topology               = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
        .primitiveRestartEnable = VK_FALSE
    };

    VkViewport tViewport = {
        .x        = 0.0f,
        .y        = 0.0f,
        .width    = (float)ptState->tSwapchainComponents.tExtent.width,
        .height   = (float)ptState->tSwapchainComponents.tExtent.height,
        .minDepth = 0.0f,
        .maxDepth = 1.0f
    };

    VkRect2D tScissor = {
        .offset = {0, 0},
        .extent = ptState->tSwapchainComponents.tExtent
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
        .cullMode                = VK_CULL_MODE_BACK_BIT,
        .frontFace               = VK_FRONT_FACE_CLOCKWISE,
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
        .blendEnable = VK_FALSE
    };

    VkPipelineColorBlendStateCreateInfo tColorBlending = {
        .sType           = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
        .logicOpEnable   = VK_FALSE,
        .logicOp         = VK_LOGIC_OP_COPY,
        .attachmentCount = 1,
        .pAttachments    = &tColorBlendAttachment,
        .blendConstants  = {0.0f, 0.0f, 0.0f, 0.0f}
    };

    // pipeline layout (empty for now)
    // Create descriptor set layout binding (same as in main())
    VkDescriptorSetLayoutBinding tTextureBinding = {
        .binding = 0,
        .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        .descriptorCount = 1,
        .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT
    };
    
    VkDescriptorSetLayoutCreateInfo tDescriptorLayoutInfo = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = 1,
        .pBindings = &tTextureBinding
    };
    
    VkDescriptorSetLayout tDescriptorSetLayout;
    VULKAN_CHECK(vkCreateDescriptorSetLayout(ptState->tContextComponents.tDevice, &tDescriptorLayoutInfo, NULL, &tDescriptorSetLayout));
    
    // Store it in resources so we can use it later
    ptState->tResources.tDescriptorSetLayout = tDescriptorSetLayout;
    
    // Update pipeline layout to include descriptor set
    VkDescriptorSetLayout setLayouts[] = {tDescriptorSetLayout};
    
    VkPipelineLayoutCreateInfo tPipelineLayoutInfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount = 1,
        .pSetLayouts = setLayouts,  // Include descriptor set layout!
        .pushConstantRangeCount = 0,
        .pPushConstantRanges = NULL
    };

    VULKAN_CHECK(vkCreatePipelineLayout(ptState->tContextComponents.tDevice, &tPipelineLayoutInfo, NULL, &ptState->tPipelineComponents.tPipelineLayout));

    // create graphics pipeline
    VkGraphicsPipelineCreateInfo tPipelineInfo = {
        .sType               = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        .stageCount          = 2, // IMPORTANT: Set to 2 for vertex+fragment shaders
        .pStages             = tShaderStages,
        .pVertexInputState   = &tVertexInputInfo,
        .pInputAssemblyState = &tInputAssembly,
        .pViewportState      = &tViewportState,
        .pRasterizationState = &tRasterizer,
        .pMultisampleState   = &tMultisampling,
        .pColorBlendState    = &tColorBlending,
        .layout              = ptState->tPipelineComponents.tPipelineLayout,
        .renderPass          = ptState->tPipelineComponents.tRenderPass,
        .subpass             = 0
    };

    VULKAN_CHECK(vkCreateGraphicsPipelines(ptState->tContextComponents.tDevice, VK_NULL_HANDLE, 1, &tPipelineInfo, NULL, &ptState->tPipelineComponents.tPipeline));

    // cleanup shader modules
    vkDestroyShaderModule(ptState->tContextComponents.tDevice, tVertShaderModule, NULL);
    vkDestroyShaderModule(ptState->tContextComponents.tDevice, tFragShaderModule, NULL);
}

void 
hg_create_framebuffers(hgAppData* ptState) 
{
    ptState->tPipelineComponents.tFramebuffers = malloc(ptState->tSwapchainComponents.uSwapchainImageCount * sizeof(VkFramebuffer));

    for(uint32_t i = 0; i < ptState->tSwapchainComponents.uSwapchainImageCount; i++) 
    {
        VkImageView attachments[] = {ptState->tSwapchainComponents.tSwapchainImageViews[i]};

        VkFramebufferCreateInfo tFramebufferInfo = {
            .sType           = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
            .renderPass      = ptState->tPipelineComponents.tRenderPass,
            .attachmentCount = 1,
            .pAttachments    = attachments,
            .width           = ptState->tSwapchainComponents.tExtent.width,
            .height          = ptState->tSwapchainComponents.tExtent.height,
            .layers          = 1
        };

        VULKAN_CHECK(vkCreateFramebuffer(ptState->tContextComponents.tDevice, &tFramebufferInfo, NULL, &ptState->tPipelineComponents.tFramebuffers[i]));
    }
}

// -----------------------------------------------------------------------------
// Command Buffer Management
// -----------------------------------------------------------------------------
void 
hg_create_command_pool(hgAppData* ptState) 
{
    VkCommandPoolCreateInfo tPoolInfo = {
        .sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .flags            = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
        .queueFamilyIndex = ptState->tContextComponents.tGraphicsQueueFamily
    };

    VULKAN_CHECK(vkCreateCommandPool(ptState->tContextComponents.tDevice, &tPoolInfo, NULL, &ptState->tCommandComponents.tCommandPool));
}

void 
hg_create_command_buffers(hgAppData* ptState) 
{
    ptState->tCommandComponents.tCommandBuffers = malloc(ptState->tSwapchainComponents.uSwapchainImageCount * sizeof(VkCommandBuffer));

    VkCommandBufferAllocateInfo tAllocInfo = {
        .sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool        = ptState->tCommandComponents.tCommandPool,
        .level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = ptState->tSwapchainComponents.uSwapchainImageCount
    };
    VULKAN_CHECK(vkAllocateCommandBuffers(ptState->tContextComponents.tDevice, &tAllocInfo, ptState->tCommandComponents.tCommandBuffers));

    // record command buffers
    for(uint32_t i = 0; i < ptState->tSwapchainComponents.uSwapchainImageCount; i++) 
    {
        VkCommandBufferBeginInfo tBeginInfo = {
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO
        };

        VULKAN_CHECK(vkBeginCommandBuffer(ptState->tCommandComponents.tCommandBuffers[i], &tBeginInfo));

        VkClearValue tClearColor = {{{0.0f, 0.0f, 0.0f, 1.0f}}};

        VkRenderPassBeginInfo tRenderPassInfo = {
            .sType       = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
            .renderPass  = ptState->tPipelineComponents.tRenderPass,
            .framebuffer = ptState->tPipelineComponents.tFramebuffers[i],
            .renderArea  = {
                .offset  = {0, 0},
                .extent  = ptState->tSwapchainComponents.tExtent
            },
            .clearValueCount = 1,
            .pClearValues    = &tClearColor
        };

        vkCmdBeginRenderPass(ptState->tCommandComponents.tCommandBuffers[i], &tRenderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
        vkCmdBindPipeline(ptState->tCommandComponents.tCommandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, ptState->tPipelineComponents.tPipeline);


        if(ptState->tResources.tDescriptorSets != NULL && ptState->tResources.tDescriptorSets[0] != VK_NULL_HANDLE) {
            vkCmdBindDescriptorSets(ptState->tCommandComponents.tCommandBuffers[i], 
                                   VK_PIPELINE_BIND_POINT_GRAPHICS,
                                   ptState->tPipelineComponents.tPipelineLayout,
                                   0,  // firstSet
                                   1,  // descriptorSetCount
                                   &ptState->tResources.tDescriptorSets[0],  // pDescriptorSets
                                   0,  // dynamicOffsetCount
                                   NULL);  // pDynamicOffsets
        }



        // bind vertex buffer
        VkBuffer vertexBuffers[] = {ptState->tResources.tVertexBuffer};
        VkDeviceSize offsets[] = {0};
        vkCmdBindVertexBuffers(ptState->tCommandComponents.tCommandBuffers[i], 0, 1, vertexBuffers, offsets);

        // bind index buffer
        vkCmdBindIndexBuffer(ptState->tCommandComponents.tCommandBuffers[i], ptState->tResources.tIndexBuffer, 0, VK_INDEX_TYPE_UINT16);

        vkCmdDrawIndexed(ptState->tCommandComponents.tCommandBuffers[i], 6, 1, 0, 0, 0); // draw quad
        vkCmdEndRenderPass(ptState->tCommandComponents.tCommandBuffers[i]);

        VULKAN_CHECK(vkEndCommandBuffer(ptState->tCommandComponents.tCommandBuffers[i]));
    }
}

// -----------------------------------------------------------------------------
// Synchronization Objects
// -----------------------------------------------------------------------------
void 
hg_create_sync_objects(hgAppData* ptState) 
{
    VkSemaphoreCreateInfo tSemaphoreInfo = {
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO
    };

    VkFenceCreateInfo tFenceInfo = {
        .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
        .flags = VK_FENCE_CREATE_SIGNALED_BIT
    };

    VULKAN_CHECK(vkCreateSemaphore(ptState->tContextComponents.tDevice, &tSemaphoreInfo, NULL, &ptState->tSyncComponents.tImageAvailable));
    VULKAN_CHECK(vkCreateSemaphore(ptState->tContextComponents.tDevice, &tSemaphoreInfo, NULL, &ptState->tSyncComponents.tRenderFinished));
    VULKAN_CHECK(vkCreateFence(ptState->tContextComponents.tDevice, &tFenceInfo, NULL, &ptState->tSyncComponents.tInFlight));
}

// -----------------------------------------------------------------------------
// Frame Rendering Loop
// -----------------------------------------------------------------------------
void 
hg_draw_frame(hgAppData* ptState) 
{
    // wait for previous frame to finish
    vkWaitForFences(ptState->tContextComponents.tDevice, 1, &ptState->tSyncComponents.tInFlight, VK_TRUE, UINT64_MAX);
    vkResetFences(ptState->tContextComponents.tDevice, 1, &ptState->tSyncComponents.tInFlight);

    // acquire next image
    uint32_t uImageIndex;
    vkAcquireNextImageKHR(ptState->tContextComponents.tDevice, ptState->tSwapchainComponents.tSwapchain, UINT64_MAX, 
                         ptState->tSyncComponents.tImageAvailable, VK_NULL_HANDLE, &uImageIndex);

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

    vkQueuePresentKHR(ptState->tContextComponents.tGraphicsQueue, &tPresentInfo);
}

// -----------------------------------------------------------------------------
// Resource Creation & Management
// -----------------------------------------------------------------------------

// -------------------------------
// Buffers & Memory
// -------------------------------
uint32_t 
hg_find_memory_type(hgVulkanContext* ptContextComponents, uint32_t uTypeFilter, VkMemoryPropertyFlags tProperties)
{
    VkPhysicalDeviceMemoryProperties tMemProperties;
    vkGetPhysicalDeviceMemoryProperties(ptContextComponents->tPhysicalDevice, &tMemProperties);

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
hg_create_buffer(hgVulkanContext* ptContextComponents, VkDeviceSize tSize, VkBufferUsageFlags tFlags, VkMemoryPropertyFlags tProperties, VkBuffer* ptBuffer, VkDeviceMemory* pMemory)
{
    // create buffer
    VkBufferCreateInfo tBufferCreateInfo = {
        .sType       = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size        = tSize,
        .usage       = tFlags,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE
    };
    VULKAN_CHECK(vkCreateBuffer(ptContextComponents->tDevice, &tBufferCreateInfo, NULL, ptBuffer));

    // get memory requirements
    VkMemoryRequirements tMemRequirements;
    vkGetBufferMemoryRequirements(ptContextComponents->tDevice, *ptBuffer, &tMemRequirements);

    // find suitable memory
    uint32_t tMemTypeIndex = hg_find_memory_type(ptContextComponents, tMemRequirements.memoryTypeBits, tProperties);

    // allocate and bind memory
    VkMemoryAllocateInfo tMemAllocInfo = {
        .sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize  = tMemRequirements.size,
        .memoryTypeIndex = tMemTypeIndex
    };
    VULKAN_CHECK(vkAllocateMemory(ptContextComponents->tDevice, &tMemAllocInfo, NULL, pMemory));
    VULKAN_CHECK(vkBindBufferMemory(ptContextComponents->tDevice, *ptBuffer, *pMemory, 0));
}

void hg_copy_buffer(hgVulkanContext* ptContextComponents, hgCommandResources* ptCommandComponents, VkBuffer tSrcBuffer, VkBuffer tDstBuffer, VkDeviceSize tSize)
{
    // create a temporary command buffer for the copy
    VkCommandBufferAllocateInfo tBufferAllocInfo = {
        .sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandPool        = ptCommandComponents->tCommandPool,
        .commandBufferCount = 1
    };

    VkCommandBuffer tCommandBuffer;
    VULKAN_CHECK(vkAllocateCommandBuffers(ptContextComponents->tDevice, &tBufferAllocInfo, &tCommandBuffer));

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

    // Submit and wait for completion
    VkSubmitInfo submitInfo = {
        .sType              = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .commandBufferCount = 1,
        .pCommandBuffers    = &tCommandBuffer
    };
    VULKAN_CHECK(vkQueueSubmit(ptContextComponents->tGraphicsQueue, 1, &submitInfo, VK_NULL_HANDLE));
    VULKAN_CHECK(vkQueueWaitIdle(ptContextComponents->tGraphicsQueue));  // wait for copy to finish

    vkFreeCommandBuffers(ptContextComponents->tDevice, ptCommandComponents->tCommandPool, 1, &tCommandBuffer);
}

// -------------------------------
// Vertex/Index Buffer Creation
// -------------------------------
void 
hg_create_quad_buffers(hgAppData* ptState)
{
    hgVertex atVertices[] = {
        {-0.5f, -0.5f,  1.0f, 1.0f, 1.0f, 1.0f,  0.0f, 0.0f},
        { 0.5f, -0.5f,  1.0f, 1.0f, 1.0f, 1.0f,  1.0f, 0.0f},
        { 0.5f,  0.5f,  1.0f, 1.0f, 1.0f, 1.0f,  1.0f, 1.0f},
        {-0.5f,  0.5f,  1.0f, 1.0f, 1.0f, 1.0f,  0.0f, 1.0f}
    };

    uint16_t atIndices[] = {0, 1, 2, 2, 3, 0};

    VkDeviceSize tVertexBufferSize = sizeof(atVertices);
    VkDeviceSize tIndexBufferSize = sizeof(atIndices);

    // vertex buffer
    VkBuffer tStagingVertexBuffer;
    VkDeviceMemory tStagingVertexBufferMemory;
    hg_create_buffer(&ptState->tContextComponents, 
        tVertexBufferSize, 
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT, 
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, 
        &tStagingVertexBuffer, 
        &tStagingVertexBufferMemory);

    void* pVertexData;
    vkMapMemory(ptState->tContextComponents.tDevice, tStagingVertexBufferMemory, 0, tVertexBufferSize, 0, &pVertexData);
    memcpy(pVertexData, atVertices, tVertexBufferSize);
    vkUnmapMemory(ptState->tContextComponents.tDevice, tStagingVertexBufferMemory);

    hg_create_buffer(&ptState->tContextComponents, 
        tVertexBufferSize,
        VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        &ptState->tResources.tVertexBuffer, 
        &ptState->tResources.tVertexBufferMemory);

    hg_copy_buffer(&ptState->tContextComponents, &ptState->tCommandComponents, 
        tStagingVertexBuffer, ptState->tResources.tVertexBuffer, tVertexBufferSize);

    vkDestroyBuffer(ptState->tContextComponents.tDevice, tStagingVertexBuffer, NULL);
    vkFreeMemory(ptState->tContextComponents.tDevice, tStagingVertexBufferMemory, NULL);
    ptState->tResources.szVertexBufferSize = tVertexBufferSize;

    // index buffer
    VkBuffer tStagingIndexBuffer;
    VkDeviceMemory tStagingIndexBufferMemory;
    hg_create_buffer(&ptState->tContextComponents, 
        tIndexBufferSize, 
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT, 
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, 
        &tStagingIndexBuffer, 
        &tStagingIndexBufferMemory);

    void* pIndexData;
    vkMapMemory(ptState->tContextComponents.tDevice, tStagingIndexBufferMemory, 0, tIndexBufferSize, 0, &pIndexData);
    memcpy(pIndexData, atIndices, tIndexBufferSize);
    vkUnmapMemory(ptState->tContextComponents.tDevice, tStagingIndexBufferMemory);

    hg_create_buffer(&ptState->tContextComponents, 
        tIndexBufferSize, 
        VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        &ptState->tResources.tIndexBuffer, 
        &ptState->tResources.tIndexBufferMemory);

    hg_copy_buffer(&ptState->tContextComponents, &ptState->tCommandComponents, 
        tStagingIndexBuffer, ptState->tResources.tIndexBuffer, tIndexBufferSize);

    vkDestroyBuffer(ptState->tContextComponents.tDevice, tStagingIndexBuffer, NULL);
    vkFreeMemory(ptState->tContextComponents.tDevice, tStagingIndexBufferMemory, NULL);
    ptState->tResources.szIndexBufferSize = tIndexBufferSize;
}

// -----------------------------------------------------------------------------
// Descriptor Management
// -----------------------------------------------------------------------------
void hg_create_descriptor_set(hgAppData* ptState)
{

};

// -----------------------------------------------------------------------------
// Textures
// -----------------------------------------------------------------------------

// CPU texture loading (uses STB)
unsigned char*
hg_load_texture_data(const char* pcFileName, int* iWidthOut, int* iHeightOut)
{
    int iComponentsInFile = 0;
    return stbi_load(pcFileName, iWidthOut, iHeightOut, &iComponentsInFile, 4);
}

// GPU texture creation and upload
hgTexture 
hg_create_texture(hgAppData* ptState, const unsigned char* pucData, int iWidth, int iHeight)
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
    VULKAN_CHECK(vkCreateImage(ptState->tContextComponents.tDevice, &tImageInfo, NULL, &tTexture.tImage));

    // allocate memory
    VkMemoryRequirements tMemRequirements;
    vkGetImageMemoryRequirements(ptState->tContextComponents.tDevice, tTexture.tImage, &tMemRequirements);

    VkMemoryAllocateInfo tAllocInfo = {
        .sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize  = tMemRequirements.size,
        .memoryTypeIndex = hg_find_memory_type(&ptState->tContextComponents, 
                                              tMemRequirements.memoryTypeBits,
                                              VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)
    };
    VULKAN_CHECK(vkAllocateMemory(ptState->tContextComponents.tDevice, &tAllocInfo, NULL, &tTexture.tMemory));
    VULKAN_CHECK(vkBindImageMemory(ptState->tContextComponents.tDevice, tTexture.tImage, tTexture.tMemory, 0));

    // upload texture data (using staging buffer)
    hg_upload_to_image(ptState, tTexture.tImage, pucData, iWidth, iHeight);

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
    VULKAN_CHECK(vkCreateImageView(ptState->tContextComponents.tDevice, &tViewInfo, NULL, &tTexture.tImageView));

    return tTexture;
}

// Texture cleanup
void 
hg_destroy_texture(hgAppData* ptState, hgTexture* tTexture)
{
    if(tTexture->tImageView != VK_NULL_HANDLE) vkDestroyImageView(ptState->tContextComponents.tDevice, tTexture->tImageView, NULL);
    if(tTexture->tImage != VK_NULL_HANDLE)     vkDestroyImage(ptState->tContextComponents.tDevice, tTexture->tImage, NULL);
    if(tTexture->tMemory != VK_NULL_HANDLE)    vkFreeMemory(ptState->tContextComponents.tDevice, tTexture->tMemory, NULL);

    memset(tTexture, 0, sizeof(hgTexture));
}

// Internal upload helper
void 
hg_upload_to_image(hgAppData* ptState, VkImage tImage, const unsigned char* pData, int iWidth, int iHeight)
{
    VkDeviceSize imageSize = iWidth * iHeight * 4; // RGBA8

    // create staging buffer
    VkBuffer       tStagingBuffer;
    VkDeviceMemory tStagingBufferMemory;
    hg_create_buffer(&ptState->tContextComponents, imageSize,
                     VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                     VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                     &tStagingBuffer, &tStagingBufferMemory);

    // copy data to staging buffer
    void* pMapped;
    vkMapMemory(ptState->tContextComponents.tDevice, tStagingBufferMemory, 0, imageSize, 0, &pMapped);
    memcpy(pMapped, pData, imageSize);
    vkUnmapMemory(ptState->tContextComponents.tDevice, tStagingBufferMemory);

    // record copy commands
    VkCommandBuffer tCmdBuffer = hg_begin_single_time_commands(ptState);

    // Transition to transfer dst
    VkImageSubresourceRange tSubResRan = {
        .aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT,
        .baseMipLevel   = 0,
        .levelCount     = 1,
        .baseArrayLayer = 0,
        .layerCount     = 1
    };

    hg_transition_image_layout(tCmdBuffer, 
                          tImage, 
                          VK_IMAGE_LAYOUT_UNDEFINED, 
                          VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                          tSubResRan,
                          VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                          VK_PIPELINE_STAGE_TRANSFER_BIT);

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
    hg_transition_image_layout(tCmdBuffer, tImage, 
                              VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 
                              VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                              tSubResRan,
                              VK_PIPELINE_STAGE_TRANSFER_BIT,
                              VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);

    hg_end_single_time_commands(ptState, tCmdBuffer);

    // cleanup staging
    vkDestroyBuffer(ptState->tContextComponents.tDevice, tStagingBuffer, NULL);
    vkFreeMemory(ptState->tContextComponents.tDevice, tStagingBufferMemory, NULL);
}

// -----------------------------------------------------------------------------
// Shader Management
// -----------------------------------------------------------------------------
VkShaderModule 
hg_create_shader_module(hgAppData* ptState, const char* pcFilename) 
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
    VULKAN_CHECK(vkCreateShaderModule(ptState->tContextComponents.tDevice, &tCreateInfo, NULL, &tShaderModule));

    free(puCode);
    return tShaderModule;
}

// -----------------------------------------------------------------------------
// Cleanup & Resource Destruction
// -----------------------------------------------------------------------------
void 
hg_cleanup(hgAppData* ptState) 
{

    // TODO: this is not a good resource clean up solution need to come up with better alternative
    vkDeviceWaitIdle(ptState->tContextComponents.tDevice);

    // cleanup vulkan resources
    if(ptState->tSyncComponents.tImageAvailable != VK_NULL_HANDLE) vkDestroySemaphore(ptState->tContextComponents.tDevice, ptState->tSyncComponents.tImageAvailable, NULL);
    if(ptState->tSyncComponents.tRenderFinished != VK_NULL_HANDLE) vkDestroySemaphore(ptState->tContextComponents.tDevice, ptState->tSyncComponents.tRenderFinished, NULL);
    if(ptState->tSyncComponents.tInFlight != VK_NULL_HANDLE)       vkDestroyFence(ptState->tContextComponents.tDevice, ptState->tSyncComponents.tInFlight, NULL);

    if(ptState->tCommandComponents.tCommandPool != VK_NULL_HANDLE) vkDestroyCommandPool(ptState->tContextComponents.tDevice, ptState->tCommandComponents.tCommandPool, NULL);

    for(uint32_t i = 0; i < ptState->tSwapchainComponents.uSwapchainImageCount; i++) 
    {
        if(ptState->tPipelineComponents.tFramebuffers[i] != VK_NULL_HANDLE)         vkDestroyFramebuffer(ptState->tContextComponents.tDevice, ptState->tPipelineComponents.tFramebuffers[i], NULL);
        if(ptState->tSwapchainComponents.tSwapchainImageViews[i] != VK_NULL_HANDLE) vkDestroyImageView(ptState->tContextComponents.tDevice, ptState->tSwapchainComponents.tSwapchainImageViews[i], NULL);
    }

    if(ptState->tPipelineComponents.tFramebuffers)         free(ptState->tPipelineComponents.tFramebuffers);
    if(ptState->tSwapchainComponents.tSwapchainImageViews) free(ptState->tSwapchainComponents.tSwapchainImageViews);
    if(ptState->tSwapchainComponents.tSwapchainImages)     free(ptState->tSwapchainComponents.tSwapchainImages);
    if(ptState->tCommandComponents.tCommandBuffers)        free(ptState->tCommandComponents.tCommandBuffers);

    if(ptState->tPipelineComponents.tPipeline != VK_NULL_HANDLE)       vkDestroyPipeline(ptState->tContextComponents.tDevice, ptState->tPipelineComponents.tPipeline, NULL);
    if(ptState->tPipelineComponents.tPipelineLayout != VK_NULL_HANDLE) vkDestroyPipelineLayout(ptState->tContextComponents.tDevice, ptState->tPipelineComponents.tPipelineLayout, NULL);
    if(ptState->tPipelineComponents.tRenderPass != VK_NULL_HANDLE)     vkDestroyRenderPass(ptState->tContextComponents.tDevice, ptState->tPipelineComponents.tRenderPass, NULL);
    if(ptState->tSwapchainComponents.tSwapchain != VK_NULL_HANDLE)     vkDestroySwapchainKHR(ptState->tContextComponents.tDevice, ptState->tSwapchainComponents.tSwapchain, NULL);
    if(ptState->tContextComponents.tDevice != VK_NULL_HANDLE)          vkDestroyDevice(ptState->tContextComponents.tDevice, NULL);
    if(ptState->tSwapchainComponents.tSurface != VK_NULL_HANDLE)       vkDestroySurfaceKHR(ptState->tContextComponents.tInstance, ptState->tSwapchainComponents.tSurface, NULL);
    if(ptState->tContextComponents.tInstance != VK_NULL_HANDLE)        vkDestroyInstance(ptState->tContextComponents.tInstance, NULL);

    if(ptState->tResources.tVertexBuffer != VK_NULL_HANDLE)       vkDestroyBuffer(ptState->tContextComponents.tDevice, ptState->tResources.tVertexBuffer, NULL);
    if(ptState->tResources.tVertexBufferMemory != VK_NULL_HANDLE) vkFreeMemory(ptState->tContextComponents.tDevice, ptState->tResources.tVertexBufferMemory, NULL);
    if(ptState->tResources.tIndexBuffer != VK_NULL_HANDLE)        vkDestroyBuffer(ptState->tContextComponents.tDevice, ptState->tResources.tIndexBuffer, NULL);
    if(ptState->tResources.tIndexBufferMemory != VK_NULL_HANDLE)  vkFreeMemory(ptState->tContextComponents.tDevice, ptState->tResources.tIndexBufferMemory, NULL);


    // TODO: temp for testing
    if(ptState->tResources.tDescriptorPool != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(ptState->tContextComponents.tDevice, ptState->tResources.tDescriptorPool, NULL);
    }

    if(ptState->tResources.tDescriptorSetLayout != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(ptState->tContextComponents.tDevice, ptState->tResources.tDescriptorSetLayout, NULL);
    }

    if(ptState->tResources.tDescriptorSets) {
        free(ptState->tResources.tDescriptorSets);
    }
}

// -----------------------------------------------------------------------------
// Helpers
// -----------------------------------------------------------------------------

// Single-time command buffer helpers
VkCommandBuffer 
hg_begin_single_time_commands(hgAppData* ptState) 
{
    VkCommandBufferAllocateInfo tAllocInfo = {
        .sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandPool        = ptState->tCommandComponents.tCommandPool,
        .commandBufferCount = 1
    };
    VkCommandBuffer tCommandBuffer;
    VULKAN_CHECK(vkAllocateCommandBuffers(ptState->tContextComponents.tDevice, &tAllocInfo, &tCommandBuffer));

    VkCommandBufferBeginInfo tBeginInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
    };
    VULKAN_CHECK(vkBeginCommandBuffer(tCommandBuffer, &tBeginInfo));
    return tCommandBuffer;
}

void 
hg_end_single_time_commands(hgAppData* ptState, VkCommandBuffer tCommandBuffer) 
{
    VULKAN_CHECK(vkEndCommandBuffer(tCommandBuffer));

    VkSubmitInfo tSubmitInfo = {
        .sType              = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .commandBufferCount = 1,
        .pCommandBuffers    = &tCommandBuffer
    };

    // submit and wait for completion
    VULKAN_CHECK(vkQueueSubmit(ptState->tContextComponents.tGraphicsQueue, 1, &tSubmitInfo, VK_NULL_HANDLE));
    VULKAN_CHECK(vkQueueWaitIdle(ptState->tContextComponents.tGraphicsQueue));

    // free the command buffer
    vkFreeCommandBuffers(ptState->tContextComponents.tDevice, ptState->tCommandComponents.tCommandPool, 1, &tCommandBuffer);
}

// Image layout transition helper
void 
hg_transition_image_layout(VkCommandBuffer tCommandBuffer, 
                          VkImage tImage, 
                          VkImageLayout tOldLayout, 
                          VkImageLayout tNewLayout, 
                          VkImageSubresourceRange tSubresourceRange, 
                          VkPipelineStageFlags tSrcStageMask, 
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

    vkCmdPipelineBarrier(tCommandBuffer, 
                        tSrcStageMask, 
                        tDstStageMask, 
                        0, 
                        0, NULL,   // Memory barriers
                        0, NULL,   // Buffer memory barriers
                        1, &tBarrier); // Image memory barriers
}