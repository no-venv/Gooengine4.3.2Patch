#!/bin/bash
set -e

# Reference variables
REPO_URL="https://github.com/dillongoostudios/goo-engine.git"
WRAPPER_DIR=$(pwd)

# This might change depending on future lib values, change if needed
SOURCE_DIR="$WRAPPER_DIR/goo-engine"
LIB_DIR="$SOURCE_DIR/lib/linux_x64"
DIFF_REF_DIR="$WRAPPER_DIR/diff_ref"
LOCATIONS_FILE="$DIFF_REF_DIR/_file_locations.txt"

echo "=== Starting Goo Engine Build Process ==="


echo "Installing Linux system packages..."
python build_files/build_environment/install_linux_packages.py

# Regenerate the patch files just in case.
if [ -f "$WRAPPER_DIR/generate_patches.sh" ]; then
    chmod +x "$WRAPPER_DIR/generate_patches.sh"
    "$WRAPPER_DIR/generate_patches.sh"
fi

echo "Downloading precompiled libraries..."
python build_files/utils/make_update.py --use-linux-libraries

echo "Renaming webp folder in libraries..."
if [ -d "$LIB_DIR/webp" ]; then
    if [ -d "$LIB_DIR/libwebp" ]; then
        rm -rf "$LIB_DIR/libwebp"
    fi
    mv "$LIB_DIR/webp" "$LIB_DIR/libwebp"
fi

# Apply Patches
echo "Applying remaining patches from manifest..."

while read -r name rel_path; do
    [[ "$name" =~ ^#.*$ ]] && continue
    [ -z "$name" ] && continue

    if [ "$name" == "make_update.py" ]; then
        continue
    fi

    apply_patch_from_manifest "$name"

done < "$LOCATIONS_FILE"

# Compile
echo "Starting Compilation (make)..."
cd "$SOURCE_DIR"
make -j$(nproc)

echo "=== Build Complete ==="
