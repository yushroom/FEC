dxc="${PWD}/../build/thirdparty/dxc/bin/dxc"
spirv="${PWD}/../binaries/spirv-cross"
dxc_flags="-I ../include -Ges -nologo -spirv -not_use_legacy_cbuf_load -Ges -pack_optimized -WX -fvk-bind-globals 0 0"
spirv_flags="tmp.spv --msl --msl-version 020101 --remove-unused-variables --msl-decoration-binding"
mkdir -p runtime && cd runtime
$dxc --help
$spirv --help
$dxc ../Texture.hlsl -Fo tmp.spv -spirv -E PS -T ps_6_0 $dxc_flags
$spirv tmp.spv --msl --reflect
$spirv --output Texture_ps.metal $spirv_flags
exit
function HLSL2METAL() {
    fn=$(basename $1 .hlsl)
    $dxc $1 -Fo tmp.spv -E VS -T vs_6_0 $dxc_flags
    $spirv --rename-entry-point VS ${fn}_VS vert --output ${fn}_vs.metal  $spirv_flags
    $spirv --rename-entry-point VS ${fn}_VS vert --output ${fn}_vs.reflect.json $spirv_flags --reflect
    $dxc $1 -Fo tmp.spv -E PS -T ps_6_0 $dxc_flags
    $spirv --rename-entry-point PS ${fn}_PS frag --output ${fn}_ps.metal $spirv_flags
    $spirv --rename-entry-point PS ${fn}_PS frag --output ${fn}_ps.reflect.json $spirv_flags --reflect
    xcrun -sdk macosx metal -c ${fn}_vs.metal -o ${fn}_vs.air
    xcrun -sdk macosx metal -c ${fn}_ps.metal -o ${fn}_ps.air
}

for file in ../*.hlsl
do
    echo $file
    HLSL2METAL $file
done

xcrun -sdk macosx metallib *.air -o shaders.metallib