import os
import subprocess
import sys

os.chdir(sys.path[0])

shader_output_dir = "../build/shaders"

if not os.path.exists(shader_output_dir):
    os.makedirs(shader_output_dir)

shader_src_dir = "../shaders"

shader_src_file_names = os.listdir(shader_src_dir)

for shader_src_name in shader_src_file_names:
    shader_src_path = os.path.join(shader_src_dir, shader_src_name)
    subprocess.check_output(["glslc", "-c", f"{shader_src_path}", "-o", f"{shader_output_dir}/{shader_src_name}.spirv"])
    subprocess.check_output(["glslc", "-c", f"{shader_src_path}", "-g", "-O0", "-o", f"{shader_output_dir}/{shader_src_name}.dbg.spirv"])
