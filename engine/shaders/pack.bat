set dxc="%cd%\..\binaries\dxc.exe"
set spirv="%cd%\..\binaries\spirv-cross.exe"
set dxc_flags=-I %cd%\include -Ges -nologo -not_use_legacy_cbuf_load -Ges -pack_optimized -WX
set dxc_spirv_flags=%dxc_flags% -spirv -fvk-bind-globals 0 0
set spirv_flags=tmp.spv --remove-unused-variables

if not exist runtime mkdir runtime
cd runtime
%dxc% --help
%spirv% --help

REM %dxc% ..\Texture.hlsl -Fo tmp.spv -spirv -E PS -T ps_6_0 %dxc_flags%
REM %spirv% tmp.spv --reflect
REM %spirv% --output Texture_ps.metal %spirv_flags%

REM pause
REM exit
set file=..\pbrMetallicRoughness.hlsl %dxc_flags%
set fn=pbrMetallicRoughness
%dxc% %file% -Fo %fn%_vs.cso -E VS -T vs_6_0
%dxc% %file% -Fo %fn%_ps.cso -E PS -T ps_6_0
%dxc% %file% -Fo tmp.spv -E VS -T vs_6_0 %dxc_spirv_flags%
%spirv% --rename-entry-point VS %fn%_vs vert --output %fn%_vs.reflect.json %spirv_flags% --reflect
%dxc% %file% -Fo tmp.spv -E PS -T ps_6_0 %dxc_spirv_flags%
%spirv% --rename-entry-point PS %fn%_ps frag --output %fn%_ps.reflect.json %spirv_flags% --reflect

pause

REM function HLSL2METAL() {
REM     fn=$(basename $1 .hlsl)
REM     %dxc% $1 -Fo tmp.spv -E VS -T vs_6_0 $dxc_flags
REM     %spirv% --rename-entry-point VS %fn%_VS vert --output %fn%_vs.reflect.json $spirv_flags --reflect
REM     %dxc% $1 -Fo tmp.spv -E PS -T ps_6_0 $dxc_flags
REM     %spirv% --rename-entry-point PS %fn%_PS frag --output %fn%_ps.reflect.json $spirv_flags --reflect
REM %

REM for file in ..\*.hlsl
REM do
REM     echo $file
REM     HLSL2METAL $file
REM done