Import("env")
import os

project_dir = env.subst("$PROJECT_DIR")
bmp_path = os.path.join(project_dir, "src", "assets", "default_celebrate.bmp")
asm_path = os.path.join(project_dir, "src", "assets", "default_celebrate_asset.S")

if os.path.exists(bmp_path) and os.path.exists(asm_path):
    bmp_mtime = os.path.getmtime(bmp_path)
    asm_mtime = os.path.getmtime(asm_path)

    # If the BMP file has been modified after the assembly wrapper,
    # touch the assembly file to invalidate SCons cache and force reassembly.
    if bmp_mtime > asm_mtime:
        print("[ASSETS] Detected updated default_celebrate.bmp. Invalidating assembly cache...")
        os.utime(asm_path, None)

        # Also remove any cached object file in build directory if it exists
        build_dir = env.subst("$BUILD_DIR")
        cached_obj = os.path.join(build_dir, "src", "assets", "default_celebrate_asset.S.o")
        if os.path.exists(cached_obj):
            try:
                os.remove(cached_obj)
            except OSError:
                pass
