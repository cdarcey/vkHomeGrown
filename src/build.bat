@setlocal

@set VULKAN_SDK=C:\VulkanSDK\1.4.309.0

@if not exist "%VULKAN_SDK%\Include\vulkan\vulkan.h" (
    @echo ERROR: vulkan.h not found at: %VULKAN_SDK%\Include\vulkan\vulkan.h
    @echo Please check the Vulkan SDK installation.
    @pause
    @exit /b 1
)

@pushd %~dp0

@set PROJECT_ROOT=..
@set SHADER_DIR=shaders
@set OUTPUT_DIR=%PROJECT_ROOT%/out
@set DEPENDENCIES_DIR=%PROJECT_ROOT%/dependencies

@if not exist "%DEPENDENCIES_DIR%\glfw3.lib" (
    @echo ERROR: glfw3.lib not found at: %DEPENDENCIES_DIR%\glfw3.lib
    @dir "%DEPENDENCIES_DIR%"
    @pause
    @exit /b 1
)

@set PATH=C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build;%PATH%
@set PATH=C:\Program Files\Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build;%PATH%
@set PATH=C:\Program Files (x86)\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build;%PATH%
@set PATH=C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build;%PATH%
@set PATH=C:\Program Files (x86)\Microsoft Visual Studio\2019\Enterprise/VC\Auxiliary\Build;%PATH%

@call vcvarsall.bat amd64 > nul

@set PL_RESULT=[1m[92mSuccessful.[0m

@if not exist "%OUTPUT_DIR%" mkdir "%OUTPUT_DIR%"
@if not exist "%OUTPUT_DIR%/shaders" mkdir "%OUTPUT_DIR%/shaders"

@if exist "%OUTPUT_DIR%/vkHomeGrown.exe" del "%OUTPUT_DIR%\vkHomeGrown.exe"

@echo [1m[36mCompiling shaders...[0m

@if exist "%SHADER_DIR%/textured.vert" (
    @echo Compiling vertex shader...
    "%VULKAN_SDK%\Bin\glslc.exe" "%SHADER_DIR%/textured.vert" -o "%OUTPUT_DIR%/shaders/textured_vert.spv"
    @if %ERRORLEVEL% NEQ 0 (
        @echo [91mFailed to compile vertex shader[0m
        @goto ShaderError
    )
) else (
    @echo [91mError: textured.vert not found at %SHADER_DIR%/textured.vert[0m
    @dir "%SHADER_DIR%"
    @goto ShaderError
)

@if exist "%SHADER_DIR%/textured.frag" (
    @echo Compiling fragment shader...
    "%VULKAN_SDK%\Bin\glslc.exe" "%SHADER_DIR%/textured.frag" -o "%OUTPUT_DIR%/shaders/textured_frag.spv"
    @if %ERRORLEVEL% NEQ 0 (
        @echo [91mFailed to compile fragment shader[0m
        @goto ShaderError
    )
) else (
    @echo [91mError: textured.frag not found at %SHADER_DIR%/textured.frag[0m
    @dir "%SHADER_DIR%"
    @goto ShaderError
)

@if exist "%SHADER_DIR%/not_textured.vert" (
    @echo Compiling vertex shader...
    "%VULKAN_SDK%\Bin\glslc.exe" "%SHADER_DIR%/not_textured.vert" -o "%OUTPUT_DIR%/shaders/not_textured_vert.spv"
    @if %ERRORLEVEL% NEQ 0 (
        @echo [91mFailed to compile vertex shader[0m
        @goto ShaderError
    )
) else (
    @echo [91mError: not_textured.vert not found at %SHADER_DIR%/not_textured.vert[0m
    @dir "%SHADER_DIR%"
    @goto ShaderError
)

@if exist "%SHADER_DIR%/not_textured.frag" (
    @echo Compiling fragment shader...
    "%VULKAN_SDK%\Bin\glslc.exe" "%SHADER_DIR%/not_textured.frag" -o "%OUTPUT_DIR%/shaders/not_textured_frag.spv"
    @if %ERRORLEVEL% NEQ 0 (
        @echo [91mFailed to compile fragment shader[0m
        @goto ShaderError
    )
) else (
    @echo [91mError: not_textured.frag not found at %SHADER_DIR%/not_textured.frag[0m
    @dir "%SHADER_DIR%"
    @goto ShaderError
)

@if exist "%SHADER_DIR%/cube_vert.vert" (
    @echo Compiling vertex shader...
    "%VULKAN_SDK%\Bin\glslc.exe" "%SHADER_DIR%/cube_vert.vert" -o "%OUTPUT_DIR%/shaders/cube_vert.spv"
    @if %ERRORLEVEL% NEQ 0 (
        @echo [91mFailed to compile vertex shader[0m
        @goto ShaderError
    )
) else (
    @echo [91mError: cube_vert.vert not found at %SHADER_DIR%/cube_vert.vert[0m
    @dir "%SHADER_DIR%"
    @goto ShaderError
)

@if exist "%SHADER_DIR%/cube_frag.frag" (
    @echo Compiling fragment shader...
    "%VULKAN_SDK%\Bin\glslc.exe" "%SHADER_DIR%/cube_frag.frag" -o "%OUTPUT_DIR%/shaders/cube_frag.spv"
    @if %ERRORLEVEL% NEQ 0 (
        @echo [91mFailed to compile fragment shader[0m
        @goto ShaderError
    )
) else (
    @echo [91mError: cube_frag.frag not found at %SHADER_DIR%/cube_frag.frag[0m
    @dir "%SHADER_DIR%"
    @goto ShaderError
)

@echo [92mShader compilation complete![0m
@goto AfterShaders

:ShaderError
@echo [91mShader compilation failed![0m
@set PL_RESULT=[1m[91mFailed.[0m
@goto Cleanupcpptest

:AfterShaders

@echo.
@echo [1m[93m~~~~~~~~~~~~~~~~~~~~~~[0m
@echo [1m[36mCompiling and Linking...[0m

cl main.c vkHomeGrown.c -Fe"%OUTPUT_DIR%/vkHomeGrown.exe" -Fo"%OUTPUT_DIR%/" -Od -Zi -nologo ^
-DGLFW_INCLUDE_VULKAN ^
-I"%DEPENDENCIES_DIR%" ^
-I"%PROJECT_ROOT%/dependencies/stb" ^
-I"%VULKAN_SDK%/Include" ^
-MD -link -incremental:no ^
/LIBPATH:"%VULKAN_SDK%/Lib" ^
"%DEPENDENCIES_DIR%\glfw3.lib" vulkan-1.lib user32.lib gdi32.lib shell32.lib

@set PL_BUILD_STATUS=%ERRORLEVEL%

@if %PL_BUILD_STATUS% NEQ 0 (
    @echo [1m[91mCompilation Failed with error code[0m: %PL_BUILD_STATUS%
    @set PL_RESULT=[1m[91mFailed.[0m
    goto Cleanupcpptest
)

@if exist "%DEPENDENCIES_DIR%\glfw3.dll" (
    @echo Copying GLFW DLL to output folder...
    copy "%DEPENDENCIES_DIR%\glfw3.dll" "%OUTPUT_DIR%\" > nul
    @if %ERRORLEVEL% EQU 0 (
        @echo [92mGLFW DLL copied successfully[0m
    ) else (
        @echo [91mWarning: Failed to copy GLFW DLL[0m
    )
)

:Cleanupcpptest
    @echo [1m[36mCleaning...[0m
    @del "%OUTPUT_DIR%\*.obj"  > nul 2> nul

@echo.
@echo [36mResult: [0m %PL_RESULT%
@echo [36m~~~~~~~~~~~~~~~~~~~~~~[0m

@popd