@REM Build for Visual Studio compiler. Run your copy of vcvars32.bat or vcvarsall.bat to setup command-line compiler.
@set OUT_DIR=out
@set OUT_EXE=know-thy-enemy.dll
@set INCLUDES=/I.\imgui /I.\imgui\backends /I "%WindowsSdkDir%Include\um" /I "%WindowsSdkDir%Include\shared" /I "%DXSDK_DIR%Include"
@set SOURCES=know_thy_enemy.cpp .\imgui\backends\imgui_impl_dx11.cpp .\imgui\backends\imgui_impl_win32.cpp .\imgui\imgui*.cpp
@set LIBS=/LIBPATH:"%DXSDK_DIR%/Lib/x86" d3d11.lib d3dcompiler.lib
mkdir %OUT_DIR%
cl /LD /EHsc /WX /O2 %INCLUDES% /D UNICODE /D _UNICODE %SOURCES% /Fe%OUT_DIR%/%OUT_EXE% /Fo%OUT_DIR%/ /link %LIBS%

