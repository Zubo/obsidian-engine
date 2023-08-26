import os
import subprocess
import sys
import argparse

os.chdir(sys.path[0])

parser = argparse.ArgumentParser(prog='Obsidian Shader Compiler', description='Compiles glsl shaders into spir-v.')
parser.add_argument('-o', '--output', default='../build/shaders')
args = parser.parse_args()

def compileShaders(shader_src_dir, shader_output_dir):
    if not os.path.exists(shader_output_dir):
        os.makedirs(shader_output_dir)
    shader_src_file_names = [p for p in os.listdir(shader_src_dir) if os.path.isfile(os.path.join(shader_src_dir, p))]
    for shader_src_name in shader_src_file_names:
        shader_src_path = os.path.join(shader_src_dir, shader_src_name)
        shader_out_name = shader_src_name.replace(".", "-")
        subprocess.check_output(["glslc", "-c", f"{shader_src_path}", "--target-env=vulkan1.2", "-Werror", "-o", f"{shader_output_dir}/{shader_out_name}.spv"])
        subprocess.check_output(["glslc", "-c", f"{shader_src_path}", "--target-env=vulkan1.2", "-Werror", "-g", "-O0", "-o", f"{shader_output_dir}/{shader_out_name}-dbg.spv"])

compileShaders("../shaders", args.output)
compileShaders("../shaders/built-in", f"{args.output}/built-in")
