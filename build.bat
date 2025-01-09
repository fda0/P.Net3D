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
if "%asan%"=="1"      set compile_additional=%compile_additional% -fsanitize=address && echo [asan enabled]

:: --- Compile/Link Line Definitions ------------------------------------------
set include_paths=-I..\src\base\ -I..\src\game\ -I..\src\meta\ -I..\src\gen\
set include_paths=%include_paths% -I..\libs\SDL\include\ -I..\libs\SDL_image\include\ -I..\libs\SDL_net\include\
set compile_common=%compile_additional% %include_paths%

set common_link_libs=..\libs\SDL\build\win\Debug\SDL3-static.lib ..\libs\SDL_image\build\win\Debug\SDL3_image-static.lib ..\libs\SDL_net\build\win\Debug\SDL3_net-static.lib

:: --- Compile/Link Line Definitions per compiler -----------------------------
set cl_common=     /nologo /FC /Z7 /MD /W4 /wd4244 /wd4201 /std:clatest
set cl_debug=      call cl /Od /Ob1 /DBUILD_DEBUG=1 %cl_common% %compile_common%
set cl_release=    call cl /O2 /DBUILD_DEBUG=0 %cl_common% %compile_common%
set cl_libs=       User32.lib Advapi32.lib Shell32.lib Gdi32.lib Version.lib OleAut32.lib Imm32.lib Ole32.lib Cfgmgr32.lib Setupapi.lib Winmm.lib Ws2_32.lib Iphlpapi.lib %common_link_libs%
set cl_link=       /link /SUBSYSTEM:WINDOWS /MANIFEST:EMBED /INCREMENTAL:NO /pdbaltpath:%%%%_PDB%%%% /STACK:33554432 %cl_libs%
set cl_out=        /out:

set clang_common=  -fdiagnostics-absolute-paths -Wall -Wno-unused-variable -Wno-missing-braces -Wno-unused-function -Wno-microsoft-static-assert -Wno-c2x-extensions
set clang_debug=   call clang -g -O0 -DBUILD_DEBUG=1 %clang_common% %compile_common%
set clang_release= call clang -g -O2 -DBUILD_DEBUG=0 %clang_common% %compile_common%
set clang_libs=    -lUser32.lib -lAdvapi32.lib -lShell32.lib -lGdi32.lib -lVersion.lib -lOleAut32.lib -lImm32.lib -lOle32.lib -lCfgmgr32.lib -lSetupapi.lib -lWinmm.lib -lWs2_32.lib -lIphlpapi.lib %common_link_libs%
set clang_link=    -fuse-ld=lld -Xlinker /SUBSYSTEM:WINDOWS -Xlinker /MANIFEST:EMBED -Xlinker /pdbaltpath:%%%%_PDB%%%% %clang_libs%
set clang_out=     -o

:: --- Per-Build Settings -----------------------------------------------------
set link_dll=-DLL
set link_icon=icon.res
if "%msvc%"=="1"    set only_compile=/c
if "%clang%"=="1"   set only_compile=-c
if "%msvc%"=="1"    set EHsc=/EHsc
if "%clang%"=="1"   set EHsc=
if "%msvc%"=="1"    set no_aslr=/DYNAMICBASE:NO
if "%clang%"=="1"   set no_aslr=-Wl,/DYNAMICBASE:NO
if "%msvc%"=="1"    set rc=call rc
if "%clang%"=="1"   set rc=call llvm-rc

:: --- Choose Compile/Link Lines ----------------------------------------------
if "%msvc%"=="1"      set compile_debug=%cl_debug%
if "%msvc%"=="1"      set compile_release=%cl_release%
if "%msvc%"=="1"      set compile_link=%cl_link%
if "%msvc%"=="1"      set out=%cl_out%
if "%clang%"=="1"     set compile_debug=%clang_debug%
if "%clang%"=="1"     set compile_release=%clang_release%
if "%clang%"=="1"     set compile_link=%clang_link%
if "%clang%"=="1"     set out=%clang_out%
if "%debug%"=="1"     set compile=%compile_debug%
if "%release%"=="1"   set compile=%compile_release%

:: --- Prep Directories -------------------------------------------------------
if not exist build mkdir build

:: --- Get Current Git Commit Id ----------------------------------------------
for /f %%i in ('call git describe --always --dirty') do set compile=%compile% -DBUILD_GIT_HASH=\"%%i\"

:: --- Build SDL --------------------------------------
if "%sdl%"=="1" (
    if exist libs\SDL (
        rem SDL build docs: https://github.com/libsdl-org/SDL/blob/main/docs/README-cmake.md
        rem @todo(mg): handle release flag for SDL
        rem @todo(mg): pass gcc/clang flag to SDL (is that even possible?)
        rem @todo(mg): I hate cmake/microsoft. This doesn't work!
        rem            Seems like this cmake doesn't work from .bat file
        rem            It only works for me when I paste these commands into
        rem            pwsh.exe with msvc developer tools.
        rem            Would be nice to fix this :)

        pushd libs\SDL
        (call cmake -S . -B build\win -DSDL_STATIC=ON && call cmake --build build\win) || exit /b 1
        popd
        pushd libs\SDL_image
        (call cmake -S . -B build\win -DBUILD_SHARED_LIBS=OFF -DSDLIMAGE_VENDORED=OFF "-DSDL3_DIR=..\SDL\build\win" && call cmake --build build\win) || exit /b 1
        popd
        pushd libs\SDL_net
        (call cmake -S . -B build\win -DBUILD_SHARED_LIBS=OFF "-DSDL3_DIR=..\SDL\build\win" && call cmake --build build\win) || exit /b 1
        set didbuild=1
        popd
    ) else (
        echo "SDL directory not found! Make sure to initialize git submodules."
    )
)

pushd build
if "%game%"=="1" (
    :: --- Clean gen directory ------------------------------------------------
    if exist ..\src\gen\ rmdir /q /s ..\src\gen\
    mkdir ..\src\gen\

    :: --- Metaprogram --------------------------------------------------------
    %compile% ..\src\meta\meta_entry.c -link %out%meta.exe || exit /b 1
    meta.exe

    :: --- Produce Logo Icon File ---------------------------------------------
    %rc% /nologo /fo icon.res ..\res\ico\icon.rc || exit /b 1

    :: --- Precompile shaders -------------------------------------------------
    dxc ..\src\shader\shader_game.hlsl /E ShaderGameVS /T vs_6_0 /Fh ..\src\gen\shader_game.vert.hx || exit /b 1
    dxc ..\src\shader\shader_game.hlsl /E ShaderGamePS /T ps_6_0 /Fh ..\src\gen\shader_game.frag.hx || exit /b 1

    :: --- Compile game -------------------------------------------------------
    %compile% ..\src\game\pog_entry.c %compile_link% %link_icon% %out%pog.exe || exit /b 1
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
