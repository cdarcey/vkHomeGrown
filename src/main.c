

#include "vkHomeGrown.h"


int main(void) 
{
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

    // vkHomeGrown api init funcs
    hg_create_instance(&tState);
    hg_create_surface(&tState);
    hg_pick_physical_device(&tState);
    hg_create_logical_device(&tState);
    hg_create_swapchain(&tState);
    hg_create_render_pass(&tState);
    hg_create_graphics_pipeline(&tState);
    hg_create_framebuffers(&tState);
    hg_create_command_pool(&tState);
    hg_create_quad_buffers(&tState);
    hg_create_command_buffers(&tState);
    hg_create_sync_objects(&tState);


    // main loop
    while (!glfwWindowShouldClose(window)) 
    {
        glfwPollEvents();  // handle window events

        // check for window resize
        int newWidth, newHeight;
        glfwGetFramebufferSize(window, &newWidth, &newHeight);
        if (newWidth != tState.width || newHeight != tState.height) 
        {
            tState.width = newWidth;
            tState.height = newHeight;
            // TODO: Handle swapchain recreation for resize
        }

        // render frame
        hg_draw_frame(&tState);
    }

    // cleanup
    vkDeviceWaitIdle(tState.tContextComponents.tDevice);  // wait before cleanup
    hg_cleanup(&tState);
    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}