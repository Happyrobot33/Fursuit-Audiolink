#!/usr/bin/env python3
"""
Protobuf code generator using nanopb.
Compiles audiolink_data.proto to C source files using nanopb.
Keeps files in sync across project directories.
"""

import os
import sys
import subprocess
import shutil
from pathlib import Path


def install_nanopb():
    """Install nanopb if not already installed."""
    print("Installing nanopb and protobuf...")
    result = subprocess.run(
        [sys.executable, '-m', 'pip', 'install', 'nanopb', 'protobuf'],
        capture_output=True,
        text=True
    )
    if result.returncode == 0:
        print("✓ nanopb and protobuf installed successfully")
        return True
    else:
        print(f"✗ Failed to install: {result.stderr}")
        return False


def get_nanopb_generator():
    """Find nanopb generator script."""
    try:
        import nanopb
        nanopb_dir = os.path.dirname(nanopb.__file__)
        generator = os.path.join(nanopb_dir, 'generator', 'nanopb_pb2.py')
        
        # Try protoc approach first
        proto_plugin = os.path.join(nanopb_dir, 'generator', 'protoc-gen-nanopb')
        if os.path.exists(proto_plugin):
            return proto_plugin, 'plugin'
        
        # Fallback to generator script
        generator_py = os.path.join(nanopb_dir, 'generator', 'nanopb_generator.py')
        if os.path.exists(generator_py):
            return generator_py, 'script'
        
        print(f"✓ nanopb found at: {nanopb_dir}")
        return nanopb_dir, 'path'
    except ImportError:
        return None, 'none'


def find_protoc():
    """Find protoc executable."""
    import shutil
    
    # Try standard PATH lookup first
    protoc = shutil.which('protoc') or shutil.which('protoc.exe')
    if protoc:
        print(f"✓ Found protoc: {protoc}")
        return protoc
    
    # Try common locations for pip-installed protoc
    python_dir = os.path.dirname(sys.executable)
    common_paths = [
        os.path.join(python_dir, 'Scripts', 'protoc.exe'),
        os.path.join(python_dir, 'protoc.exe'),
        os.path.join(python_dir, 'bin', 'protoc'),
    ]
    
    for path in common_paths:
        if os.path.exists(path):
            print(f"✓ Found protoc: {path}")
            return path
    
    # Try finding protoc from installed grpc_tools
    try:
        from grpc_tools import protoc as grpc_protoc
        # grpc_tools has protoc bundled
        return 'grpc_protoc'  # Special marker
    except ImportError:
        pass
    
    return None


def generate_with_nanopb_plugin(proto_file, output_dir, protoc_path):
    """Generate C code using protoc with nanopb plugin."""
    if protoc_path == 'grpc_protoc':
        # Use grpc_tools.protoc
        print(f"Generating C code from proto using grpc_tools protoc...")
        cmd = [
            sys.executable,
            '-m', 'grpc_tools.protoc',
            f'--nanopb_out={output_dir}',
            f'--proto_path={os.path.dirname(proto_file)}',
            proto_file
        ]
    else:
        proto_dir = os.path.dirname(proto_file)
        print(f"Generating C code from proto using nanopb plugin...")
        
        cmd = [
            protoc_path,
            f'--nanopb_out={output_dir}',
            f'--proto_path={proto_dir}',
            proto_file
        ]
    
    print(f"  Command: {' '.join(cmd[:4])}...")
    result = subprocess.run(cmd, capture_output=True, text=True)
    
    if result.returncode == 0:
        print(f"✓ Code generation successful")
        return True
    else:
        print(f"✗ protoc error: {result.stderr[:200]}")
        return False


def generate_with_nanopb_generator(proto_file, output_dir):
    """Generate C code using nanopb generator script."""
    try:
        import nanopb
        nanopb_dir = os.path.dirname(nanopb.__file__)
        generator_py = os.path.join(nanopb_dir, 'generator', 'nanopb_generator.py')
        
        if not os.path.exists(generator_py):
            print(f"✗ nanopb_generator.py not found at {generator_py}")
            return False
        
        print(f"Generating C code using nanopb generator script...")
        
        # Run the nanopb generator
        cmd = [
            sys.executable,
            generator_py,
            proto_file
        ]
        
        result = subprocess.run(
            cmd,
            capture_output=True,
            text=True,
            cwd=os.path.dirname(proto_file)
        )
        
        if result.returncode == 0:
            print(f"✓ Code generation successful")
            # Generator outputs files in same directory as proto
            return True
        else:
            print(f"✗ Generator error: {result.stderr}")
            return False
    except ImportError:
        return False


def copy_files(src_base, dest_dirs):
    """Copy generated .pb.h and .pb.c files to destination directories."""
    pb_h = f"{src_base}.pb.h"
    pb_c = f"{src_base}.pb.c"
    
    any_copied = False
    
    for dest_dir in dest_dirs:
        if not os.path.exists(dest_dir):
            print(f"  ✗ Destination directory does not exist: {dest_dir}")
            continue
        
        if os.path.exists(pb_h):
            dest_h = os.path.join(dest_dir, os.path.basename(pb_h))
            shutil.copy2(pb_h, dest_h)
            print(f"  ✓ {os.path.basename(pb_h)} → {os.path.basename(dest_dir)}")
            any_copied = True
        else:
            print(f"  ✗ Source {pb_h} not found")
        
        if os.path.exists(pb_c):
            dest_c = os.path.join(dest_dir, os.path.basename(pb_c))
            shutil.copy2(pb_c, dest_c)
            print(f"  ✓ {os.path.basename(pb_c)} → {os.path.basename(dest_dir)}")
            any_copied = True
        else:
            print(f"  ✗ Source {pb_c} not found")
    
    return any_copied


def main():
    # Setup paths
    script_dir = os.path.dirname(os.path.abspath(__file__))
    proto_file = os.path.join(script_dir, 'audiolink_data.proto')
    output_dir = script_dir
    src_file_base = os.path.join(output_dir, 'audiolink_data')
    
    # Destination directories
    dest_dirs = [
        os.path.join(script_dir, 'hello_world', 'main'),
        os.path.join(script_dir, 'sender_test', 'main'),
    ]
    
    print("=" * 70)
    print("Protobuf Code Generator (nanopb)")
    print("=" * 70)
    print(f"Proto file:        {proto_file}")
    print(f"Output directory:  {output_dir}")
    print(f"Destinations:      {len(dest_dirs)} project directories")
    print()
    
    # Check if proto file exists
    if not os.path.exists(proto_file):
        print(f"✗ Proto file not found: {proto_file}")
        return 1
    
    # Step 1: Install and check nanopb
    print("Step 1: Setting up nanopb...")
    
    generator, gen_type = get_nanopb_generator()
    if not generator:
        print("  nanopb not found, installing...")
        if not install_nanopb():
            print("✗ Failed to install nanopb")
            return 1
        generator, gen_type = get_nanopb_generator()
        if not generator:
            print("✗ nanopb still not available")
            return 1
    
    print(f"  ✓ nanopb ready")
    print()
    
    # Step 2: Generate C code
    print("Step 2: Generating C code from proto...")
    
    generated = False  # Track if we actually generated new files
    
    # Try protoc with nanopb plugin first
    protoc_path = find_protoc()
    if protoc_path:
        print(f"  ✓ Found protoc")
        if generate_with_nanopb_plugin(proto_file, output_dir, protoc_path):
            generated = True
    else:
        print("  ⚠ protoc not found in PATH, trying nanopb generator...")
    
    # Fallback to nanopb generator script if protoc not available
    if not generated:
        print("  Attempting nanopb generator (requires protoc in PATH)...")
        if generate_with_nanopb_generator(proto_file, output_dir):
            generated = True
        else:
            print("  ⚠ nanopb generator needs protoc. Falling back to existing files...")
    
    print()
    
    # Step 3: Verify generated files (or sync from project if generation failed)
    print("Step 3: Verifying/syncing generated files...")
    pb_h = f"{src_file_base}.pb.h"
    pb_c = f"{src_file_base}.pb.c"
    
    # If generation failed, try to sync from existing project files
    if not generated or not os.path.exists(pb_c):
        print("  Syncing from project directory...")
        for dest_dir in dest_dirs:
            src_pb_c = os.path.join(dest_dir, 'audiolink_data.pb.c')
            if os.path.exists(src_pb_c):
                print(f"    Found existing: {src_pb_c}")
                shutil.copy2(src_pb_c, pb_c)
                print(f"    ✓ Synced to root")
                break
    
    if os.path.exists(pb_h):
        size_h = os.path.getsize(pb_h)
        print(f"  ✓ {os.path.basename(pb_h)} ({size_h} bytes)")
    else:
        print(f"  ✗ {pb_h} not found")
        return 1
    
    if os.path.exists(pb_c):
        size_c = os.path.getsize(pb_c)
        print(f"  ✓ {os.path.basename(pb_c)} ({size_c} bytes)")
    else:
        print(f"  ✗ {pb_c} not found")
        return 1
    
    print()
    
    # Step 4: Copy to project directories
    print("Step 4: Syncing files to project directories...")
    if copy_files(src_file_base, dest_dirs):
        print()
        if generated:
            print("=" * 70)
            print("✓ Generation and sync complete!")
            print("  Generated new C files from proto using nanopb")
            print("=" * 70)
        else:
            print("=" * 70)
            print("✓ Sync complete (using existing implementations)")
            print("  Note: Install protoc to regenerate from proto")
            print("=" * 70)
        return 0
    else:
        print()
        print("✗ No files were copied")
        return 1


if __name__ == '__main__':
    sys.exit(main())
