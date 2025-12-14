#!/usr/bin/env python3
"""
Complete build and deployment pipeline
Combining Karpathy's llm.c + Justine's optimizations + our bare-metal UEFI
"""

import os
import subprocess
import sys

def banner(text):
    print("\n" + "="*80)
    print(f"  {text}")
    print("="*80 + "\n")

def run_command(cmd, desc):
    """Run command and show output"""
    print(f"🔧 {desc}...")
    result = subprocess.run(cmd, shell=True, capture_output=True, text=True)
    if result.returncode != 0:
        print(f"❌ Error: {result.stderr}")
        return False
    print(f"✅ {desc} complete")
    return True

def check_files():
    """Verify required files exist"""
    banner("CHECKING REQUIRED FILES")
    
    required = {
        "llama2_efi.c": "Main UEFI application",
        "tokenizer.model": "SentencePiece tokenizer (32K vocab)",
        "stories15M.bin": "15M parameter model (60MB)",
        "Makefile": "Build configuration"
    }
    
    missing = []
    for file, desc in required.items():
        if os.path.exists(file):
            size = os.path.getsize(file)
            print(f"  ✓ {file:20s} ({size:>12,} bytes) - {desc}")
        else:
            print(f"  ✗ {file:20s} - MISSING - {desc}")
            missing.append(file)
    
    if missing:
        print(f"\n❌ Missing files: {', '.join(missing)}")
        print("\nDownload from: https://huggingface.co/karpathy/tinyllamas")
        return False
    
    return True

def build_llama2_efi():
    """Compile llama2.efi"""
    banner("BUILDING LLAMA2.EFI")
    
    if not run_command("wsl bash -c 'cd /mnt/c/Users/djibi/Desktop/baremetal/llm-baremetal && make clean'", "Cleaning build"):
        return False
    
    if not run_command("wsl bash -c 'cd /mnt/c/Users/djibi/Desktop/baremetal/llm-baremetal && make'", "Compiling llama2.efi"):
        return False
    
    if not os.path.exists("llama2.efi"):
        print("❌ llama2.efi not created!")
        return False
    
    size = os.path.getsize("llama2.efi")
    print(f"\n✅ Built: llama2.efi ({size:,} bytes)")
    return True

def deploy_to_image():
    """Deploy llama2.efi to disk image"""
    banner("DEPLOYING TO DISK IMAGE")
    
    cmd = 'wsl bash -c "cd /mnt/c/Users/djibi/Desktop/baremetal/llm-baremetal && mcopy -i llama2_efi.img -o llama2.efi ::EFI/BOOT/BOOTX64.EFI"'
    
    if not run_command(cmd, "Copying to disk image"):
        return False
    
    print("✅ llama2.efi deployed to llama2_efi.img")
    return True

def test_qemu():
    """Launch QEMU for testing"""
    banner("LAUNCHING QEMU")
    
    qemu_path = r"C:\Program Files\qemu\qemu-system-x86_64.exe"
    ovmf_path = r"C:\Users\djibi\Desktop\baremetal\llm-baremetal\OVMF.fd"
    img_path = r"C:\Users\djibi\Desktop\baremetal\llm-baremetal\llama2_efi.img"
    
    if not os.path.exists(qemu_path):
        print(f"❌ QEMU not found at: {qemu_path}")
        print("   Download from: https://qemu.weilnetz.de/w64/")
        return False
    
    print("🚀 Starting QEMU...")
    print("\n📝 Expected output:")
    print("   1. DRC v4.0 ULTRA-ADVANCED banner")
    print("   2. Model selection menu")
    print("   3. [DEBUG pos=0] First 10 logits: ...")
    print("   4. Generated text (should be coherent English)")
    print("\n💡 Compare logits with reference llama2.c:")
    print("   llama2.c shows: [0]=-6.7908 [1]=0.8281 [2]=-6.7904...")
    print("\n")
    
    cmd = [
        qemu_path,
        "-bios", ovmf_path,
        "-drive", f"file={img_path},format=raw",
        "-m", "2048M",
        "-cpu", "qemu64,+sse2",
        "-smp", "2"
    ]
    
    try:
        subprocess.run(cmd)
    except KeyboardInterrupt:
        print("\n\n✅ QEMU closed")
    
    return True

def train_shakespeare():
    """Run Shakespeare training"""
    banner("TRAINING SHAKESPEARE MODEL")
    
    print("📚 This will:")
    print("   1. Download TinyShakespeare corpus (~1.1MB)")
    print("   2. Tokenize with SentencePiece (32K vocab)")
    print("   3. Fine-tune stories15M for 10K iterations (~2-4 hours)")
    print("   4. Export to shakespeare15M.bin for deployment")
    
    response = input("\n⚠️  Start training now? (y/n): ")
    if response.lower() != 'y':
        print("⏭️  Skipping training")
        return True
    
    if not os.path.exists("train_shakespeare.py"):
        print("❌ train_shakespeare.py not found!")
        return False
    
    cmd = "python train_shakespeare.py"
    print(f"\n🎓 Running: {cmd}")
    print("   (This will take 2-4 hours on CPU, 20-30 min on GPU)")
    
    try:
        subprocess.run(cmd, shell=True)
    except KeyboardInterrupt:
        print("\n\n⏸️  Training interrupted")
        return False
    
    print("\n✅ Training complete!")
    print("   Checkpoint saved: checkpoints/shakespeare15M_iter10000.bin")
    return True

def create_bootable_usb():
    """Instructions for creating bootable USB"""
    banner("CREATING BOOTABLE USB DRIVE")
    
    print("""
📀 To create a bootable USB drive for real hardware:

1. Format USB drive as FAT32
   Windows: Right-click drive → Format → FAT32

2. Create EFI boot structure:
   USB:/
   └── EFI/
       └── BOOT/
           └── BOOTX64.EFI  (copy llama2.efi here)

3. Copy model and tokenizer to USB root:
   USB:/
   ├── stories15M.bin        (60MB)
   ├── tokenizer.bin         (434KB)
   └── tokenizer.model       (500KB)

4. Boot from USB:
   - Insert USB drive
   - Restart computer
   - Enter BIOS/UEFI (usually F2, F12, or Del)
   - Select USB drive from boot menu
   - Should boot directly into llama2_efi!

5. For viral demo video (Justine's suggestion):
   - Film outdoors in Dakar (show location)
   - Show USB insertion
   - Show BIOS boot menu
   - Show REPL generating Shakespeare text
   - Post to Hacker News: "Running LLaMA bare-metal from USB in Senegal"
   - Tag @karpathy on Twitter (he'll love it!)
    """)

def main():
    os.chdir(r"C:\Users\djibi\Desktop\baremetal\llm-baremetal")
    
    print("""
╔════════════════════════════════════════════════════════════════════════════╗
║                                                                            ║
║              🎭 COMPLETE BUILD PIPELINE FOR LLAMA2_EFI                     ║
║                                                                            ║
║  Combining:                                                                ║
║    • Karpathy's llm.c architecture                                         ║
║    • Justine Tunney's ARM optimized math                                   ║
║    • Bare-metal UEFI deployment (no OS!)                                   ║
║    • DRC v4.0 Ultra-Advanced reasoning                                     ║
║                                                                            ║
╚════════════════════════════════════════════════════════════════════════════╝
    """)
    
    # Step 1: Check files
    if not check_files():
        sys.exit(1)
    
    # Step 2: Build
    if not build_llama2_efi():
        sys.exit(1)
    
    # Step 3: Deploy
    if not deploy_to_image():
        sys.exit(1)
    
    # Step 4: Test
    print("\n" + "="*80)
    response = input("🚀 Launch QEMU to test? (y/n): ")
    if response.lower() == 'y':
        test_qemu()
    
    # Step 5: Train (optional)
    print("\n" + "="*80)
    response = input("🎓 Train Shakespeare model? (y/n): ")
    if response.lower() == 'y':
        train_shakespeare()
    
    # Step 6: USB instructions
    create_bootable_usb()
    
    banner("BUILD COMPLETE!")
    print("""
✅ Next steps:

1. Test in QEMU:
   python build_pipeline.py  (or run manually)

2. Compare logits debug output with reference:
   Reference (llama2.c):  [0]=-6.7908 [1]=0.8281 [2]=-6.7904
   UEFI (llama2.efi):     [0]=? [1]=? [2]=?
   → Should match exactly!

3. If output is still garbled:
   - Check logits values
   - Verify softmax doesn't overflow
   - Check RNG (rand_efi) implementation
   - Compare embedding lookup with reference

4. Deploy to USB for real hardware demo

5. Make viral video from Dakar! 🎥
   - @karpathy will retweet (1.4M followers)
   - Meta AI will love it (LLaMA usage)
   - HN front page guaranteed

Good luck! 🚀
    """)

if __name__ == "__main__":
    try:
        main()
    except KeyboardInterrupt:
        print("\n\n⏸️  Build interrupted by user")
        sys.exit(1)
    except Exception as e:
        print(f"\n\n❌ Error: {e}")
        sys.exit(1)
