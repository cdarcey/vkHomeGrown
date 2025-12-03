// Complete standalone Vulkan Windows application


#include <windows.h>
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_win32.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#define VK_CHECK(result) if((result) != VK_SUCCESS) { \
    printf("Vulkan error at %s:%d: %d\n", __FILE__, __LINE__, (result)); \
    exit(1); \
}

typedef struct _tAppData{
    HINSTANCE hinstance;
    HWND      hwnd;
    int       iWidth;
    int       iHeight;

    // Vulkan handles
    VkInstance       tVkInstance;
    VkSurfaceKHR     tVkSurface;
    VkPhysicalDevice tPhysicalDevice;
    VkDevice         tDevice;
    VkQueue          tGraphicsQueue;
    uint32_t         tGraphicsQueueFamily;

    // Swapchain
    VkSwapchainKHR tSwapchain;
    VkFormat       tSwapchainFormat;
    VkExtent2D     tSwapchainExtent;
    VkImage*       tSwapchainImages;
    VkImageView*   tSwapchainImageViews;
    VkFramebuffer* tFramebuffers;
    uint32_t       tSwapchainImageCount;

    // Rendering
    VkRenderPass     tRenderPass;
    VkPipelineLayout tPipelineLayout;
    VkPipeline       tGraphicsPipeline;
    
    // Commands
    VkCommandPool    tCommandPool;
    VkCommandBuffer* tCommandBuffers;
    
    // Sync
    VkSemaphore tImageAvailableSemaphore;
    VkSemaphore tRenderFinishedSemaphore;
    VkFence     tInFlightFence;
} tAppData;

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
void createInstance(AppState* state);
void createSurface(AppState* state);
void pickPhysicalDevice(AppState* state);
void createLogicalDevice(AppState* state);
void createSwapchain(AppState* state);
void createRenderPass(AppState* state);
void createGraphicsPipeline(AppState* state);
void createFramebuffers(AppState* state);
void createCommandPool(AppState* state);
void createCommandBuffers(AppState* state);
void createSyncObjects(AppState* state);
void drawFrame(AppState* state);
void cleanup(AppState* state)


// -----------------------------------------------------------------------------
// Main Entry Point
// -----------------------------------------------------------------------------
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, 
                   LPSTR lpCmdLine, int nCmdShow) {
    (void)hPrevInstance; (void)lpCmdLine; // Unused
    
    // 1. Register window class
    const char CLASS_NAME[] = "VulkanWindowClass";
    
    WNDCLASSEX wc = {
        .cbSize = sizeof(WNDCLASSEX),
        .style = CS_HREDRAW | CS_VREDRAW,
        .lpfnWndProc = WindowProc,
        .hInstance = hInstance,
        .hCursor = LoadCursor(NULL, IDC_ARROW),
        .hbrBackground = (HBRUSH)(COLOR_WINDOW + 1),
        .lpszClassName = CLASS_NAME
    };
    
    RegisterClassEx(&wc);
    
    // 2. Create window
    HWND hwnd = CreateWindowEx(
        0, CLASS_NAME, "Vulkan Application",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 800, 600,
        NULL, NULL, hInstance, NULL
    );
    
    if (!hwnd) return 0;
    
    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);
    
    // 3. Initialize app state
    AppState state = {0};
    state.hinstance = hInstance;
    state.hwnd = hwnd;
    state.width = 800;
    state.height = 600;
    
    // 4. Initialize Vulkan
    printf("Creating Vulkan instance...\n");
    createInstance(&state);
    
    printf("Creating surface...\n");
    createSurface(&state);
    
    printf("Picking physical device...\n");
    pickPhysicalDevice(&state);
    
    printf("Creating logical device...\n");
    createLogicalDevice(&state);
    
    printf("Creating swapchain...\n");
    createSwapchain(&state);
    
    printf("Creating render pass...\n");
    createRenderPass(&state);
    
    printf("Creating graphics pipeline...\n");
    createGraphicsPipeline(&state);
    
    printf("Creating framebuffers...\n");
    createFramebuffers(&state);
    
    printf("Creating command pool...\n");
    createCommandPool(&state);
    
    printf("Creating command buffers...\n");
    createCommandBuffers(&state);
    
    printf("Creating sync objects...\n");
    createSyncObjects(&state);
    
    printf("Vulkan initialization complete!\n");
    
    // 5. Main loop
    MSG msg = {0};
    while (msg.message != WM_QUIT) {
        if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        } else {
            // Render frame
            drawFrame(&state);
        }
    }
    
    // Cleanup
    printf("Cleaning up...\n");
    cleanup(&state);

    return (int)msg.wParam;
}

// -----------------------------------------------------------------------------
// Window Procedure
// -----------------------------------------------------------------------------
LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) 
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
            BeginPaint(hwnd, &ps);
            EndPaint(hwnd, &ps);
            return 0;
        }
        default:
            return DefWindowProc(hwnd, uMsg, wParam, lParam);
    }
}

// -----------------------------------------------------------------------------
// Vulkan Initialization
// -----------------------------------------------------------------------------

void createInstance(AppState* state) 
{
    VkApplicationInfo appInfo = {
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pApplicationName = "Vulkan App",
        .applicationVersion = VK_MAKE_VERSION(1, 0, 0),
        .pEngineName = "No Engine",
        .engineVersion = VK_MAKE_VERSION(1, 0, 0),
        .apiVersion = VK_API_VERSION_1_0
    };
    
    const char* extensions[] = {
        VK_KHR_SURFACE_EXTENSION_NAME,
        VK_KHR_WIN32_SURFACE_EXTENSION_NAME
    };
    
    VkInstanceCreateInfo createInfo = {
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pApplicationInfo = &appInfo,
        .enabledExtensionCount = 2,
        .ppEnabledExtensionNames = extensions
    };
    
    VK_CHECK(vkCreateInstance(&createInfo, NULL, &state->instance));
}

void createSurface(AppState* state) 
{
    VkWin32SurfaceCreateInfoKHR createInfo = {
        .sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR,
        .hinstance = state->hinstance,
        .hwnd = state->hwnd
    };
    
    VK_CHECK(vkCreateWin32SurfaceKHR(state->instance, &createInfo, NULL, &state->surface));
}

void pickPhysicalDevice(AppState* state) {
    uint32_t deviceCount = 0;
    vkEnumeratePhysicalDevices(state->instance, &deviceCount, NULL);
    assert(deviceCount > 0);
    
    VkPhysicalDevice* devices = malloc(deviceCount * sizeof(VkPhysicalDevice));
    vkEnumeratePhysicalDevices(state->instance, &deviceCount, devices);
    
    // Just pick the first GPU
    state->physicalDevice = devices[0];
    
    free(devices);
}

void createLogicalDevice(AppState* state) {
    // Find queue family that supports graphics and presentation
    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(state->physicalDevice, &queueFamilyCount, NULL);
    VkQueueFamilyProperties* queueFamilies = malloc(queueFamilyCount * sizeof(VkQueueFamilyProperties));
    vkGetPhysicalDeviceQueueFamilyProperties(state->physicalDevice, &queueFamilyCount, queueFamilies);
    
    // Find graphics queue family
    state->graphicsQueueFamily = UINT32_MAX;
    for (uint32_t i = 0; i < queueFamilyCount; i++) {
        if (queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            VkBool32 presentSupport = VK_FALSE;
            vkGetPhysicalDeviceSurfaceSupportKHR(state->physicalDevice, i, state->surface, &presentSupport);
            if (presentSupport) {
                state->graphicsQueueFamily = i;
                break;
            }
        }
    }
    assert(state->graphicsQueueFamily != UINT32_MAX);
    
    free(queueFamilies);
    
    float queuePriority = 1.0f;
    VkDeviceQueueCreateInfo queueCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
        .queueFamilyIndex = state->graphicsQueueFamily,
        .queueCount = 1,
        .pQueuePriorities = &queuePriority
    };
    
    const char* deviceExtensions[] = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};
    VkDeviceCreateInfo deviceCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .queueCreateInfoCount = 1,
        .pQueueCreateInfos = &queueCreateInfo,
        .enabledExtensionCount = 1,
        .ppEnabledExtensionNames = deviceExtensions
    };
    
    VK_CHECK(vkCreateDevice(state->physicalDevice, &deviceCreateInfo, NULL, &state->device));
    vkGetDeviceQueue(state->device, state->graphicsQueueFamily, 0, &state->graphicsQueue));
}

void createSwapchain(AppState* state) {
    // Get surface capabilities
    VkSurfaceCapabilitiesKHR capabilities;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(state->physicalDevice, state->surface, &capabilities);
    
    // Get surface formats
    uint32_t formatCount;
    vkGetPhysicalDeviceSurfaceFormatsKHR(state->physicalDevice, state->surface, &formatCount, NULL);
    VkSurfaceFormatKHR* formats = malloc(formatCount * sizeof(VkSurfaceFormatKHR));
    vkGetPhysicalDeviceSurfaceFormatsKHR(state->physicalDevice, state->surface, &formatCount, formats);
    
    // Choose surface format (prefer B8G8R8A8_UNORM with SRGB)
    state->swapchainFormat = VK_FORMAT_B8G8R8A8_UNORM;
    for (uint32_t i = 0; i < formatCount; i++) {
        if (formats[i].format == VK_FORMAT_B8G8R8A8_UNORM && 
            formats[i].colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            state->swapchainFormat = formats[i].format;
            break;
        }
    }
    
    // Choose swap extent
    if (capabilities.currentExtent.width != UINT32_MAX) {
        state->swapchainExtent = capabilities.currentExtent;
    } else {
        VkExtent2D actualExtent = {state->width, state->height};
        actualExtent.width = (actualExtent.width < capabilities.minImageExtent.width) ? 
            capabilities.minImageExtent.width : (actualExtent.width > capabilities.maxImageExtent.width) ? 
            capabilities.maxImageExtent.width : actualExtent.width;
        actualExtent.height = (actualExtent.height < capabilities.minImageExtent.height) ? 
            capabilities.minImageExtent.height : (actualExtent.height > capabilities.maxImageExtent.height) ? 
            capabilities.maxImageExtent.height : actualExtent.height;
        state->swapchainExtent = actualExtent;
    }
    
    // Choose image count
    uint32_t imageCount = capabilities.minImageCount + 1;
    if (capabilities.maxImageCount > 0 && imageCount > capabilities.maxImageCount) {
        imageCount = capabilities.maxImageCount;
    }
    
    // Create swapchain
    VkSwapchainCreateInfoKHR createInfo = {
        .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
        .surface = state->surface,
        .minImageCount = imageCount,
        .imageFormat = state->swapchainFormat,
        .imageColorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR,
        .imageExtent = state->swapchainExtent,
        .imageArrayLayers = 1,
        .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
        .imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .preTransform = capabilities.currentTransform,
        .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
        .presentMode = VK_PRESENT_MODE_FIFO_KHR,
        .clipped = VK_TRUE
    };
    
    VK_CHECK(vkCreateSwapchainKHR(state->device, &createInfo, NULL, &state->swapchain));
    
    // Get swapchain images
    vkGetSwapchainImagesKHR(state->device, state->swapchain, &state->swapchainImageCount, NULL);
    state->swapchainImages = malloc(state->swapchainImageCount * sizeof(VkImage));
    vkGetSwapchainImagesKHR(state->device, state->swapchain, &state->swapchainImageCount, state->swapchainImages);
    
    // Create image views
    state->swapchainImageViews = malloc(state->swapchainImageCount * sizeof(VkImageView));
    for (uint32_t i = 0; i < state->swapchainImageCount; i++) {
        VkImageViewCreateInfo viewCreateInfo = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .image = state->swapchainImages[i],
            .viewType = VK_IMAGE_VIEW_TYPE_2D,
            .format = state->swapchainFormat,
            .components = {
                .r = VK_COMPONENT_SWIZZLE_IDENTITY,
                .g = VK_COMPONENT_SWIZZLE_IDENTITY,
                .b = VK_COMPONENT_SWIZZLE_IDENTITY,
                .a = VK_COMPONENT_SWIZZLE_IDENTITY
            },
            .subresourceRange = {
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .baseMipLevel = 0,
                .levelCount = 1,
                .baseArrayLayer = 0,
                .layerCount = 1
            }
        };
        VK_CHECK(vkCreateImageView(state->device, &viewCreateInfo, NULL, &state->swapchainImageViews[i]));
    }
    
    free(formats);
}

void createRenderPass(AppState* state) {
    VkAttachmentDescription colorAttachment = {
        .format = state->swapchainFormat,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
        .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
        .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
        .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR
    };
    
    VkAttachmentReference colorAttachmentRef = {
        .attachment = 0,
        .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
    };
    
    VkSubpassDescription subpass = {
        .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
        .colorAttachmentCount = 1,
        .pColorAttachments = &colorAttachmentRef
    };
    
    VkRenderPassCreateInfo renderPassInfo = {
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
        .attachmentCount = 1,
        .pAttachments = &colorAttachment,
        .subpassCount = 1,
        .pSubpasses = &subpass
    };
    
    VK_CHECK(vkCreateRenderPass(state->device, &renderPassInfo, NULL, &state->renderPass));
}

void createGraphicsPipeline(AppState* state) {
    // Hardcoded shaders for a simple triangle
    const uint32_t vertShaderCode[] = {
        #include "vert.spv"  // You'll need to compile shaders separately
    };
    
    const uint32_t fragShaderCode[] = {
        #include "frag.spv"
    };
    
    // Create shader modules (simplified - in real app, load from file)
    VkShaderModule vertShaderModule;
    VkShaderModule fragShaderModule;
    
    VkShaderModuleCreateInfo vertCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize = sizeof(vertShaderCode),
        .pCode = vertShaderCode
    };
    
    VkShaderModuleCreateInfo fragCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize = sizeof(fragShaderCode),
        .pCode = fragShaderCode
    };
    
    VK_CHECK(vkCreateShaderModule(state->device, &vertCreateInfo, NULL, &vertShaderModule));
    VK_CHECK(vkCreateShaderModule(state->device, &fragCreateInfo, NULL, &fragShaderModule));
    
    VkPipelineShaderStageCreateInfo vertShaderStageInfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        .stage = VK_SHADER_STAGE_VERTEX_BIT,
        .module = vertShaderModule,
        .pName = "main"
    };
    
    VkPipelineShaderStageCreateInfo fragShaderStageInfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
        .module = fragShaderModule,
        .pName = "main"
    };
    
    VkPipelineShaderStageCreateInfo shaderStages[] = {vertShaderStageInfo, fragShaderStageInfo};
    
    // Fixed function states
    VkPipelineVertexInputStateCreateInfo vertexInputInfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO
        // No vertex inputs for now
    };
    
    VkPipelineInputAssemblyStateCreateInfo inputAssembly = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
        .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
        .primitiveRestartEnable = VK_FALSE
    };
    
    VkViewport viewport = {
        .x = 0.0f,
        .y = 0.0f,
        .width = (float)state->swapchainExtent.width,
        .height = (float)state->swapchainExtent.height,
        .minDepth = 0.0f,
        .maxDepth = 1.0f
    };
    
    VkRect2D scissor = {
        .offset = {0, 0},
        .extent = state->swapchainExtent
    };
    
    VkPipelineViewportStateCreateInfo viewportState = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
        .viewportCount = 1,
        .pViewports = &viewport,
        .scissorCount = 1,
        .pScissors = &scissor
    };
    
    VkPipelineRasterizationStateCreateInfo rasterizer = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
        .depthClampEnable = VK_FALSE,
        .rasterizerDiscardEnable = VK_FALSE,
        .polygonMode = VK_POLYGON_MODE_FILL,
        .lineWidth = 1.0f,
        .cullMode = VK_CULL_MODE_BACK_BIT,
        .frontFace = VK_FRONT_FACE_CLOCKWISE,
        .depthBiasEnable = VK_FALSE
    };
    
    VkPipelineMultisampleStateCreateInfo multisampling = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
        .sampleShadingEnable = VK_FALSE,
        .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT
    };
    
    VkPipelineColorBlendAttachmentState colorBlendAttachment = {
        .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | 
                         VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
        .blendEnable = VK_FALSE
    };
    
    VkPipelineColorBlendStateCreateInfo colorBlending = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
        .logicOpEnable = VK_FALSE,
        .logicOp = VK_LOGIC_OP_COPY,
        .attachmentCount = 1,
        .pAttachments = &colorBlendAttachment,
        .blendConstants = {0.0f, 0.0f, 0.0f, 0.0f}
    };
    
    // Pipeline layout (empty for now)
    VkPipelineLayoutCreateInfo pipelineLayoutInfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO
    };
    
    VK_CHECK(vkCreatePipelineLayout(state->device, &pipelineLayoutInfo, NULL, &state->pipelineLayout));
    
    // Create graphics pipeline
    VkGraphicsPipelineCreateInfo pipelineInfo = {
        .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        .stageCount = 2,
        .pStages = shaderStages,
        .pVertexInputState = &vertexInputInfo,
        .pInputAssemblyState = &inputAssembly,
        .pViewportState = &viewportState,
        .pRasterizationState = &rasterizer,
        .pMultisampleState = &multisampling,
        .pColorBlendState = &colorBlending,
        .layout = state->pipelineLayout,
        .renderPass = state->renderPass,
        .subpass = 0
    };
    
    VK_CHECK(vkCreateGraphicsPipelines(state->device, VK_NULL_HANDLE, 1, &pipelineInfo, NULL, &state->graphicsPipeline));
    
    // Cleanup shader modules
    vkDestroyShaderModule(state->device, vertShaderModule, NULL);
    vkDestroyShaderModule(state->device, fragShaderModule, NULL);
}

void createFramebuffers(AppState* state) {
    state->framebuffers = malloc(state->swapchainImageCount * sizeof(VkFramebuffer));
    
    for (uint32_t i = 0; i < state->swapchainImageCount; i++) {
        VkImageView attachments[] = {state->swapchainImageViews[i]};
        
        VkFramebufferCreateInfo framebufferInfo = {
            .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
            .renderPass = state->renderPass,
            .attachmentCount = 1,
            .pAttachments = attachments,
            .width = state->swapchainExtent.width,
            .height = state->swapchainExtent.height,
            .layers = 1
        };
        
        VK_CHECK(vkCreateFramebuffer(state->device, &framebufferInfo, NULL, &state->framebuffers[i]));
    }
}

void createCommandPool(AppState* state) {
    VkCommandPoolCreateInfo poolInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
        .queueFamilyIndex = state->graphicsQueueFamily
    };
    
    VK_CHECK(vkCreateCommandPool(state->device, &poolInfo, NULL, &state->commandPool));
}

void createCommandBuffers(AppState* state) {
    state->commandBuffers = malloc(state->swapchainImageCount * sizeof(VkCommandBuffer));
    
    VkCommandBufferAllocateInfo allocInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = state->commandPool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = state->swapchainImageCount
    };
    
    VK_CHECK(vkAllocateCommandBuffers(state->device, &allocInfo, state->commandBuffers));
    
    // Record command buffers
    for (uint32_t i = 0; i < state->swapchainImageCount; i++) {
        VkCommandBufferBeginInfo beginInfo = {
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO
        };
        
        VK_CHECK(vkBeginCommandBuffer(state->commandBuffers[i], &beginInfo));
        
        VkClearValue clearColor = {{{0.0f, 0.0f, 0.0f, 1.0f}}};
        
        VkRenderPassBeginInfo renderPassInfo = {
            .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
            .renderPass = state->renderPass,
            .framebuffer = state->framebuffers[i],
            .renderArea = {
                .offset = {0, 0},
                .extent = state->swapchainExtent
            },
            .clearValueCount = 1,
            .pClearValues = &clearColor
        };
        
        vkCmdBeginRenderPass(state->commandBuffers[i], &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
        vkCmdBindPipeline(state->commandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, state->graphicsPipeline);
        vkCmdDraw(state->commandBuffers[i], 3, 1, 0, 0); // Draw triangle
        vkCmdEndRenderPass(state->commandBuffers[i]);
        
        VK_CHECK(vkEndCommandBuffer(state->commandBuffers[i]));
    }
}

void createSyncObjects(AppState* state) {
    VkSemaphoreCreateInfo semaphoreInfo = {
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO
    };
    
    VkFenceCreateInfo fenceInfo = {
        .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
        .flags = VK_FENCE_CREATE_SIGNALED_BIT
    };
    
    VK_CHECK(vkCreateSemaphore(state->device, &semaphoreInfo, NULL, &state->imageAvailableSemaphore));
    VK_CHECK(vkCreateSemaphore(state->device, &semaphoreInfo, NULL, &state->renderFinishedSemaphore));
    VK_CHECK(vkCreateFence(state->device, &fenceInfo, NULL, &state->inFlightFence));
}

void drawFrame(AppState* state) {
    // Wait for previous frame to finish
    vkWaitForFences(state->device, 1, &state->inFlightFence, VK_TRUE, UINT64_MAX);
    vkResetFences(state->device, 1, &state->inFlightFence);
    
    // Acquire next image
    uint32_t imageIndex;
    vkAcquireNextImageKHR(state->device, state->swapchain, UINT64_MAX, 
                         state->imageAvailableSemaphore, VK_NULL_HANDLE, &imageIndex);
    
    // Submit command buffer
    VkSubmitInfo submitInfo = {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = &state->imageAvailableSemaphore,
        .pWaitDstStageMask = (VkPipelineStageFlags[]){VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT},
        .commandBufferCount = 1,
        .pCommandBuffers = &state->commandBuffers[imageIndex],
        .signalSemaphoreCount = 1,
        .pSignalSemaphores = &state->renderFinishedSemaphore
    };
    
    VK_CHECK(vkQueueSubmit(state->graphicsQueue, 1, &submitInfo, state->inFlightFence));
    
    // Present
    VkPresentInfoKHR presentInfo = {
        .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = &state->renderFinishedSemaphore,
        .swapchainCount = 1,
        .pSwapchains = &state->swapchain,
        .pImageIndices = &imageIndex
    };
    
    vkQueuePresentKHR(state->graphicsQueue, &presentInfo);
}

void cleanup(AppState* state) {
    vkDeviceWaitIdle(state->device);
    
    // Cleanup Vulkan resources
    vkDestroySemaphore(state->device, state->imageAvailableSemaphore, NULL);
    vkDestroySemaphore(state->device, state->renderFinishedSemaphore, NULL);
    vkDestroyFence(state->device, state->inFlightFence, NULL);
    
    vkDestroyCommandPool(state->device, state->commandPool, NULL);
    
    for (uint32_t i = 0; i < state->swapchainImageCount; i++) {
        vkDestroyFramebuffer(state->device, state->framebuffers[i], NULL);
        vkDestroyImageView(state->device, state->swapchainImageViews[i], NULL);
    }
    
    free(state->framebuffers);
    free(state->swapchainImageViews);
    free(state->swapchainImages);
    free(state->commandBuffers);
    
    vkDestroyPipeline(state->device, state->graphicsPipeline, NULL);
    vkDestroyPipelineLayout(state->device, state->pipelineLayout, NULL);
    vkDestroyRenderPass(state->device, state->renderPass, NULL);
    vkDestroySwapchainKHR(state->device, state->swapchain, NULL);
    vkDestroyDevice(state->device, NULL);
    vkDestroySurfaceKHR(state->instance, state->surface, NULL);
    vkDestroyInstance(state->instance, NULL);
}