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

def compileShaders(shader_src_dir, shader_out_dir):
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
        subprocess.check_output(["glslc", "-c", f"{shader_src_path}", "--target-env=vulkan1.2", "-Werror", "-o", f"{shader_temp_dir}/{shader_out_name}.spv"])
        subprocess.check_output(["glslc", "-c", f"{shader_src_path}", "--target-env=vulkan1.2", "-Werror", "-g", "-O0", "-o", f"{shader_temp_dir}/{shader_out_name}-dbg.spv"])
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

compileShaders("../assets/shaders/src", args.output)
compileShaders("../assets/shaders/src/built-in", f"{args.output}/built-in")
