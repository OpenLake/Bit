# 🧠 bit — a tiny Git-like version control system in C++

[![C++17](https://img.shields.io/badge/C%2B%2B-17-blue.svg)](https://isocpp.org/)
[![Build](https://img.shields.io/badge/build-passing-brightgreen.svg)]()
[![License: MIT](https://img.shields.io/badge/license-MIT-lightgrey.svg)](LICENSE)

> A minimalist reimplementation of Git core concepts in modern C++17.  
> Built for learning, exploration, and hacking on version control internals.

---

## 🚀 Features

- 🏗️ **`bit init`** — initialize a new repository  
- ➕ **`bit add <file>`** — stage files for commit  
- 🧾 **`bit commit <message>`** — create commits  
- 📜 **`bit log`** — view commit history  
- 📂 **`bit status`** — check staging and changes  
- 🌿 **`bit branch` / `bit checkout`** — manage branches  
- 🔐 Uses **SHA-1** hashing and **zlib** compression (like real Git)

---

## 🗂️ Project Structure

bit/
├── include/
│ ├── repository.h # Repository operations
│ ├── object.h # Git objects (blobs, trees)
│ ├── commit.h # Commit creation and logging
│ ├── branch.h # Branch management
│ └── utils.h # Helper utilities (hashing, compression)
├── src/
│ ├── repository.cpp
│ ├── object.cpp
│ ├── commit.cpp
│ ├── branch.cpp
│ ├── utils.cpp
│ └── main.cpp # CLI entry point
├── Makefile
└── README.md

---

## 🛠️ Build Instructions

### 🔧 Prerequisites
You’ll need:
- **g++** (C++17 or newer)
- **make**
- **zlib** and **OpenSSL** libraries

For Ubuntu / Debian:
```bash
sudo apt install g++ make zlib1g-dev libssl-dev

⚙️ Build
git clone https://github.com/<your-username>/bit.git
cd bit
make

▶️ Run

Initialize a new repository:
./bit init

Add and commit files:
./bit add file.txt
./bit commit "Initial commit"

View log:
./bit log

⚙️ Example Session
$ ./bit init
Initialized empty Bit repository in /path/to/project/.bit

$ echo "hello" > test.txt
$ ./bit add test.txt
$ ./bit commit "Add test file"
Committed: Add test file (a3b4e9c...)

$ ./bit log
commit a3b4e9c...
Author: user
Message: Add test file


💡 Design Overview

Each module mirrors Git’s real internal architecture:

Module	Purpose
repository	Handles .bit structure and configuration
object	Creates and retrieves objects (blobs, trees)
commit	Stores metadata and commit history
branch	Manages refs/heads and HEAD state
utils	Handles SHA-1, compression, filesystem ops
📘 Learning Objectives

Understand how Git works under the hood

Practice C++17 filesystem, hashing, and file I/O

Learn modular architecture + Makefile builds

Explore content-addressable storage concepts

🧑‍💻 Contributing

Pull requests, suggestions, and experiments are welcome!
If you’d like to extend this project:

Add diff and merge functionality

Support remote push/pull

Implement tag and rebase commands

⚖️ License

MIT License © 2025
Free to use, modify, and share.

🌟 Acknowledgements

Inspired by:

Git Internals

Write Yourself a Git

Linus Torvalds’ original Git design

🧊 TL;DR

bit — your own mini Git, built from scratch in C++17.
Learn how commits, objects, and branches really work under the hood.