# `ecall` behaves differently depending on privilege mode

This note clarifies how `ecall` works across the [Hello World](https://operating-system-in-1000-lines.vercel.app/en/05-hello-world) and [System Call](https://operating-system-in-1000-lines.vercel.app/en/14-system-call) chapters.

Both chapters use the `ecall` instruction, but they trap to different places:

| Chapter | Code runs in | `ecall` traps to | Purpose |
|---------|--------------|------------------|---------|
| 5 (Hello World) | S-mode (kernel) | M-mode (SBI/firmware) | Print characters via SBI |
| 14 (System Call) | U-mode (user app) | S-mode (kernel) | Invoke system calls |

## RISC-V privilege levels

RISC-V defines three privilege levels:

| Level | Name | Typical use |
|-------|------|-------------|
| M | Machine mode | Firmware (OpenSBI) |
| S | Supervisor mode | OS kernel |
| U | User mode | Applications |

When the CPU executes `ecall`, it raises an exception. Where that exception is handled depends on **delegation** settings configured by M-mode firmware via the `medeleg` register.

In the OpenSBI + tutorial setup:

```
U-mode  ──ecall──►  S-mode   (delegated to kernel via medeleg)
S-mode  ──ecall──►  M-mode   (handled by SBI)
```

OpenSBI delegates U-mode `ecall` exceptions to S-mode, so your kernel's trap handler receives them. S-mode `ecall` exceptions are not delegated, so they go to M-mode where SBI handles them.

## Design philosophy

Why is `ecall` designed this way?

**Single controlled entry point**: `ecall` is the *only* way to request services from a higher privilege level. There's no way to jump directly into kernel or firmware code—you must go through the trap handler. This lets each layer validate requests before doing anything.

**Each layer only trusts the layer above it**: User apps can't bypass the kernel to access protected resources. In this tutorial, the kernel uses SBI for certain platform services (console, timer), so those requests go through firmware.

**Uniform mechanism at every boundary**: The same `ecall` instruction works for both "app → kernel" and "kernel → firmware". One instruction, one trap mechanism, reused at every privilege boundary.

**Minimal hardware complexity**: The CPU doesn't need to understand system call numbers or SBI extensions. It just traps to the configured handler and lets software sort out what the caller wanted.

## Who sets the trap handlers?

Each privilege level has its own trap vector register:

| Register | Privilege | Who sets it |
|----------|-----------|-------------|
| `stvec` | S-mode | Your kernel, via `WRITE_CSR(stvec, ...)` |
| `mtvec` | M-mode | OpenSBI, before your kernel starts |

Your kernel runs in S-mode, so it **cannot access M-mode registers** like `mtvec`. When QEMU boots, OpenSBI sets up `mtvec` to point to its own trap handler, then jumps to your kernel. This is why your kernel can call SBI via `ecall` without ever configuring M-mode.

## Summary

- `ecall` in S-mode (kernel) → traps to M-mode (handled by SBI)
- `ecall` in U-mode (user app) → traps to S-mode (handled by your kernel, due to delegation)
- The destination depends on the current privilege level and `medeleg` settings
- You only configure `stvec`; OpenSBI takes care of `mtvec` and `medeleg`
