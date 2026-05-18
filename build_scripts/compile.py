"""
Compiles Goo Engine with patches
"""

import subprocess


def run_compile():

    def install_packages():
        subprocess.run("python build_files/build_environment/install_linux_packages.py", shell=True, check=True)
        subprocess.run("python build_files/utils/make_update.py --use-linux-libraries", shell=True, check=True)
        pass


    def patch_files():
        patch_locations = {
            "GridBuilder.h": "lib/linux_x64/openvdb/include/nanovdb/util/GridBuilder.h",
            "childrenProxy.h": "lib/linux_x64/usd/include/pxr/usd/sdf/childrenProxy.h",
            "buildinfo.c": "source/creator/buildinfo.c",
            "OpenColorIO.h": "lib/linux_x64/opencolorio/include/OpenColorIO/OpenColorIO.h",
        }

        for key, value in patch_locations.items():
            src = open(f"build_scripts/diff_ref/{key}.from", "r").read()
            patched_file = open(value, "r").read()
            if src != patched_file:
                print("skipping because this file is probably patched")
            else:
                subprocess.run(f"patch -N {value} build_scripts/diff_ref/{key}.patch", shell=True, check=True)  # get file locations
        pass


    install_packages()
    patch_files()

    subprocess.run(f"make -j$(nproc)", shell=True, check=True)


