# README

## Local Development

# 1. Install QEMU
```bash
brew install qemu
```

# 2. Create working directory
```bash
mkdir ~/mte-vm && cd ~/mte-vm
```

# 3. Download Debian nocloud arm64 image
```bash
curl -LO https://cloud.debian.org/images/cloud/trixie/latest/debian-13-nocloud-arm64.qcow2
```

# 4. Resize disk
```bash
qemu-img resize debian-13-nocloud-arm64.qcow2 20G
```

# 5. Locate EFI firmware
```bash
FIRMWARE=$(find $(brew --prefix) -name "edk2-aarch64-code.fd" 2>/dev/null | head -1)
echo "Firmware found at: $FIRMWARE"
```

# 6. Create writable flash vars
```bash
dd if=/dev/zero of=flash_vars.img bs=1m count=64
```

# 7. Boot the VM
```bash
qemu-system-aarch64 \
  -machine virt,mte=on \
  -cpu max \
  -accel tcg \
  -m 4G \
  -smp 2 \
  -nographic \
  -drive if=pflash,format=raw,file=$FIRMWARE,readonly=on \
  -drive if=pflash,format=raw,file=flash_vars.img \
  -drive if=virtio,format=qcow2,file=debian-13-nocloud-arm64.qcow2 \
  -netdev user,id=net0,hostfwd=tcp::2222-:22 \
  -device virtio-net-pci,netdev=net0
  ```

# 8. Log in (at console prompt)
#    Username: root

# 9. Inside the VM — install tools and verify MTE
```bash
apt update && apt install -y gcc make
cat /proc/cpuinfo | grep -i mte
```

# 10. From host — copy source files to VM
#     May need to do `ssh-keygen -R "[localhost]:2222" first`
```bash
scp -P 2222 my_alloc.c my_alloc_mte.c my_alloc.h my_alloc_internal.h test.c test_mte.c root@localhost:~/
```

# 11. Inside the VM — compile and run
```bash
gcc -DMTE_ENABLED -march=armv8.5-a+memtag -o test_mte my_alloc.c my_alloc_mte.c test_mte.c
./test_mte
```