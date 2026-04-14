# 🚀 Korus OK - Operating System Project

<p align="center">
  <b>Experimental • Modular • Low-Level • Educational</b>
</p>

<p align="center">
  <img src="https://img.shields.io/badge/build-xmake%20all-blue?style=for-the-badge">
  <img src="https://img.shields.io/badge/status-em%20desenvolvimento-orange?style=for-the-badge">
  <img src="https://img.shields.io/badge/license-MIT-green?style=for-the-badge">
</p>

---

# 📖 About

**Korus OK** is an experimental operating system designed for learning, research, and low-level system development.

The project focuses on building a minimal yet structured OS from scratch using modern tooling and a clean architecture approach.

---

# 🧠 Philosophy

Korus follows principles inspired by large open-source systems:

- Simplicity over complexity
- Explicit control over abstractions
- Modular and extensible architecture
- Full control of the build pipeline
- Educational clarity

---

# 🛠️ Technology Stack

### Languages
- C
- C++
- Assembly (x86)

### Toolchain
- Clang / LLVM
- NASM
- GRUB Bootloader

### Build System
- XMake (custom build system)

---

# ⚙️ XMake (Custom Builder)

XMake is a custom build system designed specifically for Korus.

It is inspired by:
- Make
- Ninja

But provides:
- Cleaner syntax
- Optimized build pipeline
- Better modularization
- Native integration with Korus structure

> ⚠️ XMake is not publicly available yet.

---

# 📁 Project Layout

```
Korus-OK/
│
├── .vscode/                # VSCode configs (debug/build)
│
├── config/                # Build configuration
│   ├── make.objects.xmk   # Object definitions
│   ├── make.rules.xmk     # Build/debug rules
│   └── make.tools.xmk     # Toolchain variables
│
├── core/                  # Kernel and system source
│
├── link.id                # Linker script
│
├── xmakefile.xmk          # Main build file
│
└── .gitignore
```

---

# 💻 Features

- GRUB-based boot process
- Kernel written in C/C++ with ASM support
- Shell-style terminal interface
- Modular build system
- Clear separation of concerns

---

# 🚧 Project Status

> **Current Status:** In Development

The system is under active development. Core features are being implemented progressively.

---

# ▶️ Build Instructions

```
xmake all
```

> Requires XMake (coming soon)

---

# 🎯 Goals

- Learn operating system internals
- Build a clean OS architecture from scratch
- Provide a playground for low-level experimentation
- Develop custom tooling (XMake)

---

# 🤝 Contributing

Contributions are welcome.

You can help by:
- Reporting bugs
- Suggesting improvements
- Submitting pull requests

---

# 📜 License

This project is licensed under the MIT License.

---

# 👨‍💻 Author

Leandro M

---

# 🌍 Future Plans

- Filesystem support
- Memory management
- Multitasking
- Drivers (keyboard, display, disk)
- Packaging system
- XMake public release

---

# ⭐ Final Notes

Korus OK is not just an OS project — it is a platform for learning, experimentation, and control over the machine.

If you are interested in low-level development, this project is for you.
