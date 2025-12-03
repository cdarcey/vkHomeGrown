@rem keep environment variables modifications local
@setlocal

@rem set vulkan sdk path
@set VULKAN_SDK=C:\VulkanSDK\1.4.309.0

@rem verify the file exists
@if not exist "%VULKAN_SDK%\Include\vulkan\vulkan.h" (
    @echo ERROR: vulkan.h not found at: %VULKAN_SDK%\Include\vulkan\vulkan.h
    @echo Please check the Vulkan SDK installation.
    @pause
    @exit /b 1
)

@rem make script directory CWD (now in src folder)
@pushd %~dp0

@rem setup paths relative to src folder
@set PROJECT_ROOT=..
@set SHADER_DIR=%PROJECT_ROOT%/shaders
@set OUTPUT_DIR=%PROJECT_ROOT%/out

@rem modify PATH to find vcvarsall.bat
@set PATH=C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build;%PATH%
@set PATH=C:\Program Files\Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build;%PATH%
@set PATH=C:\Program Files (x86)\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build;%PATH%
@set PATH=C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build;%PATH%
@set PATH=C:\Program Files (x86)\Microsoft Visual Studio\2019\Enterprise/VC\Auxiliary\Build;%PATH%

@rem setup environment for MSVC dev tools
@call vcvarsall.bat amd64 > nul

@rem default compilation result
@set PL_RESULT=[1m[92mSuccessful.[0m

@rem create output directories
@if not exist "%OUTPUT_DIR%" mkdir "%OUTPUT_DIR%"
@if not exist "%OUTPUT_DIR%/shaders" mkdir "%OUTPUT_DIR%/shaders"

@rem cleanup binaries if not hot reloading
@if exist "%OUTPUT_DIR%/vkHomeGrown.exe" del "%OUTPUT_DIR%\vkHomeGrown.exe"

@rem -----------------------------------------------------------------
@rem COMPILE SHADERS
@rem -----------------------------------------------------------------
@echo [1m[36mCompiling shaders...[0m

@rem compile vertex shader
@if exist "%SHADER_DIR%/simple.vert" (
    @echo Compiling vertex shader...
    "%VULKAN_SDK%\Bin\glslc.exe" "%SHADER_DIR%/simple.vert" -o "%OUTPUT_DIR%/shaders/vert.spv"
    @if %ERRORLEVEL% NEQ 0 (
        @echo [91mFailed to compile vertex shader[0m
        @goto ShaderError
    )
) else (
    @echo [91mError: simple.vert not found at %SHADER_DIR%/simple.vert[0m
    @echo Available shaders:
    @dir "%SHADER_DIR%"
    @goto ShaderError
)

@rem compile fragment shader
@if exist "%SHADER_DIR%/simple.frag" (
    @echo Compiling fragment shader...
    "%VULKAN_SDK%\Bin\glslc.exe" "%SHADER_DIR%/simple.frag" -o "%OUTPUT_DIR%/shaders/frag.spv"
    @if %ERRORLEVEL% NEQ 0 (
        @echo [91mFailed to compile fragment shader[0m
        @goto ShaderError
    )
) else (
    @echo [91mError: simple.frag not found at %SHADER_DIR%/simple.frag[0m
    @echo Available shaders:
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
@rem -----------------------------------------------------------------
@rem END SHADER COMPILATION
@rem -----------------------------------------------------------------

@rem run compiler (and linker)
@echo.
@echo [1m[93m~~~~~~~~~~~~~~~~~~~~~~[0m
@echo [1m[36mCompiling and Linking...[0m

@rem compile from src folder, output to parent out folder
cl main.c -Fe"%OUTPUT_DIR%/vkHomeGrown.exe" -Fo"%OUTPUT_DIR%/" -Od -Zi -nologo -I"%PROJECT_ROOT%/dependencies/stb" -I"%VULKAN_SDK%/Include" -MD -link -incremental:no /LIBPATH:"%VULKAN_SDK%/Lib" user32.lib gdi32.lib vulkan-1.lib

@rem check build status
@set PL_BUILD_STATUS=%ERRORLEVEL%

@rem failed
@if %PL_BUILD_STATUS% NEQ 0 (
    @echo [1m[91mCompilation Failed with error code[0m: %PL_BUILD_STATUS%
    @set PL_RESULT=[1m[91mFailed.[0m
    goto Cleanupcpptest
)

@rem cleanup obj files
:Cleanupcpptest
    @echo [1m[36mCleaning...[0m
    @del "%OUTPUT_DIR%\*.obj"  > nul 2> nul


@rem print results
@echo.
@echo [36mResult: [0m %PL_RESULT%
@echo [36m~~~~~~~~~~~~~~~~~~~~~~[0m

@rem return CWD to previous CWD
@popd