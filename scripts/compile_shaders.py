import os
import subprocess
import sys
import argparse
import shutil
from pathlib import Path

os.chdir(sys.path[0])

parser = argparse.ArgumentParser(prog='Obsidian Shader Compiler', description='Compiles glsl shaders into spir-v.')
parser.add_argument('-o', '--output', default='../build/shaders')
parser.add_argument('--dbg', default=False, action='store_true')
args = parser.parse_args()

shader_define_variants = {
    "default": {
        "c-" : ["-D_HAS_COLOR"],
        "u-" : ["-D_HAS_UV"],
        "cu-" : ["-D_HAS_COLOR", "-D_HAS_UV"]
    },
    "default-unlit": {
        "c-" : ["-D_HAS_COLOR"],
        "u-" : ["-D_HAS_UV"],
        "cu-" : ["-D_HAS_COLOR", "-D_HAS_UV"]
    }
}

def execute_compilation(shader_src_path, shader_out_dir, shader_out_name, debugOn, defines = []):
    vert_shader_src_path = shader_src_path + ".vert"
    frag_shader_src_path = shader_src_path + ".frag"
    compile_options =  ["-s", "-Od", "-gVS"] if debugOn else ["-s", "-Os"]
    subprocess.check_output(
        ["glslangValidator", "-V"]
        + compile_options + defines
        + ["-o", f"{shader_out_dir}/{shader_out_name}-vert.spv", f"{vert_shader_src_path}"])
    subprocess.check_output(
        ["glslangValidator", "-V"]
        + compile_options + defines
        + ["-o", f"{shader_out_dir}/{shader_out_name}-frag.spv", f"{frag_shader_src_path}"])

def compile_shaders(shader_src_dir, shader_out_dir, debugOn):
    if not os.path.exists(shader_out_dir):
        os.makedirs(shader_out_dir)
    shader_src_file_names = set()
    for p in os.listdir(shader_src_dir):
        if os.path.isfile(os.path.join(shader_src_dir, p)):
            shader_src_file_names.add(Path(p).with_suffix("").as_posix())
    shader_names = set()
    for shader_src_name in shader_src_file_names:
        shader_src_path = os.path.join(shader_src_dir, shader_src_name)
        shader_names.add(Path(shader_src_name).stem)
        shader_out_name = shader_src_name.replace(".", "-")
        execute_compilation(shader_src_path, shader_out_dir, shader_out_name, debugOn)
        if shader_out_name in shader_define_variants:
            for key in shader_define_variants[shader_out_name]:
                execute_compilation(shader_src_path, shader_out_dir, key + shader_out_name, debugOn, shader_define_variants[shader_out_name][key])
                shader_names.add(key + Path(shader_src_name).stem)

compile_shaders("../assets/shaders/src", args.output, args.dbg is not None)
compile_shaders("../assets/shaders/src/built-in", f"{args.output}/built-in", args.dbg is not None)
