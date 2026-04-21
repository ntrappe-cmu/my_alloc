# README

## Local Development

### Terminal 1 (Linux VM)
##### 1. Install QEMU
```bash
brew install qemu
```

##### 2. Create working directory
```bash
mkdir ~/mte-vm && cd ~/mte-vm
```

##### 3. Download Debian nocloud arm64 image
```bash
curl -LO https://cloud.debian.org/images/cloud/trixie/latest/debian-13-nocloud-arm64.qcow2
```

##### 4. Resize disk
```bash
qemu-img resize debian-13-nocloud-arm64.qcow2 20G
```

#### 5. Locate EFI firmware
```bash
FIRMWARE=$(find $(brew --prefix) -name "edk2-aarch64-code.fd" 2>/dev/null | head -1)
echo "Firmware found at: $FIRMWARE"
```

#### 6. Create writable flash vars
```bash
dd if=/dev/zero of=flash_vars.img bs=1m count=64
```

#### 7. Boot the VM
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

#### 8. Log in (at console prompt)
```bash
#    Username: root
```

#### 9. Install tools and verify MTE
```bash
apt update && apt install -y gcc make
cat /proc/cpuinfo | grep -i mte
```

#### 10. Add SSH support to port over files
```bash
apt install -y openssh-server
echo "PermitRootLogin yes" >> /etc/ssh/sshd_config
systemctl restart sshd
```

### Terminal 2 (Host)

#### 11. Copy over files using SSH
(May need to do `ssh-keygen -R "[localhost]:2222"` first)
```bash
scp -P 2222 my_alloc.c my_alloc_mte.c my_alloc.h my_alloc_internal.h test.c test_mte.c root@localhost:~/
```

### Terminal 1 (VM)
#### 11. Compile and run
```bash
gcc -DMTE_ENABLED -march=armv8.5-a+memtag -o test_mte my_alloc.c my_alloc_mte.c test_mte.c
./test_mte
```

## Fuzzing
### Setup
#### 1. In the VM, install tools.
```bash
apt install clang
```

#### 2. Create harness and port over.
```bash
scp -P 2222 fuzz_harness.c root@localhost:~/
```

#### 2. In the VM, compile the three configurations
```bash
clang -fsanitize=fuzzer -o fuzz_vanilla fuzz_harness.c my_alloc.c
clang -fsanitize=fuzzer,address -o fuzz_asan fuzz_harness.c my_alloc.c
clang -fsanitize=fuzzer -DMTE_ENABLED -march=armv8.5-a+memtag \
  -o fuzz_mte fuzz_harness.c my_alloc.c my_alloc_mte.c
```

#### 3. Run and compare
```bash
mkdir corpus_vanilla corpus_asan corpus_mte
timeout 60 ./fuzz_vanilla corpus_vanilla/ 2>&1 | tee vanilla.log
timeout 60 ./fuzz_asan corpus_asan/ 2>&1 | tee asan.log
timeout 60 ./fuzz_mte corpus_mte/ 2>&1 | tee mte.log
```

## dlmalloc Usage

If you want to use the bundled dlmalloc directly without colliding with the
system allocator, compile both the implementation and your caller with
`USE_DL_PREFIX` defined. That gives you `dlmalloc`, `dlfree`, `dlcalloc`, and
`dlrealloc` instead of the libc names.

```bash
cd dlmalloc
gcc -DUSE_DL_PREFIX -o dltest main.c malloc.c
./dltest
```

If you want to stay inside this project’s allocator API, include
`my_alloc.h` and call `my_malloc` / `my_free` instead of the libc symbols.
That is the safer option if you want to avoid any naming conflict with the
system allocator on macOS.

```c
#include "my_alloc.h"

int main(void) {
  my_alloc_init();
  void *ptr = my_malloc(16);
  my_free(ptr);
  my_alloc_destroy();
  return 0;
}
```
