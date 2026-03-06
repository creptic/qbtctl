# qbtctl
[![Release](https://img.shields.io/github/v/release/creptic/qbtctl)](https://github.com/creptic/qbtctl/releases)
[![Donate](https://img.shields.io/badge/Donate-PayPal-blue.svg)](https://www.paypal.com/paypalme/crepticdev)
![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)

---

**qbtctl** is a minimal, ultra-fast command-line interface for controlling qBittorrent via its Web API.

It allows you to manage torrents directly from the terminal, supporting operations such as:

- Pausing, resuming, stopping, and removing torrents  
- Setting categories, tags, sequential download, and superseeding 
- Monitoring torrent upload/download limits, state, and seed time  
- Interactive authentication setup or command-line credentials  

Designed to be **compact, single-binary, and portable**, making it easy to compile and deploy.

Lightweight yet powerful — perfect for scripting or managing your qBittorrent instance from the terminal.

## 📥 Download

Download the latest **precompiled binary (x86 / x86_64 Linux)**
```bash
wget https://github.com/creptic/qbtctl/releases/latest/download/qbtctl
chmod +x qbtctl
```
https://github.com/creptic/qbtctl/releases/latest/download/qbtctl

---

💡 **Quick Reference / Cheat Sheet**

Most common commands and flags are listed in the [**Options**](#options) section below.  
Refer there for full details and all available flags.

---

# Requirements for Compiling

Before compiling `qbtctl`, ensure your system has:

- **C compiler** (`cc`, `gcc`, or `clang`)
- **libcurl** (for connecting to qbittorent api)
- **libsodium** (for encrypting/decrypting password) 
- POSIX-compatible OS (Linux, macOS, etc.)

A Makefile is included.

Compile with:

```bash
make
```

For a portable [**static build**](#static), see the Docker section below.

---

# Compiling

Clone the repository:

```bash
git clone https://github.com/creptic/qbtctl.git
cd qbtctl
make
./qbtctl --setup
```

---

# Authentication Setup

`qbtctl` requires qBittorrent Web API credentials.

Credentials can be provided:

- via **auth.txt** (or a file saved with --setup using then using -c filename)
- via **command-line arguments** (overides file credentials)

Auth file search order:

1. `~/.qbtctl/auth.txt`
2. `./auth.txt` (inside the qbtctl directory)
3. Custom path using `-c <path>`
---

# Interactive Setup

Run:

```bash
./qbtctl --setup
```

You will be prompted for:

- qBittorrent URL (default: `http://localhost`)
- Port (default: 8080)
- Username (default: admin)
- Password (cannot be empty)

Note: To save to program directory just type ```auth.txt``` or alternate filename, when saving in -i or --setup

- Use full filename when saving, it will not create if pointed to a directory only.
      
By default credentials are saved to:
```text
~/.qbtctl/auth.txt
```
---

# Command-Line Overrides

You may override credentials when running commands:

```bash
./qbtctl --url http://host:8080 --user admin --pass mypass [command]
```

---
# 💡 Usage Examples

These examples assume you have already set up credentials with `./qbtctl --setup`.

---

## 1. Show all torrents

```bash
./qbtctl -a
```

**Example output:**

```text

+------------------------------------------+--------+--------------+--------------+--------------+--------------+--------------+----------+------------------+
| Name                                     | Hash   | UL Limit(Kb) | DL Limit(Kb) | State        | Seed Time    | Limit D:H:M  | Category | Tags             |
+------------------------------------------+--------+--------------+--------------+--------------+--------------+--------------+----------+------------------+
| Ubuntu.ISO                               | 9c3289 | 0            | 0            | stalledUP    | 01:01:01     | 04:04:04     | ISO      | ISO, Debian      |

```

With raw output (bytes, seconds, true/false):

```bash
./qbtctl -r -a
```

```text
+------------------------------------------+--------+--------------+--------------+--------------+--------------+--------------+----------+------------------+
| Name                                     | Hash   | UL Limit (b) | DL Limit (b) | State        | Seed Time    | Limit(secs)  | Category | Tags             |
+------------------------------------------+--------+--------------+--------------+--------------+--------------+--------------+----------+------------------+
| Ubuntu.ISO                               | 9c3289 | 0            | 0            | stalledUP    | 90104        | 360240       | ISO      | ISO, Debian      |

```

---

## 2. Show single torrent info (with or without raw)

```bash
./qbtctl -s -h 9c3289
```

**Example output:**

```text
+------------------------------------------+
Name: Ubuntu.ISO
Hash: 9c3289027f9760e0f3fcfd2df40e0d80b2350191
Tags: ISO, Debian
Category: ISO
Upload Limit: 0
Download Limit: 0
Full Path: /incoming/Ubuntu.iso
Ratio Limit: 99.99
Seedtime: 01:01:01
Seedtime Limit: 04:04:04
Sequential Download: 0
Auto TMM: 0
Superseed: 0
Tracker: http://tracker.foo.com
Private: 0
State: stalledUP
+------------------------------------------+

```

With raw JSON:

```bash
./qbtctl -sj -h 9c3289
```

```json
{"name":"Ubuntu.ISO","state":"stalledUP","dl_limit":1048576,"ul_limit":524288,"ratio":2.0,"seed_time":45296,....etc}
```

---

## 3. Modify torrent settings

Set category and tags:

```bash
./qbtctl -sc Linux -st Linux,ISO -h 9c3289
```

Set upload and download limits:

```bash
./qbtctl -sul 1024 -sdl 2048 -h 9c3289
```

Enable sequential download and superseed:

```bash
./qbtctl -ssd 1 -sss 1 -h 9c3289
```

---

## 4. Pause, resume, or remove torrents

Pause a torrent:

```bash
./qbtctl -ap -h 9c3289
```

Resume a torrent:

```bash
./qbtctl -as -h 9c3289
```

Remove torrent (keep data):

```bash
./qbtctl -ar -h 9c3289
```

Delete torrent and data:

```bash
./qbtctl -ad -h 9c3289
```

---

## 5. Move torrent data

```bash
./qbtctl -am /downloads/linux -h 9c3289
```

Moves the files to `/downloads/linux` on the server.

---

## 6. Quick non-interactive run

If you want to run commands directly with credentials:

```bash
./qbtctl --url http://localhost:8080 --user admin --pass mypass -a
```

---

## 7. Docker One-Liner (Static Build & Run)

```bash
docker run --rm -v $(pwd):/out alpine:latest /bin/sh -c "
apk add --no-cache build-base musl-dev zlib-static pkgconf wget tar libsodium-dev libsodium-static &&
cd /tmp &&
wget https://curl.se/download/curl-8.17.0.tar.gz &&
tar xzf curl-8.17.0.tar.gz &&
cd curl-8.17.0 &&
./configure --disable-shared --enable-static --without-ssl --disable-ntlm --disable-ldap --disable-ldaps \
            --disable-ftp --disable-file --disable-dict --disable-telnet --disable-pop3 --disable-imap \
            --disable-smtp --disable-gopher --disable-manual --disable-psl --without-libpsl &&
make -j$(nproc) &&
cd /out &&
gcc -O2 -static -s \
-I/tmp/curl-8.17.0/include \
-L/tmp/curl-8.17.0/lib/.libs \
-o qbtctl *.c \
-lcurl -lsodium -lz -lm -ldl -lpthread &&
./qbtctl --setup
"
```

# Options

```bash
--help                   Show help message
-i, --setup              Setup server credentials interactively
-c <path>                Alternate auth file path
-r, --raw                Raw output mode (bytes, seconds, true/false)
--url <url>              qBittorrent WebUI URL (ex: http://localhost:8080)
--user <user>            qBittorrent username (default: admin)
--pass <pass>            qBittorrent password (NULL allowed)
-h, --hash <hash>        Torrent hash (minimum 6 characters)
-a, --show-all           Show basic torrent info for all torrents
-c, --show-all-clean     Show torrents info (borderless output)
-aj, --show-all-json     Show all torrents info as JSON
-s, --show-single        Show single torrent info (requires hash)
-sj, --show-single-json  Show single torrent info as JSON (requires hash)
```

---

# Actions (requires --hash)

```bash
-am <path>, --move <path>  Move torrent to <path> on the server
-as, --start               Start torrent
-af, --force-start         Force start torrent
-ap, --pause               Pause or stop torrent
-ar, --remove              Stop and remove torrent (keeps data)
-ad, --delete              Stop, remove, and delete torrent and data
```

---

# Getters (requires --hash)

```bash
# Command                   Standard Output              --raw Output
-gn, --get-name             Ubuntu.ISO                   "Ubuntu.ISO"
-gt, --get-tags             Linux,ISO                    ["Linux","ISO"]
-gc, --get-category         Linux ISOs                   "Linux ISOs"
-gu, --get-up-limit         512 (KB/s)                   524288
-gd, --get-dl-limit         1024 (KB/s)                  1048576
-gdp, --get-dl-path         /downloads/linux/iso.iso     "/downloads/linux/iso.iso"
-gr, --get-ratio            2.0                          2.0
-gs, --get-seedtime         12:34:56                     45296
-gsl,--get-seedtime-limit   24:00:00                     86400
-gsd, --get-seqdl           0 (OFF)                      false
-gat, --get-autotmm         0 (OFF)                      false
-gss, --get-superseed       1 (ON)                       true
-gtr, --get-tracker         tracker.example.com          "tracker.example.com:port/announce"
-gtl, --get-tracker-list    tracker1,tracker2            ["udp.tracker1:port","tracker2:port"]
-gp, --get-private          1 (YES)                      true
-gst, --get-status          ForcedUP                     "ForcedUP"
```

---
# Setters (requires --hash)
```bash
# Command                             Standard Input  --raw Input
-sc <cat>, --set-category <cat>       Linux           Linux
-st <tag>, --set-tags <tag>           Linux,ISO       Linux,ISO
-sul <#>, --set-up-limit <#>          512 (KB/s)      524288
-sdl <#>, --set-dl-limit <#>          1024 (KB/s)     1048576
-srl <#>, --set-ratio-limit <#>       2.0             2.0
-sst <#>, --set-seedtime-limit <#>    24:00:00        86400
-ssd <val>, --set-seqdl <val>         1 (ON)          true
-sat <val>, --set-autotmm <val>       0 (OFF)         false
-sss <val>, --set-superseed <val>     0 (OFF)         false
```
## ⚠ Exit / Error Codes

```bash
# Exit Codes for qbtctl
0  EXIT_OK           # Success
1  EXIT_AUTH_FAIL    # Authentication failed
2  EXIT_BAD_ARGS     # Invalid command-line arguments
3  EXIT_NETWORK      # Network or connection error
4  EXIT_API_FAIL     # API request failed
5  EXIT_FILE_ERROR   # File read/write failed
```
Usage Example:
```
./qbtctl -a
if [ $? -ne 0 ]; then
    echo "[ERROR] qbtctl encountered an error!"
fi
```
Notes:

These codes are returned by the CLI when commands fail.

Useful for scripting and automated workflows.

Always check $? immediately after a command for accurate results.

---

# Static Build Using Docker

Run this inside the source directory:

```bash
docker run --rm -v $(pwd):/out alpine:latest /bin/sh -c "
apk add --no-cache build-base musl-dev zlib-static pkgconf wget tar libsodium-dev libsodium-static &&
cd /tmp &&
wget https://curl.se/download/curl-8.17.0.tar.gz &&
tar xzf curl-8.17.0.tar.gz &&
cd curl-8.17.0 &&
./configure --disable-shared --enable-static --without-ssl --disable-ntlm --disable-ldap --disable-ldaps \
            --disable-ftp --disable-file --disable-dict --disable-telnet --disable-pop3 --disable-imap \
            --disable-smtp --disable-gopher --disable-manual --disable-psl --without-libpsl &&
make -j$(nproc) &&
cd /out &&
gcc -O2 -static -s \
-I/tmp/curl-8.17.0/include \
-L/tmp/curl-8.17.0/lib/.libs \
-o qbtctl *.c \
-lcurl -lsodium -lz -lm -ldl -lpthread
"
```
---

# ☕ Support / Donate 
If you enjoy **qbtctl**, consider buying me a coffee

[![Donate](https://img.shields.io/badge/Donate-PayPal-blue.svg)](https://www.paypal.com/paypalme/crepticdev)

---
# Notes
- Not tested with SSL (https://), I added some code but needs testing.
- Special thank you to the cJSON drop in from https://github.com/DaveGamble/cJSON

---
