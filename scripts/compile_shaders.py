import os
import subprocess
import sys
import argparse
import shutil
from pathlib import Path

os.chdir(sys.path[0])

parser = argparse.ArgumentParser(prog='Obsidian Shader Compiler', description='Compiles glsl shaders into spir-v.')
parser.add_argument('-o', '--output', default='../build/shaders')
args = parser.parse_args()

shader_define_variants = {
    "default-vert": {
        "c-" : ["-D_HAS_COLOR"],
        "u-" : ["-D_HAS_UV"],
        "cu-" : ["-D_HAS_COLOR", "-D_HAS_UV"]
    }, 
    "default-frag": {
        "c-" : ["-D_HAS_COLOR"],
        "u-" : ["-D_HAS_UV"],
        "cu-" : ["-D_HAS_COLOR", "-D_HAS_UV"]
    }, 
    "default-unlit-vert": {
        "c-" : ["-D_HAS_COLOR"],
        "u-" : ["-D_HAS_UV"],
        "cu-" : ["-D_HAS_COLOR", "-D_HAS_UV"]
    },
    "default-unlit-frag": {
        "c-" : ["-D_HAS_COLOR"],
        "u-" : ["-D_HAS_UV"],
        "cu-" : ["-D_HAS_COLOR", "-D_HAS_UV"]
    }
}

def execute_compilation(shader_src_path, shader_temp_dir, shader_out_name, defines = []):
    subprocess.check_output(["glslc", "-c", f"{shader_src_path}", "--target-env=vulkan1.2", "-Werror", "-Os"] + defines + ["-o", f"{shader_temp_dir}/{shader_out_name}.spv"] )
    subprocess.check_output(["glslc", "-c", f"{shader_src_path}", "--target-env=vulkan1.2", "-Werror", "-g", "-O0"] + defines + ["-o", f"{shader_temp_dir}/{shader_out_name}-dbg.spv"])



def compile_shaders(shader_src_dir, shader_out_dir):
    if not os.path.exists(shader_out_dir):
        os.makedirs(shader_out_dir)
    shader_temp_dir = os.path.join(shader_out_dir, "temp")
    if not os.path.exists(shader_temp_dir):
        os.makedirs(shader_temp_dir)
    shader_src_file_names = [p for p in os.listdir(shader_src_dir) if os.path.isfile(os.path.join(shader_src_dir, p))]
    shader_names = set()
    for shader_src_name in shader_src_file_names:
        shader_src_path = os.path.join(shader_src_dir, shader_src_name)
        shader_names.add(Path(shader_src_name).stem)
        shader_out_name = shader_src_name.replace(".", "-")
        execute_compilation(shader_src_path, shader_temp_dir, shader_out_name)
        if shader_out_name in shader_define_variants:
            for key in shader_define_variants[shader_out_name]:
                execute_compilation(shader_src_path, shader_temp_dir, key + shader_out_name, shader_define_variants[shader_out_name][key])
                shader_names.add(key + Path(shader_src_name).stem)
    for shader_file in shader_names:
        subprocess.check_output([
            "spirv-link",
            f"{shader_temp_dir}/{shader_file}-vert.spv",
            f"{shader_temp_dir}/{shader_file}-frag.spv",
            "-o",
            f"{shader_out_dir}/{shader_file}.spv"])
        subprocess.check_output([
            "spirv-link",
            f"{shader_temp_dir}/{shader_file}-vert-dbg.spv",
            f"{shader_temp_dir}/{shader_file}-frag-dbg.spv",
            "-o",
            f"{shader_out_dir}/{shader_file}-dbg.spv"])
    shutil.rmtree(shader_temp_dir)

compile_shaders("../assets/shaders/src", args.output)
compile_shaders("../assets/shaders/src/built-in", f"{args.output}/built-in")
