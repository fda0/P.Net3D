@echo off
rem build script based on: https://github.com/EpicGamesExt/raddebugger/blob/master/build.bat
setlocal
cd /D "%~dp0"

:: --- Unpack Arguments -------------------------------------------------------
for %%a in (%*) do set "%%a=1"
if not "%msvc%"=="1" if not "%clang%"=="1" set msvc=1
if not "%release%"=="1" set debug=1
if "%debug%"=="1"   set release=0 && echo [debug mode]
if "%release%"=="1" set debug=0 && echo [release mode]
if "%msvc%"=="1"    set clang=0 && echo [msvc compile]
if "%clang%"=="1"   set msvc=0 && echo [clang compile]

:: --- Unpack Command Line Build Arguments ------------------------------------
set compile_common=
if "%asan%"=="1" set compile_common=%compile_common% -fsanitize=address && echo [asan enabled]

:: --- Compile/Link Line Definitions ------------------------------------------
set include_paths=-I..\src\base\ -I..\src\game\ -I..\src\meta\ -I..\gen\ -I..\libs\
set include_paths=%include_paths% -I..\libs\SDL\include\ -I..\libs\SDL_ttf\include\ -I..\libs\SDL_net\include\ -I..\libs\SDL_image\include\
set compile_common=%compile_common% %include_paths%

if "%debug%"=="1"     set sdl_build=build\win\Debug
if "%release%"=="1"   set sdl_build=build\win\Release
set sdl_libs=..\libs\SDL\%sdl_build%\SDL3-static.lib ..\libs\SDL_image\%sdl_build%\SDL3_image-static.lib ..\libs\SDL_net\%sdl_build%\SDL3_net-static.lib ..\libs\SDL_ttf\%sdl_build%\SDL3_ttf-static.lib

:: --- Compile/Link Line Definitions per compiler -----------------------------
set cl_common=     /nologo /FC /Z7 /W4 /wd4244 /wd4201 /wd4324 /std:clatest
set cl_debug=      call cl /Od /Ob1 /DBUILD_DEBUG=1 /MDd %cl_common% %compile_common%
set cl_release=    call cl /O2 /DBUILD_DEBUG=0 /MD %cl_common% %compile_common%
set cl_libs=       User32.lib Advapi32.lib Shell32.lib Gdi32.lib Version.lib OleAut32.lib Imm32.lib Ole32.lib Cfgmgr32.lib Setupapi.lib Winmm.lib Ws2_32.lib Iphlpapi.lib %sdl_libs%
set cl_link=       /link /MANIFEST:EMBED /INCREMENTAL:NO /pdbaltpath:%%%%_PDB%%%%
set cl_link_game=  /SUBSYSTEM:WINDOWS
set cl_out=        /out:

set clang_common=    -fdiagnostics-absolute-paths -Wall -Wno-missing-braces -Wno-unused-function -Wno-microsoft-static-assert -Wno-c2x-extensions
set clang_debug=     call clang -g -O0 -DBUILD_DEBUG=1 %clang_common% %compile_common%
set clang_release=   call clang -g -O2 -DBUILD_DEBUG=0 %clang_common% %compile_common%
set clang_libs=      -lUser32.lib -lAdvapi32.lib -lShell32.lib -lGdi32.lib -lVersion.lib -lOleAut32.lib -lImm32.lib -lOle32.lib -lCfgmgr32.lib -lSetupapi.lib -lWinmm.lib -lWs2_32.lib -lIphlpapi.lib %sdl_libs%
set clang_link=      -fuse-ld=lld -Xlinker /MANIFEST:EMBED -Xlinker /pdbaltpath:%%%%_PDB%%%%
set clang_link_game= -Xlinker /SUBSYSTEM:WINDOWS
set clang_out=       -o

:: --- Per-Build Settings -----------------------------------------------------
set link_dll=-DLL
set link_icon=icon.res
if "%msvc%"=="1"    set only_compile=/c
if "%clang%"=="1"   set only_compile=-c
if "%msvc%"=="1"    set EHsc=/EHsc
if "%clang%"=="1"   set EHsc=
if "%msvc%"=="1"    set no_aslr=/DYNAMICBASE:NO
if "%clang%"=="1"   set no_aslr=-Wl,/DYNAMICBASE:NO
if "%msvc%"=="1"    set call_rc=call rc
if "%clang%"=="1"   set call_rc=call llvm-rc

:: --- Choose Compile/Link Lines ----------------------------------------------
if "%msvc%"=="1"      set compile_debug=%cl_debug%
if "%msvc%"=="1"      set compile_release=%cl_release%
if "%msvc%"=="1"      set compile_link=%cl_link% %cl_libs%
if "%msvc%"=="1"      set compile_link_game=%compile_link% %cl_link_game%
if "%msvc%"=="1"      set out=%cl_out%
if "%clang%"=="1"     set compile_debug=%clang_debug%
if "%clang%"=="1"     set compile_release=%clang_release%
if "%clang%"=="1"     set compile_link=%clang_link% %clang_libs%
if "%clang%"=="1"     set compile_link_game=%compile_link% %clang_link_game% 
if "%clang%"=="1"     set out=%clang_out%
if "%debug%"=="1"     set compile=%compile_debug%
if "%release%"=="1"   set compile=%compile_release%

:: --- Prep Directories -------------------------------------------------------
if not exist build mkdir build

:: --- Get Current Git Commit Id ----------------------------------------------
for /f %%i in ('call git describe --always --dirty') do set compile=%compile% -DBUILD_GIT_HASH=\"%%i\"

:: --- Build SDL --------------------------------------
set cmake_stage1=call cmake -S . -B build\win
if "%release%"=="1"   set cmake_stage1=%cmake_stage1% -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_FLAGS="/arch:AVX2" -DCMAKE_CXX_FLAGS="/arch:AVX2"

set cmake_stage2=call cmake --build build\win
if "%release%"=="1"   set cmake_stage2=%cmake_stage2% --config Release

set cmake_stage1_libs=%cmake_stage1% -DBUILD_SHARED_LIBS=OFF "-DSDL3_DIR=..\SDL\build\win"

if "%sdl%"=="1" (
    if exist libs\SDL (
        rem SDL build docs: https://github.com/libsdl-org/SDL/blob/main/docs/README-cmake.md
        rem @todo(mg): pass gcc/clang flag to SDL (is that even possible?)

        pushd libs\SDL
        %cmake_stage1% -DSDL_STATIC=ON || exit /b 1
        %cmake_stage2% || exit /b 1
        popd

        pushd libs\SDL_ttf
        %cmake_stage1_libs% || exit /b 1
        %cmake_stage2% || exit /b 1
        popd

        pushd libs\SDL_net
        %cmake_stage1_libs% || exit /b 1
        %cmake_stage2% || exit /b 1
        popd

        pushd libs\SDL_image
        %cmake_stage1_libs% -DSDLIMAGE_VENDORED=OFF || exit /b 1
        %cmake_stage2% || exit /b 1
        popd

        set didbuild=1
    ) else (
        echo "SDL directory not found! Make sure to initialize git submodules."
    )
)

pushd build
if "%game%"=="1" (
    :: --- Clean gen directory ------------------------------------------------
    if exist ..\gen\ rmdir /q /s ..\gen\
    mkdir ..\gen\

    :: --- Metaprogram --------------------------------------------------------
    %compile% ..\src\meta\meta_entry.c %compile_link% %out%meta.exe || exit /b 1
    meta.exe || exit /b 1

    :: --- Produce Logo Icon File ---------------------------------------------
    %call_rc% /nologo /fo icon.res ..\res\ico\icon.rc || exit /b 1

    :: --- Precompile shaders -------------------------------------------------
    dxc ..\src\game\shader_model.hlsl /E ShaderRigidVS   /T vs_6_0 /D IS_RIGID=1    /Fh ..\gen\gen_shader_rigid.vert.h || exit /b 1
    dxc ..\src\game\shader_model.hlsl /E ShaderRigidPS   /T ps_6_0 /D IS_RIGID=1    /Fh ..\gen\gen_shader_rigid.frag.h || exit /b 1
    dxc ..\src\game\shader_model.hlsl /E ShaderSkinnedVS /T vs_6_0 /D IS_SKINNED=1  /Fh ..\gen\gen_shader_skinned.vert.h || exit /b 1
    dxc ..\src\game\shader_model.hlsl /E ShaderSkinnedPS /T ps_6_0 /D IS_SKINNED=1  /Fh ..\gen\gen_shader_skinned.frag.h || exit /b 1
    dxc ..\src\game\shader_model.hlsl /E ShaderWallVS    /T vs_6_0 /D IS_TEXTURED=1 /Fh ..\gen\gen_shader_wall.vert.h || exit /b 1
    dxc ..\src\game\shader_model.hlsl /E ShaderWallPS    /T ps_6_0 /D IS_TEXTURED=1 /Fh ..\gen\gen_shader_wall.frag.h || exit /b 1

    :: --- Compile game -------------------------------------------------------
    %compile% ..\src\game\game_sdl_entry.c %compile_link_game% %link_icon% %out%p.exe || exit /b 1
    set didbuild=1
)
popd

:: --- Unset ------------------------------------------------------------------
for %%a in (%*) do set "%%a=0"
set compile=
set compile_link=
set out=
set msvc=
set debug=
set release=

:: --- Warn On No Builds ------------------------------------------------------
if "%didbuild%"=="" (
  echo [WARNING] no valid build target specified; must use build target names as arguments to this script, like `build game`.
  exit /b 1
)
