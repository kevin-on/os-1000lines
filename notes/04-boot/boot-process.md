# RISC-V Boot Process: Who Loads the Kernel?

This note clarifies the boot process for the [Boot](https://operating-system-in-1000-lines.vercel.app/en/04-boot) chapter, specifically addressing "who loads the kernel into memory and how does execution start?"

---

## The Simple Mental Model

To run an OS:

1. Load the OS binary from storage into memory
2. Set the program counter (`pc`) to the kernel entry point

This is correct, but incomplete. The CPU cannot magically read disks—something must run first to do the loading.

---

## What Happens at Power-On

| Step | What runs | Privilege | What it does |
|------|-----------|-----------|--------------|
| 1 | Boot ROM | M-mode | CPU starts at reset vector; ROM selects boot source, loads next stage |
| 2 | FSBL/SPL | M-mode | Initializes DRAM, loads OpenSBI + bootloader into RAM |
| 3 | OpenSBI | M-mode | Provides SBI runtime, hands off to next stage |
| 4 | U-Boot | S-mode | Reads kernel from storage, loads it into RAM, jumps to kernel |
| 5 | Linux/Kernel | S-mode | Your OS starts |

**Key insight**: OpenSBI does not read disks. It provides runtime services and transfers control. The actual kernel loading is done by a bootloader (typically U-Boot) that has storage drivers.

---

## How "Setting PC" Works

There is no special "set PC" instruction. The boot code simply jumps:

```asm
# Pseudocode: bootloader jumps to kernel
lui   t0, %hi(kernel_entry)
addi  t0, t0, %lo(kernel_entry)
jalr  x0, t0, 0                  # PC = t0, no return
```

The CPU starts executing at the new address. For Linux, the bootloader also sets:
- `a0` = hart ID
- `a1` = device tree blob (DTB) pointer

---

## OpenSBI Firmware Types

OpenSBI comes in three variants that determine how it finds the next stage:

| Type | How it knows the next entry | Who loaded the kernel? |
|------|----------------------------|------------------------|
| FW_JUMP | Fixed address compiled in | Previous stage (SPL, emulator) |
| FW_DYNAMIC | Previous stage passes address in `a2` | Previous stage (SPL) |
| FW_PAYLOAD | Kernel is embedded in OpenSBI image | Build-time bundling |

### FW_JUMP (common in QEMU)

OpenSBI jumps to a hardcoded address after M-mode setup. Something else must have placed the next-stage code there.

```
[Boot ROM] → [SPL loads everything] → [OpenSBI runs] → jump to fixed address
```

### FW_DYNAMIC (common on real boards)

SPL loads both OpenSBI and U-Boot into RAM, then tells OpenSBI where U-Boot is via a data structure:

```
[Boot ROM] → [SPL] → loads OpenSBI + U-Boot
                   → passes fw_dynamic_info to OpenSBI
                   → [OpenSBI] → jumps to U-Boot → [U-Boot loads kernel]
```

### FW_PAYLOAD (combined firmware)

The kernel bytes are baked into the OpenSBI firmware image. OpenSBI "loads" it because it's already there, not because it read storage.

```
[Boot ROM] → [OpenSBI with kernel payload] → sret to kernel
```

---

## The QEMU Tutorial Case

In the tutorial's `run.sh`:

```bash
$QEMU -machine virt -bios default -nographic -serial mon:stdio \
      --no-reboot -kernel kernel.elf
```

QEMU itself acts as the loader:

```
QEMU loads:
  ├─ OpenSBI (via -bios default) → placed at 0x80000000
  └─ kernel.elf (via -kernel) → placed at 0x80200000

OpenSBI (FW_DYNAMIC mode):
  └─ jumps to 0x80200000 (boot function)

Your kernel:
  └─ boot() → kernel_main()
```

QEMU reads your `kernel.elf` file and places it at `0x80200000`. OpenSBI then jumps there. No storage drivers needed—the emulator does the loading.

---

## Real Hardware vs QEMU

| Aspect | QEMU | Real board |
|--------|------|------------|
| Who loads kernel? | QEMU itself (`-kernel` option) | U-Boot with SD/eMMC drivers |
| Where is kernel? | Host filesystem | SD card, eMMC, network |
| OpenSBI type | Usually FW_DYNAMIC | Varies by board |
| Complexity | Minimal | Full boot chain |

---

## Visual Summary

```
Power on / Reset
      │
      ▼
┌─────────────────────────────────┐
│ Boot ROM (M-mode, in SoC)       │
│ - selects boot source           │
│ - loads FSBL/SPL into SRAM      │
└─────────────────────────────────┘
      │
      ▼
┌─────────────────────────────────┐
│ FSBL / SPL (M-mode)             │
│ - initializes DRAM              │
│ - loads OpenSBI into DRAM       │
│ - loads U-Boot into DRAM        │
└─────────────────────────────────┘
      │
      ▼
┌─────────────────────────────────┐
│ OpenSBI (M-mode runtime)        │
│ - provides SBI services         │
│ - delegates traps to S-mode     │
│ - jumps to U-Boot               │
└─────────────────────────────────┘
      │
      ▼
┌─────────────────────────────────┐
│ U-Boot (S-mode bootloader)      │
│ - has storage/filesystem drivers│
│ - loads kernel from SD/eMMC     │
│ - sets a0=hartid, a1=DTB        │
│ - jumps to kernel entry         │
└─────────────────────────────────┘
      │
      ▼
┌─────────────────────────────────┐
│ Kernel (S-mode)                 │
│ - your OS starts here           │
└─────────────────────────────────┘
```

---

## Summary

- The CPU starts at a fixed reset vector pointing to Boot ROM
- Boot ROM and SPL initialize hardware and load subsequent stages
- OpenSBI provides M-mode services but does **not** read storage
- A bootloader (U-Boot) or the emulator loads the kernel into RAM
- "Setting PC" is just a jump instruction to the kernel entry point
- In QEMU, `-kernel` flag makes the emulator load your kernel; OpenSBI jumps to it

