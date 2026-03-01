# qbtctl

**qbtctl** is a minimal, crash-proof command-line interface for controlling [qBittorrent](https://www.qbittorrent.org/) via its Web API.  

It allows you to manage torrents directly from the terminal, supporting operations like:  
- Pausing, resuming, stopping, and removing torrents  
- Setting categories, tags, and sequential download  
- Monitoring torrent upload/download limits, state, and seed time  
- Interactive authentication setup or command-line credentials  

Designed to be **compact, single-binary, and portable**, easy to compile and deploy.  
Lightweight, yet powerful — perfect for scripting or managing your qBittorrent instance from the terminal.

## Table of Contents

1. [About](#qbtctl)
2. [Requirements](#requirements)
3. [Installation & Compilation](#installation--compilation)
4. [Authentication Setup](#authentication)
5. [Usage Examples](#usage-examples)
6. [File Structure](#file-structure)
7. [Notes](#notes)
8. [License](#license)

## Requirements

Before compiling and using `qbtctl`, make sure your system has:

- **C compiler** (`cc`, `gcc`, or `clang`)  
- **libcurl** (sudo pacman -S curl on arch) (sudo apt-get install libcurl4-openssl-dev on debian)  
- POSIX-compliant OS (Linux, macOS, etc.)  

---

## Installation & Compilation

Clone the repository:

```bash
git clone https://github.com/creptic/qbtctl.git
cd qbtctl
make 
```
## Authentication Setup

`qbtctl` requires your qBittorrent Web API credentials to function. You can set them up interactively or pass them via CLI flags.

`qbtctl` checks for auth.txt the following order;
- ~/.qbtctl/auth.txt 
- */qbtctl-directory/auth.txt>, 
- or you can use -c <custompath/auth.txt>
- **command line arguements will always override auth.txt**
- If no auth.txt is found, it will use the defaults with admin as password.

### Interactive Setup

Run:
```bash
./qbtctl --setup
```
You will be prompted to enter:

- **qBittorrent URL** (default: `http://localhost:8080`)  
- **Username** (default: `admin`)  
- **Password** (empty allowed)  

You can also specify a custom path for the auth file. By default, credentials are stored in:

```text
~/.qbtctl/auth.txt
```

### Command-Line Overrides

- You can override stored credentials directly when running commands:

```bash
./qbtctl --url http://host:8080 --user admin --pass mypass [action]
