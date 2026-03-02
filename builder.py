import os
import subprocess
import sys
import glob
import shutil
import re
from scikit_build_core.build import build_wheel as _build_wheel
from scikit_build_core.build import build_sdist as _build_sdist
from scikit_build_core.build import get_requires_for_build_wheel as _get_requires_for_build_wheel
from scikit_build_core.build import get_requires_for_build_sdist as _get_requires_for_build_sdist

LIBSESSION_REPO = "https://github.com/oxen-io/libsession-util.git"

def run_command(cmd, cwd=None, env=None):
    print(f">>> [Builder] Executing: {' '.join(cmd)}")
    subprocess.run(cmd, check=True, cwd=cwd, env=env)

def ensure_dependencies(root):
    util_path = os.path.join(root, "libsession-util")
    if not os.path.exists(os.path.join(util_path, "CMakeLists.txt")):
        print(f">>> [Builder] Cloning libsession-util...")
        if os.path.exists(util_path): shutil.rmtree(util_path)
        run_command(["git", "clone", "--recursive", LIBSESSION_REPO, util_path])

def patch_for_compatibility(root):
    print(">>> [Builder] Applying compatibility patches...")
    util_path = os.path.join(root, "libsession-util")
    
    # 1. Aggressive CMake version patch (< 3.5) + Policy Fix
    # This is MANDATORY for the pip build environment because it uses a very recent CMake
    # that has dropped support for legacy versions (like the 2.8.12 used in zstd).
    for root_dir, dirs, files in os.walk(util_path):
        for file in files:
            if file == "CMakeLists.txt":
                fpath = os.path.join(root_dir, file)
                with open(fpath, "r") as f: content = f.read()
                
                # Match VERSION 2.x or 3.0-3.4, handling optional args like FATAL_ERROR
                pattern = r"cmake_minimum_required\s*\(\s*VERSION\s+([0-2]\.[0-9.]+|3\.[0-4](\.[0-9.]+)?)[^)]*\)"
                if re.search(pattern, content, re.I):
                    new_content = re.sub(pattern, "cmake_minimum_required(VERSION 3.5)", content, flags=re.I)
                    
                    # Force CMP0069 for IPO on main file (Required for CMake 4.x)
                    if fpath == os.path.join(util_path, "CMakeLists.txt"):
                        if "CMP0069" not in new_content:
                            new_content = new_content.replace("cmake_minimum_required(VERSION 3.5)", 
                                                            "cmake_minimum_required(VERSION 3.5)\ncmake_policy(SET CMP0069 NEW)")
                    
                    if new_content != content:
                        print(f">>> [Builder] Patched {fpath}")
                        with open(fpath, "w") as f: f.write(new_content)

def run_build_sh(root):
    ensure_dependencies(root)
    patch_for_compatibility(root)
    
    print(">>> [Builder] Running build.sh...")
    env = os.environ.copy()
    env["SAF_SKIP_MAIN_BUILD"] = "ON"
    env["SAF_USE_SYSTEM_DEPS"] = "ON"
    
    cmd = ["bash", "build.sh"]
    if os.name == "nt":
        msys_bash = r"C:\msys64\usr\bin\bash.exe"
        if os.path.exists(msys_bash):
            env["MSYSTEM"] = "MINGW64"
            env["CHERE_INVOKING"] = "1"
            cmd = [msys_bash, "-lc", "bash build.sh"]
            
    run_command(cmd, cwd=root, env=env)

def get_cmake_args(root):
    lib_dir = os.path.join(root, "libsession-util")
    build_dir = os.path.join(lib_dir, "Build")
    
    # EXACT logic and paths from test_python_binding.sh
    oxenc_inc = os.path.join(lib_dir, "external", "oxen-libquic", "external", "oxen-encoding")
    fmt_inc = os.path.join(lib_dir, "external", "oxen-logging", "fmt", "include")
    spdlog_inc = os.path.join(lib_dir, "external", "oxen-logging", "spdlog", "include")
    proto_inc = os.path.join(lib_dir, "proto")
    
    inc_dirs = [
        os.path.join(lib_dir, "include"),
        oxenc_inc,
        fmt_inc,
        spdlog_inc,
        proto_inc
    ]
    
    # Collect static libs from Build folder like test_python_binding.sh does
    libs = []
    for ext in ["**/*.a", "**/*.lib"]:
        libs.extend(glob.glob(os.path.join(build_dir, ext), recursive=True))
    libs = list(set(libs))
    
    if any("libprotobuf-lite" in l for l in libs):
        libs = [l for l in libs if not l.endswith("libprotobuf.a") and not l.endswith("libprotobuf.lib")]

    if sys.platform.startswith("linux"):
        lib_list = ";".join(libs)
        lib_str = f"-Wl,--whole-archive;{lib_list};-Wl,--no-whole-archive"
    else:
        lib_str = ";".join(libs)

    # Find pybind11 CMake directory (from test_python_binding.sh)
    import pybind11
    pybind11_cmake_dir = pybind11.get_cmake_dir()

    return [
        f"-DLIBSESSION_INCLUDE_DIRS={';'.join(inc_dirs)}",
        f"-DLIBSESSION_LIBRARIES={lib_str}",
        f"-Dpybind11_DIR={pybind11_cmake_dir}",
        "-DSAF_BUILD_PYTHON=ON",
        "-DSAF_BUILD_EXAMPLES=OFF",
        "-DCMAKE_POSITION_INDEPENDENT_CODE=ON"
    ]

def build_wheel(wheel_directory, config_settings=None, metadata_directory=None):
    root = os.path.abspath(os.getcwd())
    run_build_sh(root)
    if config_settings is None: config_settings = {}
    args = config_settings.get("cmake.args", [])
    if isinstance(args, str): args = [args]
    config_settings["cmake.args"] = list(args) + get_cmake_args(root)
    return _build_wheel(wheel_directory, config_settings, metadata_directory)

def build_sdist(sdist_directory, config_settings=None):
    return _build_sdist(sdist_directory, config_settings)

def get_requires_for_build_wheel(config_settings=None):
    return _get_requires_for_build_wheel(config_settings)

def get_requires_for_build_sdist(config_settings=None):
    return _get_requires_for_build_sdist(config_settings)

def prepare_metadata_for_build_wheel(metadata_directory, config_settings=None):
    root = os.path.abspath(os.getcwd())
    run_build_sh(root)
    if config_settings is None: config_settings = {}
    args = config_settings.get("cmake.args", [])
    if isinstance(args, str): args = [args]
    config_settings["cmake.args"] = list(args) + get_cmake_args(root)
    from scikit_build_core.build import prepare_metadata_for_build_wheel as _prepare
    return _prepare(metadata_directory, config_settings)
