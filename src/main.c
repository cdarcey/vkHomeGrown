


#include "vkHomeGrown.h"


int WINAPI 
WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) 
{
    (void)hPrevInstance; (void)lpCmdLine; // unused

    // 1. register window class
    const char CLASS_NAME[] = "VulkanWindowClass";

    WNDCLASSEX wc = {
        .cbSize        = sizeof(WNDCLASSEX),
        .style         = CS_HREDRAW | CS_VREDRAW,
        .lpfnWndProc   = hg_window_procedure,
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

    if(!hWnd) return 0;

    ShowWindow(hWnd, nCmdShow);
    UpdateWindow(hWnd);

    // 3. initialize app state
    hgAppData tState  = {0};
    tState.hInstance = hInstance;
    tState.hWnd      = hWnd;
    tState.width     = 800;
    tState.height    = 600;

    // 4. initialize vulkan
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


    // 5. main loop
    MSG msg = {0};
    while(msg.message != WM_QUIT) 
    {
        if(PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) 
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        } 
        else 
        {
            // render frame
            hg_draw_frame(&tState);
        }
    }

    // 6. cleanup
    hg_cleanup(&tState);

    return (int)msg.wParam;
}

