import glob, os, sys, json
import tarfile
from shutil import copyfile, rmtree
from CompileUnityShader import CompileAllVariants

fxc = r"C:\Program Files (x86)\Windows Kits\10\bin\10.0.18362.0\x64\fxc.exe"
dxc = r"E:\workspace\cengine\engine\binaries\dxc.exe"
hlslreflect = r"..\binaries\Release\hlslreflect.exe"

output_dir = "runtime/d3d"
if os.path.exists(output_dir):
    rmtree(output_dir)
os.makedirs(output_dir)

def reflect(input, output):
    input = os.path.abspath(input)
    cmd = f'""{hlslreflect}" {input} > {output}"'
    print(cmd)
    return os.system(cmd)

def basename(path:str):
    return os.path.splitext(os.path.basename(path))[0]

def CompileAndReflect(path:str, shaderType:str):
    print(f, shaderType)
    assert(shaderType in ['vs', 'ps', 'cs'])
    fn = basename(f)

    st = shaderType.lower()
    ST = shaderType.upper()
    output = f"{output_dir}/{fn}_{st}.cso"
    cmd = f'""{fxc}" {f} /nologo /I .\\include /E {ST} /T {st}_5_1 -Fo {output}"'
    ret = os.system(cmd)
    assert(ret == 0)

    input = output
    output = f"{output_dir}/{fn}_{st}.reflect.json"
    ret = reflect(input, output)
    assert(ret == 0)
    return ret

if __name__ == "__main__":
    with open("./shaders.json", 'r') as f:
        shaders = json.load(f)
    print(shaders)
    unity_files = [x for x in shaders if x.endswith(".shader")]
    hlsl_files = [x for x in shaders if x.endswith('.hlsl')]
    comp_files = [x for x in shaders if x.endswith('.comp')]

    for f in unity_files:
        CompileAllVariants(f, output_dir)

    for f in hlsl_files:
        CompileAndReflect(f, 'vs')
        CompileAndReflect(f, 'ps')

    for f in comp_files:
        CompileAndReflect(f, 'cs')

    files = hlsl_files + comp_files + glob.glob(f"{output_dir}/*")

    tar_path = "./shaders.tar"
    with tarfile.open(tar_path, "w") as tar:
        for f in files:
            #fullpath = os.path.abspath(f)
            name = os.path.basename(f)
            tar.add(f, name)
    
    copyfile(tar_path, "../binaries/Debug/shaders.tar")
    copyfile(tar_path, "../binaries/Release/shaders.tar")