

#include "vkHomeGrown.h"
#include "hg_math.h"


int main(void) 
{

    // example settings
    bool bQuad     = true;  // false renders a triangle
    bool bTextured = false; // false renders colors in vert data 

    // init GLFW
    if (!glfwInit()) 
    {
        printf("Failed to initialize GLFW!\n");
        return -1;
    }

    // tell GLFW not to create OpenGL context
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

    // create window
    GLFWwindow* window = glfwCreateWindow(800, 600, "Vulkan Application", NULL, NULL);
    if (!window) 
    {
        printf("Failed to create GLFW window!\n");
        glfwTerminate();
        return -1;
    }

    // init app state
    hgAppData tState = {0};
    tState.pWindow = window;  // store GLFW window pointer
    tState.width = 800;
    tState.height = 600;

    // get actual window size (framebuffer size for Vulkan)
    int fbWidth, fbHeight;
    glfwGetFramebufferSize(window, &fbWidth, &fbHeight);
    tState.width = fbWidth;
    tState.height = fbHeight;

    // fixed resources
    hg_create_instance(&tState, "cube app", VK_MAKE_VERSION(1, 0, 0), true);
    hg_create_surface(&tState);
    hg_pick_physical_device(&tState);
    hg_create_logical_device(&tState);


    // cube vertices (8 corners)
    hgVertex tCubeVertices[8] = {
        // x, y, z             r, g, b, a
        {-0.5f, -0.5f, -0.5f,  1.0f, 0.0f, 0.0f, 1.0f}, // back bottom left
        { 0.5f, -0.5f, -0.5f,  0.0f, 1.0f, 0.0f, 1.0f}, // back bottom right
        { 0.5f,  0.5f, -0.5f,  0.0f, 0.0f, 1.0f, 1.0f}, // back top right
        {-0.5f,  0.5f, -0.5f,  1.0f, 1.0f, 0.0f, 1.0f}, // back top left
        {-0.5f, -0.5f,  0.5f,  1.0f, 0.0f, 1.0f, 1.0f}, // front bottom left
        { 0.5f, -0.5f,  0.5f,  0.0f, 1.0f, 1.0f, 1.0f}, // front bottom right
        { 0.5f,  0.5f,  0.5f,  1.0f, 1.0f, 1.0f, 1.0f}, // front top right
        {-0.5f,  0.5f,  0.5f,  0.5f, 0.5f, 0.5f, 1.0f}, // front top left
    };

    // cube indices (36 indices)
    uint16_t tCubeIndices[] = {
        // back face
        0, 1, 2,  2, 3, 0,
        // front face
        4, 5, 6,  6, 7, 4,
        // left face
        0, 3, 7,  7, 4, 0,
        // right face
        1, 5, 6,  6, 2, 1,
        // bottom face
        0, 1, 5,  5, 4, 0,
        // top face
        3, 2, 6,  6, 7, 3
    };

    hgRenderPassConfig tConfig = {
        .tLoadOp  = VK_ATTACHMENT_LOAD_OP_CLEAR,
        .tStoreOp = VK_ATTACHMENT_STORE_OP_STORE,
        .afClearColor = {1.0f, 1.0f, 1.0f, 1.0f}
    };

    hg_create_swapchain(&tState, VK_PRESENT_MODE_FIFO_KHR);
    hg_create_render_pass(&tState, &tConfig);

    hg_create_framebuffers(&tState);
    hg_create_command_pool(&tState);

    hgVertexBuffer tCubeVertexBuffer = hg_create_vertex_buffer(&tState, tCubeVertices, sizeof(tCubeVertices), sizeof(hgVertex));
    hgIndexBuffer  tCubeIndexBuffer  = hg_create_index_buffer(&tState, tCubeIndices, 36);


    // descriptors 
    VkDescriptorPoolSize tPoolSize = {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 50};
    VkDescriptorPool     tDescPool = hg_create_descriptor_pool(&tState, 100, &tPoolSize, 1);


    VkDescriptorSetLayout tDescriptorSetLayout = VK_NULL_HANDLE;
    VkDescriptorSet       tDescriptorSet       = VK_NULL_HANDLE;

    // sets and layouts
    VkDescriptorSetLayoutBinding tUboLayout = {
        .binding         = 0,
        .descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
        .descriptorCount = 1,
        .stageFlags      = VK_SHADER_STAGE_VERTEX_BIT
    };
    VkDescriptorSetLayoutCreateInfo tDescriptorLayoutInfo = {
        .sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = 1,
        .pBindings    = &tUboLayout
    };
    VULKAN_CHECK(vkCreateDescriptorSetLayout(tState.tContextComponents.tDevice, &tDescriptorLayoutInfo, NULL, &tDescriptorSetLayout));

    // allocate
    const VkDescriptorSetAllocateInfo tDescSetAllocInfo = {
        .sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool     = tDescPool,
        .descriptorSetCount = 1,
        .pSetLayouts        = &tDescriptorSetLayout
    };
    VULKAN_CHECK(vkAllocateDescriptorSets(tState.tContextComponents.tDevice, &tDescSetAllocInfo, &tDescriptorSet));

    // create uniform buffer
    hgUniformBuffer tUniBuffer = hg_create_uniform_buffer(&tState, sizeof(UniformBufferObject));

    VkDescriptorBufferInfo tBufferInfo = {
        .buffer = tUniBuffer.tBuffer,
        .offset = 0,
        .range  = sizeof(UniformBufferObject)
    };
    
    VkWriteDescriptorSet tDescriptorWrite = {
        .sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .dstSet          = tDescriptorSet,
        .dstBinding      = 0,
        .dstArrayElement = 0,
        .descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
        .descriptorCount = 1,
        .pBufferInfo     = &tBufferInfo
    };
    vkUpdateDescriptorSets(tState.tContextComponents.tDevice, 1, &tDescriptorWrite, 0, NULL);


    // tests for new pipeline creation
    VkVertexInputAttributeDescription tTestVertAttribs[3] = {
        {.location = 0, .binding = 0, .format = VK_FORMAT_R32G32B32_SFLOAT,    .offset = 0},                 // pos
        {.location = 1, .binding = 0, .format = VK_FORMAT_R32G32B32A32_SFLOAT, .offset = sizeof(float) * 3}, // color

    };

    hgPipelineConfig tTestConfig = {
        .pcVertexShaderPath        = "../out/shaders/cube_vert.spv",
        .pcFragmentShaderPath      = "../out/shaders/cube_frag.spv",
        .uVertexStride             = sizeof(float) * 8,
        .ptAttributeDescriptions   = tTestVertAttribs,
        .uAttributeCount           = 2,
        .bBlendEnable              = VK_FALSE,
        .tCullMode                 = VK_CULL_MODE_NONE,
        .tFrontFace                = VK_FRONT_FACE_CLOCKWISE,
        .ptDescriptorSetLayouts    = &tDescriptorSetLayout,
        .uDescriptorSetLayoutCount = 1,
        .ptPushConstantRanges      = NULL,
        .uPushConstantRangeCount   = 0,
        .tPipelineBindPoint        = VK_PIPELINE_BIND_POINT_GRAPHICS // for pipeline binding
    };
    hgPipeline tCubePipline = hg_create_graphics_pipeline(&tState, &tTestConfig);

    hg_create_sync_objects(&tState);

    // command buffer creation and recording
    hg_allocate_frame_cmd_buffers(&tState);

    //rotation variable
    float rotation = 0.0f;

    // main loop
    while (!glfwWindowShouldClose(window)) 
    {
        glfwPollEvents();

        int newWidth, newHeight;
        glfwGetFramebufferSize(window, &newWidth, &newHeight);

        // handle resize or minimization (width and height are 0)
        if (newWidth != tState.width || newHeight != tState.height || newWidth == 0 || newHeight == 0) 
        {
            // wait for device to finish current work
            vkDeviceWaitIdle(tState.tContextComponents.tDevice);

            // recreate swapchain and dependent resources
            hg_recreate_swapchain(&tState);
            continue; // skip this frame
        }

        // rotate cube each frame
        rotation += 0.01f;

        UniformBufferObject uboData = {0};

        // model matrix - rotate around Y axis
        mat4_rotate_y(uboData.model, rotation);

        // view matrix - camera at (0, 0, 3) looking at origin
        mat4_translate(uboData.view, 0.0f, 0.0f, -5.0f);

        // projection matrix - perspective
        float aspect = (float)tState.width / (float)tState.height;
        mat4_perspective(uboData.proj, 45.0f * 3.14159f / 180.0f, aspect, 0.1f, 100.0f);

        // update the uniform buffer
        hg_update_uniform_buffer(&tState, &tUniBuffer, &uboData, sizeof(uboData));

        VkDeviceSize tOffset[] = {0}; // no offset but vkCmdBindVertexBuffers wants this type passed in

        // begin frame
        uint32_t uImageIndex = hg_begin_frame(&tState);
        hg_begin_render_pass(&tState, uImageIndex);

        // scene/ frame building -> testing stuff in here for now
        vkCmdBindPipeline(tState.tCommandComponents.tCommandBuffers[uImageIndex], VK_PIPELINE_BIND_POINT_GRAPHICS, tCubePipline.tPipeline);
        vkCmdBindDescriptorSets(tState.tCommandComponents.tCommandBuffers[uImageIndex], tCubePipline.tPipelineBindPoint, 
                tCubePipline.tPipelineLayout, 0, 1, &tDescriptorSet, 0, 0);
        vkCmdBindVertexBuffers(tState.tCommandComponents.tCommandBuffers[uImageIndex], 0, 1, &tCubeVertexBuffer.tBuffer, tOffset);
        vkCmdBindIndexBuffer(tState.tCommandComponents.tCommandBuffers[uImageIndex], tCubeIndexBuffer.tBuffer, 0, VK_INDEX_TYPE_UINT16);
        vkCmdDrawIndexed(tState.tCommandComponents.tCommandBuffers[uImageIndex], tCubeIndexBuffer.uIndexCount, 1, 0, 0, 0);

        // end frame 
        hg_end_render_pass(&tState);
        hg_end_frame(&tState, tState.tCommandComponents.uCurrentImageIndex);

    }

    // cleanup
    vkDeviceWaitIdle(tState.tContextComponents.tDevice);  // wait before cleanup

    // destroy low level resources first 
    hg_destroy_vertex_buffer(&tState, &tCubeVertexBuffer);
    hg_destroy_index_buffer(&tState, &tCubeIndexBuffer);
    hg_destroy_uniform_buffer(&tState, &tUniBuffer);

    // destroy pipeline 
    hg_destroy_pipeline(&tState, &tCubePipline); // destroys pipeline + pipeline layout

    // vulkan clean up that have no helpers (the resources will be managed by the api user)
    vkDestroyDescriptorPool(tState.tContextComponents.tDevice, tDescPool, NULL);
    vkDestroyDescriptorSetLayout(tState.tContextComponents.tDevice, tDescriptorSetLayout, NULL);

    // should be called after all other cleanup
    hg_core_cleanup(&tState);
    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}