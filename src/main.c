// complete standalone vulkan windows application

#include <windows.h>
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_win32.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#define VK_CHECK(result) if((result) != VK_SUCCESS) { \
    printf("vulkan error at %s:%d: %d\n", __FILE__, __LINE__, (result)); \
    exit(1); \
}

typedef struct _tAppData
{
    HINSTANCE hInstance;
    HWND      hWnd;
    int       iWidth;
    int       iHeight;

    // vulkan handles
    VkInstance       tInstance;
    VkSurfaceKHR     tSurface;
    VkPhysicalDevice tPhysicalDevice;
    VkDevice         tDevice;
    VkQueue          tGraphicsQueue;
    uint32_t         uGraphicsQueueFamily;

    // swapchain
    VkSwapchainKHR tSwapchain;
    VkFormat       tSwapchainFormat;
    VkExtent2D     tSwapchainExtent;
    VkImage*       ptSwapchainImages;
    VkImageView*   ptSwapchainImageViews;
    VkFramebuffer* ptFramebuffers;
    uint32_t       uSwapchainImageCount;

    // rendering
    VkRenderPass     tRenderPass;
    VkPipelineLayout tPipelineLayout;
    VkPipeline       tGraphicsPipeline;

    // commands
    VkCommandPool    tCommandPool;
    VkCommandBuffer* ptCommandBuffers;

    // sync
    VkSemaphore tImageAvailableSemaphore;
    VkSemaphore tRenderFinishedSemaphore;
    VkFence     tInFlightFence;
} tAppData;

// -----------------------------------------------------------------------------
// function declarations
// -----------------------------------------------------------------------------

// TODO: order these in a more logical way i.e. group tasks and handle creation etc...
LRESULT CALLBACK WindowProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
void             CreateInstance(tAppData* ptState);
void             CreateSurface(tAppData* ptState);
void             PickPhysicalDevice(tAppData* ptState);
void             CreateLogicalDevice(tAppData* ptState);
void             CreateSwapchain(tAppData* ptState);
void             CreateRenderPass(tAppData* ptState);
void             CreateGraphicsPipeline(tAppData* ptState);
void             CreateFramebuffers(tAppData* ptState);
void             CreateCommandPool(tAppData* ptState);
void             CreateCommandBuffers(tAppData* ptState);
void             CreateSyncObjects(tAppData* ptState);
void             DrawFrame(tAppData* ptState);
void             Cleanup(tAppData* ptState);


// -----------------------------------------------------------------------------
// shaders
// -----------------------------------------------------------------------------
VkShaderModule CreateShaderModule(tAppData* ptState, const char* pcFilename);

// -----------------------------------------------------------------------------
// main entry point
// -----------------------------------------------------------------------------
int WINAPI 
WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) 
{
    (void)hPrevInstance; (void)lpCmdLine; // unused

    // 1. register window class
    const char CLASS_NAME[] = "VulkanWindowClass";

    WNDCLASSEX wc = {
        .cbSize        = sizeof(WNDCLASSEX),
        .style         = CS_HREDRAW | CS_VREDRAW,
        .lpfnWndProc   = WindowProc,
        .hInstance     = hInstance,
        .hCursor       = LoadCursor(NULL, IDC_ARROW),
        .hbrBackground = (HBRUSH)(COLOR_WINDOW + 1),
        .lpszClassName = CLASS_NAME
    };

    RegisterClassEx(&wc);

    // 2. create window
    HWND hWnd = CreateWindowEx(
        0, CLASS_NAME, "Vulkan Application",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 800, 600,
        NULL, NULL, hInstance, NULL
    );

    if (!hWnd) return 0;

    ShowWindow(hWnd, nCmdShow);
    UpdateWindow(hWnd);

    // 3. initialize app state
    tAppData tState  = {0};
    tState.hInstance = hInstance;
    tState.hWnd      = hWnd;
    tState.iWidth    = 800;
    tState.iHeight   = 600;

    // 4. initialize vulkan
    CreateInstance(&tState);
    CreateSurface(&tState);
    PickPhysicalDevice(&tState);
    CreateLogicalDevice(&tState);
    CreateSwapchain(&tState);
    CreateRenderPass(&tState);
    CreateGraphicsPipeline(&tState);
    CreateFramebuffers(&tState);
    CreateCommandPool(&tState);
    CreateCommandBuffers(&tState);
    CreateSyncObjects(&tState);


    // 5. main loop
    MSG msg = {0};
    while (msg.message != WM_QUIT) 
    {
        if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) 
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        } 
        else 
        {
            // render frame
            DrawFrame(&tState);
        }
    }

    // 6. cleanup
    Cleanup(&tState);

    return (int)msg.wParam;
}

// -----------------------------------------------------------------------------
// window procedure
// -----------------------------------------------------------------------------
LRESULT CALLBACK 
WindowProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) 
{
    switch (uMsg) 
    {
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
        case WM_SIZE: 
        {
            // handle resize - in a real app, recreate swapchain here
            return 0;
        }
        case WM_PAINT: 
        {
            PAINTSTRUCT ps;
            BeginPaint(hWnd, &ps);
            EndPaint(hWnd, &ps);
            return 0;
        }
        default:
            return DefWindowProc(hWnd, uMsg, wParam, lParam);
    }
}

// -----------------------------------------------------------------------------
// vulkan initialization
// -----------------------------------------------------------------------------

void 
CreateInstance(tAppData* ptState) 
{
    VkApplicationInfo tAppInfo = {
        .sType              = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pApplicationName   = "Vulkan App",
        .applicationVersion = VK_MAKE_VERSION(1, 0, 0),
        .pEngineName        = "No Engine",
        .engineVersion      = VK_MAKE_VERSION(1, 0, 0),
        .apiVersion         = VK_API_VERSION_1_0
    };

    const char* extensions[] = {
        VK_KHR_SURFACE_EXTENSION_NAME,
        VK_KHR_WIN32_SURFACE_EXTENSION_NAME
    };

    VkInstanceCreateInfo tCreateInfo = {
        .sType                   = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pApplicationInfo        = &tAppInfo,
        .enabledExtensionCount   = 2,
        .ppEnabledExtensionNames = extensions
    };

    VK_CHECK(vkCreateInstance(&tCreateInfo, NULL, &ptState->tInstance));
}

void 
CreateSurface(tAppData* ptState) 
{
    VkWin32SurfaceCreateInfoKHR tCreateInfo = {
        .sType     = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR,
        .hinstance = ptState->hInstance,
        .hwnd      = ptState->hWnd
    };

    VK_CHECK(vkCreateWin32SurfaceKHR(ptState->tInstance, &tCreateInfo, NULL, &ptState->tSurface));
}

void 
PickPhysicalDevice(tAppData* ptState) 
{
    uint32_t uDeviceCount = 0;
    vkEnumeratePhysicalDevices(ptState->tInstance, &uDeviceCount, NULL);
    assert(uDeviceCount > 0);

    VkPhysicalDevice* pDevices = malloc(uDeviceCount * sizeof(VkPhysicalDevice));
    vkEnumeratePhysicalDevices(ptState->tInstance, &uDeviceCount, pDevices);

    // just pick the first gpu
    ptState->tPhysicalDevice = pDevices[0];

    free(pDevices);
}

void 
CreateLogicalDevice(tAppData* ptState) 
{
    // find queue family that supports graphics and presentation
    uint32_t uQueueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(ptState->tPhysicalDevice, &uQueueFamilyCount, NULL);
    VkQueueFamilyProperties* pQueueFamilies = malloc(uQueueFamilyCount * sizeof(VkQueueFamilyProperties));
    vkGetPhysicalDeviceQueueFamilyProperties(ptState->tPhysicalDevice, &uQueueFamilyCount, pQueueFamilies);

    // find graphics queue family
    ptState->uGraphicsQueueFamily = UINT32_MAX;
    for (uint32_t i = 0; i < uQueueFamilyCount; i++) 
    {
        if (pQueueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) 
        {
            VkBool32 bPresentSupport = VK_FALSE;
            vkGetPhysicalDeviceSurfaceSupportKHR(ptState->tPhysicalDevice, i, ptState->tSurface, &bPresentSupport);
            if (bPresentSupport) 
            {
                ptState->uGraphicsQueueFamily = i;
                break;
            }
        }
    }
    assert(ptState->uGraphicsQueueFamily != UINT32_MAX);

    free(pQueueFamilies);

    float fQueuePriority = 1.0f;
    VkDeviceQueueCreateInfo tQueueCreateInfo = {
        .sType            = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
        .queueFamilyIndex = ptState->uGraphicsQueueFamily,
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

    VK_CHECK(vkCreateDevice(ptState->tPhysicalDevice, &tDeviceCreateInfo, NULL, &ptState->tDevice));
    vkGetDeviceQueue(ptState->tDevice, ptState->uGraphicsQueueFamily, 0, &ptState->tGraphicsQueue);
}

void
CreateSwapchain(tAppData* ptState) 
{
    // get surface capabilities
    VkSurfaceCapabilitiesKHR tCapabilities;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(ptState->tPhysicalDevice, ptState->tSurface, &tCapabilities);

    // get surface formats
    uint32_t uFormatCount;
    vkGetPhysicalDeviceSurfaceFormatsKHR(ptState->tPhysicalDevice, ptState->tSurface, &uFormatCount, NULL);
    VkSurfaceFormatKHR* pFormats = malloc(uFormatCount * sizeof(VkSurfaceFormatKHR));
    vkGetPhysicalDeviceSurfaceFormatsKHR(ptState->tPhysicalDevice, ptState->tSurface, &uFormatCount, pFormats);

    // choose surface format (prefer B8G8R8A8_UNORM with sRGB)
    ptState->tSwapchainFormat = VK_FORMAT_B8G8R8A8_UNORM;
    for (uint32_t i = 0; i < uFormatCount; i++) 
    {
        if (pFormats[i].format == VK_FORMAT_B8G8R8A8_UNORM && 
            pFormats[i].colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) 
        {
            ptState->tSwapchainFormat = pFormats[i].format;
            break;
        }
    }

    // choose swap extent
    if (tCapabilities.currentExtent.width != UINT32_MAX) 
    {
        ptState->tSwapchainExtent = tCapabilities.currentExtent;
    } 
    else 
    {
        VkExtent2D tActualExtent = {ptState->iWidth, ptState->iHeight};
        tActualExtent.width = (tActualExtent.width < tCapabilities.minImageExtent.width) ? 
            tCapabilities.minImageExtent.width : (tActualExtent.width > tCapabilities.maxImageExtent.width) ? 
            tCapabilities.maxImageExtent.width : tActualExtent.width;
        tActualExtent.height = (tActualExtent.height < tCapabilities.minImageExtent.height) ? 
            tCapabilities.minImageExtent.height : (tActualExtent.height > tCapabilities.maxImageExtent.height) ? 
            tCapabilities.maxImageExtent.height : tActualExtent.height;
        ptState->tSwapchainExtent = tActualExtent;
    }

    // choose image count
    uint32_t uImageCount = tCapabilities.minImageCount + 1;
    if (tCapabilities.maxImageCount > 0 && uImageCount > tCapabilities.maxImageCount) 
    {
        uImageCount = tCapabilities.maxImageCount;
    }

    // create swapchain
    VkSwapchainCreateInfoKHR tCreateInfo = {
        .sType            = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
        .surface          = ptState->tSurface,
        .minImageCount    = uImageCount,
        .imageFormat      = ptState->tSwapchainFormat,
        .imageColorSpace  = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR,
        .imageExtent      = ptState->tSwapchainExtent,
        .imageArrayLayers = 1,
        .imageUsage       = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
        .imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .preTransform     = tCapabilities.currentTransform,
        .compositeAlpha   = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
        .presentMode      = VK_PRESENT_MODE_FIFO_KHR,
        .clipped          = VK_TRUE
    };
    VK_CHECK(vkCreateSwapchainKHR(ptState->tDevice, &tCreateInfo, NULL, &ptState->tSwapchain));

    // get swapchain images
    vkGetSwapchainImagesKHR(ptState->tDevice, ptState->tSwapchain, &ptState->uSwapchainImageCount, NULL);
    ptState->ptSwapchainImages = malloc(ptState->uSwapchainImageCount * sizeof(VkImage));
    vkGetSwapchainImagesKHR(ptState->tDevice, ptState->tSwapchain, &ptState->uSwapchainImageCount, ptState->ptSwapchainImages);

    // create image views
    ptState->ptSwapchainImageViews = malloc(ptState->uSwapchainImageCount * sizeof(VkImageView));
    for (uint32_t i = 0; i < ptState->uSwapchainImageCount; i++) 
    {
        VkImageViewCreateInfo tViewCreateInfo = {
            .sType      = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .image      = ptState->ptSwapchainImages[i],
            .viewType   = VK_IMAGE_VIEW_TYPE_2D,
            .format     = ptState->tSwapchainFormat,
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
        VK_CHECK(vkCreateImageView(ptState->tDevice, &tViewCreateInfo, NULL, &ptState->ptSwapchainImageViews[i]));
    }
    free(pFormats);
}

void 
CreateRenderPass(tAppData* ptState) 
{
    VkAttachmentDescription tColorAttachment = {
        .format         = ptState->tSwapchainFormat,
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

    VK_CHECK(vkCreateRenderPass(ptState->tDevice, &tRenderPassInfo, NULL, &ptState->tRenderPass));
}

void 
CreateGraphicsPipeline(tAppData* ptState) 
{
    printf("Loading shaders...\n");

    // Try different paths
    VkShaderModule tVertShaderModule = CreateShaderModule(ptState, "../out/shaders/vert.spv");
    if (tVertShaderModule == VK_NULL_HANDLE) 
    {
        tVertShaderModule = CreateShaderModule(ptState, "../../out/shaders/vert.spv");
    }

    VkShaderModule tFragShaderModule = CreateShaderModule(ptState, "../out/shaders/frag.spv");
    if (tFragShaderModule == VK_NULL_HANDLE) 
    {
        tFragShaderModule = CreateShaderModule(ptState, "../../out/shaders/frag.spv");
    }

    if (tVertShaderModule == VK_NULL_HANDLE || tFragShaderModule == VK_NULL_HANDLE) 
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

    // Fixed function states (keep your existing code)
    VkPipelineVertexInputStateCreateInfo tVertexInputInfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO
        // No vertex inputs for now - we'll add your vertex buffers later
    };

    VkPipelineInputAssemblyStateCreateInfo tInputAssembly = {
        .sType                  = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
        .topology               = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
        .primitiveRestartEnable = VK_FALSE
    };

    VkViewport tViewport = {
        .x        = 0.0f,
        .y        = 0.0f,
        .width    = (float)ptState->tSwapchainExtent.width,
        .height   = (float)ptState->tSwapchainExtent.height,
        .minDepth = 0.0f,
        .maxDepth = 1.0f
    };

    VkRect2D tScissor = {
        .offset = {0, 0},
        .extent = ptState->tSwapchainExtent
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
        .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | 
                         VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
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

    // Pipeline layout (empty for now)
    VkPipelineLayoutCreateInfo tPipelineLayoutInfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO
    };

    VK_CHECK(vkCreatePipelineLayout(ptState->tDevice, &tPipelineLayoutInfo, NULL, &ptState->tPipelineLayout));

    // Create graphics pipeline
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
        .layout              = ptState->tPipelineLayout,
        .renderPass          = ptState->tRenderPass,
        .subpass             = 0
    };

    VK_CHECK(vkCreateGraphicsPipelines(ptState->tDevice, VK_NULL_HANDLE, 1, &tPipelineInfo, NULL, &ptState->tGraphicsPipeline));

    // Cleanup shader modules (they're copied into the pipeline)
    vkDestroyShaderModule(ptState->tDevice, tVertShaderModule, NULL);
    vkDestroyShaderModule(ptState->tDevice, tFragShaderModule, NULL);
}

void 
CreateFramebuffers(tAppData* ptState) 
{
    ptState->ptFramebuffers = malloc(ptState->uSwapchainImageCount * sizeof(VkFramebuffer));

    for (uint32_t i = 0; i < ptState->uSwapchainImageCount; i++) 
    {
        VkImageView attachments[] = {ptState->ptSwapchainImageViews[i]};

        VkFramebufferCreateInfo tFramebufferInfo = {
            .sType           = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
            .renderPass      = ptState->tRenderPass,
            .attachmentCount = 1,
            .pAttachments    = attachments,
            .width           = ptState->tSwapchainExtent.width,
            .height          = ptState->tSwapchainExtent.height,
            .layers          = 1
        };

        VK_CHECK(vkCreateFramebuffer(ptState->tDevice, &tFramebufferInfo, NULL, &ptState->ptFramebuffers[i]));
    }
}

void 
CreateCommandPool(tAppData* ptState) 
{
    VkCommandPoolCreateInfo tPoolInfo = {
        .sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .flags            = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
        .queueFamilyIndex = ptState->uGraphicsQueueFamily
    };

    VK_CHECK(vkCreateCommandPool(ptState->tDevice, &tPoolInfo, NULL, &ptState->tCommandPool));
}

void 
CreateCommandBuffers(tAppData* ptState) 
{
    ptState->ptCommandBuffers = malloc(ptState->uSwapchainImageCount * sizeof(VkCommandBuffer));

    VkCommandBufferAllocateInfo tAllocInfo = {
        .sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool        = ptState->tCommandPool,
        .level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = ptState->uSwapchainImageCount
    };

    VK_CHECK(vkAllocateCommandBuffers(ptState->tDevice, &tAllocInfo, ptState->ptCommandBuffers));

    // record command buffers
    for (uint32_t i = 0; i < ptState->uSwapchainImageCount; i++) 
    {
        VkCommandBufferBeginInfo tBeginInfo = {
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO
        };

        VK_CHECK(vkBeginCommandBuffer(ptState->ptCommandBuffers[i], &tBeginInfo));

        VkClearValue tClearColor = {{{0.0f, 0.0f, 0.0f, 1.0f}}};

        VkRenderPassBeginInfo tRenderPassInfo = {
            .sType       = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
            .renderPass  = ptState->tRenderPass,
            .framebuffer = ptState->ptFramebuffers[i],
            .renderArea  = {
                .offset  = {0, 0},
                .extent  = ptState->tSwapchainExtent
            },
            .clearValueCount = 1,
            .pClearValues    = &tClearColor
        };

        vkCmdBeginRenderPass(ptState->ptCommandBuffers[i], &tRenderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
        vkCmdBindPipeline(ptState->ptCommandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, ptState->tGraphicsPipeline);
        vkCmdDraw(ptState->ptCommandBuffers[i], 3, 1, 0, 0); // draw triangle
        vkCmdEndRenderPass(ptState->ptCommandBuffers[i]);

        VK_CHECK(vkEndCommandBuffer(ptState->ptCommandBuffers[i]));
    }
}

void 
CreateSyncObjects(tAppData* ptState) 
{
    VkSemaphoreCreateInfo tSemaphoreInfo = {
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO
    };

    VkFenceCreateInfo tFenceInfo = {
        .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
        .flags = VK_FENCE_CREATE_SIGNALED_BIT
    };

    VK_CHECK(vkCreateSemaphore(ptState->tDevice, &tSemaphoreInfo, NULL, &ptState->tImageAvailableSemaphore));
    VK_CHECK(vkCreateSemaphore(ptState->tDevice, &tSemaphoreInfo, NULL, &ptState->tRenderFinishedSemaphore));
    VK_CHECK(vkCreateFence(ptState->tDevice, &tFenceInfo, NULL, &ptState->tInFlightFence));
}

void 
DrawFrame(tAppData* ptState) 
{
    // wait for previous frame to finish
    vkWaitForFences(ptState->tDevice, 1, &ptState->tInFlightFence, VK_TRUE, UINT64_MAX);
    vkResetFences(ptState->tDevice, 1, &ptState->tInFlightFence);

    // acquire next image
    uint32_t uImageIndex;
    vkAcquireNextImageKHR(ptState->tDevice, ptState->tSwapchain, UINT64_MAX, 
                         ptState->tImageAvailableSemaphore, VK_NULL_HANDLE, &uImageIndex);

    // submit command buffer
    VkSubmitInfo tSubmitInfo = {
        .sType                = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .waitSemaphoreCount   = 1,
        .pWaitSemaphores      = &ptState->tImageAvailableSemaphore,
        .pWaitDstStageMask    = (VkPipelineStageFlags[]){VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT},
        .commandBufferCount   = 1,
        .pCommandBuffers      = &ptState->ptCommandBuffers[uImageIndex],
        .signalSemaphoreCount = 1,
        .pSignalSemaphores    = &ptState->tRenderFinishedSemaphore
    };

    VK_CHECK(vkQueueSubmit(ptState->tGraphicsQueue, 1, &tSubmitInfo, ptState->tInFlightFence));

    // present
    VkPresentInfoKHR tPresentInfo = {
        .sType              = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores    = &ptState->tRenderFinishedSemaphore,
        .swapchainCount     = 1,
        .pSwapchains        = &ptState->tSwapchain,
        .pImageIndices      = &uImageIndex
    };

    vkQueuePresentKHR(ptState->tGraphicsQueue, &tPresentInfo);
}

void 
Cleanup(tAppData* ptState) 
{
    vkDeviceWaitIdle(ptState->tDevice);

    // cleanup vulkan resources
    vkDestroySemaphore(ptState->tDevice, ptState->tImageAvailableSemaphore, NULL);
    vkDestroySemaphore(ptState->tDevice, ptState->tRenderFinishedSemaphore, NULL);
    vkDestroyFence(ptState->tDevice, ptState->tInFlightFence, NULL);

    vkDestroyCommandPool(ptState->tDevice, ptState->tCommandPool, NULL);

    for (uint32_t i = 0; i < ptState->uSwapchainImageCount; i++) 
    {
        vkDestroyFramebuffer(ptState->tDevice, ptState->ptFramebuffers[i], NULL);
        vkDestroyImageView(ptState->tDevice, ptState->ptSwapchainImageViews[i], NULL);
    }

    free(ptState->ptFramebuffers);
    free(ptState->ptSwapchainImageViews);
    free(ptState->ptSwapchainImages);
    free(ptState->ptCommandBuffers);

    vkDestroyPipeline(ptState->tDevice, ptState->tGraphicsPipeline, NULL);
    vkDestroyPipelineLayout(ptState->tDevice, ptState->tPipelineLayout, NULL);
    vkDestroyRenderPass(ptState->tDevice, ptState->tRenderPass, NULL);
    vkDestroySwapchainKHR(ptState->tDevice, ptState->tSwapchain, NULL);
    vkDestroyDevice(ptState->tDevice, NULL);
    vkDestroySurfaceKHR(ptState->tInstance, ptState->tSurface, NULL);
    vkDestroyInstance(ptState->tInstance, NULL);
}

VkShaderModule 
CreateShaderModule(tAppData* ptState, const char* pcFilename) 
{
    // Try multiple possible paths
    const char* apcPossiblePaths[] = {
        pcFilename,                         // absolute path
        "../out/shaders/vert.spv",          // relative from src folder
        "../../out/shaders/vert.spv",       // relative if running from project root
        "out/shaders/vert.spv",             // relative if running from project root
        NULL
    };

    FILE* pFile = NULL;
    const char* pcFoundPath = NULL;

    for (int i = 0; apcPossiblePaths[i] != NULL; i++) 
    {
        pFile = fopen(apcPossiblePaths[i], "rb");
        if (pFile) 
        {
            pcFoundPath = apcPossiblePaths[i];
            printf("Found shader at: %s\n", pcFoundPath);
            break;
        }
    }

    if (!pFile) 
    {
        printf("Failed to open shader file: %s\n", pcFilename);
        printf("Tried paths:\n");
        for (int i = 0; apcPossiblePaths[i] != NULL; i++) 
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
    VK_CHECK(vkCreateShaderModule(ptState->tDevice, &tCreateInfo, NULL, &tShaderModule));

    free(puCode);
    return tShaderModule;
}