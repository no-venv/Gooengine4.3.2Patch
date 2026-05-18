"""
    Builds an AppImage of Goo Engine
"""
import os
import shutil
import subprocess
import stat

LINUX_DEPLOY_DIR = "build_scripts/linuxdeploy"
LINUX_DEPLOY = f"{LINUX_DEPLOY_DIR}/linuxdeploy-x86_64.AppImage"

BUILD_BIN_DIR = "../build_linux/bin"
WORK_DIR = "../build_linux_appimage"
APPDIR = f"{WORK_DIR}/AppDir"
OUTPUT_DIR = f"{WORK_DIR}/out"


def run_build_appimage():

    if os.path.isdir(WORK_DIR):
        print("removing existing work dir")
        shutil.rmtree(WORK_DIR)

    os.mkdir(WORK_DIR)
    os.makedirs(f"{APPDIR}/usr/bin")
    shutil.copytree(BUILD_BIN_DIR, f"{APPDIR}/usr/bin", dirs_exist_ok=True)
    # copy the icon
    shutil.copy(f"{BUILD_BIN_DIR}/blender.svg", f"{APPDIR}/blender.svg")
    # configure metadata
    shutil.copy(f"{BUILD_BIN_DIR}/blender.desktop", f"{APPDIR}/blender.desktop")

    env = os.environ.copy()
    env["VERSION"] = "latest"
    env["NO_STRIP"] = "true"
    env["PATH"] += f":{LINUX_DEPLOY_DIR}"
    st = os.stat(LINUX_DEPLOY)
    os.chmod(LINUX_DEPLOY,st.st_mode | stat.S_IEXEC)
    subprocess.run(
        f"./{LINUX_DEPLOY} --appdir {APPDIR} --executable {APPDIR}/usr/bin/blender --desktop-file {APPDIR}/blender.desktop --icon-file {APPDIR}/blender.svg --output appimage",
        env=env,
        shell=True,
    )
    pass
