# OS in 1000 Lines

This repository follows the [Operating System in 1,000 Lines](https://operating-system-in-1000-lines.vercel.app/en/) tutorial—a minimal RISC-V OS written in C.

## Supplementary Notes

While working through the tutorial, I documented concepts that were confusing or worth deeper exploration. These notes expand on specific sections with examples, diagrams, and detailed explanations.

| Chapter | Note | Summary |
|---------|------|---------|
| 02 - RISC-V 101 | [Operand Constraints](notes/02-risc-v-101/operand-constraints.md) | How inline assembly constraints (`"r"`, `"=r"`, `"+r"`) affect compiler optimization |
| 04 - Boot | [Boot Process](notes/04-boot/boot-process.md) | Who loads the kernel into memory, OpenSBI firmware types, and the QEMU boot flow |
| 05 - Hello World | [Variadic Arguments](notes/05-hello-world/variadic-arguments.md) | How `va_*` macros work for implementing `printf` |
| 05 - Hello World | [Register Pinning](notes/05-hello-world/register-pinning.md) | Why SBI calls require pinning variables to specific registers |
| 13 - User Mode | [Stacks and SP](notes/13-user-mode/stacks-and-sp.md) | Stack pointer management across kernel and user mode, `sscratch` usage |
| 14 - System Call | [ecall Privilege Modes](notes/14-system-call/ecall-privilege-modes.md) | How `ecall` traps to different handlers depending on current privilege level |

## Building and Running

```bash
./run.sh
```

Requires QEMU with RISC-V support and a RISC-V cross-compiler toolchain.
