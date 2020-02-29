THIRDPARTY_DIR=${PWD}/../source/thirdparty
BIN_DIR=${PWD}/../binaries
echo ${THIRDPARTY_DIR}
echo ${BIN_DIR}

mkdir -p thirdparty && cd thirdparty

# nvtt
mkdir -p nvtt && cd nvtt
cmake -DCMAKE_BUILD_TYPE=Release ${THIRDPARTY_DIR}/nvidia-texture-tools
cmake --build . --target nvcompress --config Release
cp -rf src/nvtt/tools/nvcompress ${BIN_DIR}
cd ..

# spirv-cross
mkdir -p spirv-cross && cd spirv-cross
cmake -DCMAKE_BUILD_TYPE=Release ${THIRDPARTY_DIR}/SPIRV-Cross 
cmake --build . --target spirv-cross --config Release
cp -rf spirv-cross ${BIN_DIR}
cd ..

# dxc
mkdir -p dxc && cd dxc
dxc_src_dir="${THIRDPARTY_DIR}/DirectXShaderCompiler"
cmake ${dxc_src_dir} -DCMAKE_BUILD_TYPE=Release $(cat ${dxc_src_dir}/utils/cmake-predefined-config-params)
cmake --build . --target dxc --config Release
install_name_tool -add_rpath ${PWD}/lib/libdxcompiler.3.7.dylib ./bin/dxc
cp -rf bin/dxc ${BIN_DIR}
cd ..