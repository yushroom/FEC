set THIRDPARTY_DIR=%cd%\..\source\thirdparty
set BIN_DIR=%cd%\..\binaries
echo %THIRDPARTY_DIR%
echo %BIN_DIR%

if not exist thirdparty mkdir thirdparty
cd thirdparty

rem nvtt
if not exist nvtt mkdir nvtt
cd nvtt
cmake -DCMAKE_BUILD_TYPE=Release %THIRDPARTY_DIR%/nvidia-texture-tools
cmake --build . --target nvcompress --config Release
copy /y src\nvtt\tools\Release\nvcompress.exe %BIN_DIR%
cd ..

rem spirv-cross
if not exist spirv-cross mkdir spirv-cross
cd spirv-cross
cmake -DCMAKE_BUILD_TYPE=Release %THIRDPARTY_DIR%/SPIRV-Cross 
cmake --build . --target spirv-cross --config Release
copy /y .\Release\spirv-cross.exe %BIN_DIR%
cd ..

rem dxc
if not exist dxc mkdir dxc
cd dxc
set dxc_src_dir="%THIRDPARTY_DIR%\DirectXShaderCompiler"
cmake %dxc_src_dir% -DCMAKE_BUILD_TYPE=Release -DHLSL_BUILD_DXILCONV:BOOL=OFF -DCMAKE_EXPORT_COMPILE_COMMANDS:BOOL=ON -DLLVM_APPEND_VC_REV:BOOL=ON -DLLVM_DEFAULT_TARGET_TRIPLE:STRING=dxil-ms-dx -DLLVM_ENABLE_EH:BOOL=ON -DLLVM_ENABLE_RTTI:BOOL=ON -DLLVM_INCLUDE_DOCS:BOOL=OFF -DLLVM_INCLUDE_EXAMPLES:BOOL=OFF -DLLVM_INCLUDE_TESTS:BOOL=OFF -DLLVM_OPTIMIZED_TABLEGEN:BOOL=OFF -DLLVM_REQUIRES_EH:BOOL=ON -DLLVM_REQUIRES_RTTI:BOOL=ON -DLLVM_TARGETS_TO_BUILD:STRING=None -DLIBCLANG_BUILD_STATIC:BOOL=ON -DCLANG_BUILD_EXAMPLES:BOOL=OFF -DCLANG_CL:BOOL=OFF -DCLANG_ENABLE_ARCMT:BOOL=OFF -DCLANG_ENABLE_STATIC_ANALYZER:BOOL=OFF -DCLANG_INCLUDE_TESTS:BOOL=OFF -DHLSL_INCLUDE_TESTS:BOOL=OFF -DENABLE_SPIRV_CODEGEN:BOOL=ON
cmake --build . --target dxc --config Release
copy /y .\Release\bin\dxc.exe %BIN_DIR%
copy /y .\Release\bin\dxcompiler.dll %BIN_DIR%
cd ..

pause