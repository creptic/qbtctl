# qbtctl

---

**qbtctl** is a minimal, ultra-fast command-line interface for controlling qBittorrent via its Web API.

It allows you to manage torrents directly from the terminal, supporting operations such as:

- Pausing, resuming, stopping, and removing torrents  
- Setting categories, tags, and sequential download  
- Monitoring torrent upload/download limits, state, and seed time  
- Interactive authentication setup or command-line credentials  

Designed to be **compact, single-binary, and portable**, making it easy to compile and deploy.

Lightweight yet powerful — perfect for scripting or managing your qBittorrent instance from the terminal.

Download the latest precompiled binary:

https://github.com/creptic/qbtctl/releases/download/v0.99/qbtctl

or see the latest release packages.

---

# Requirements for Compiling

Before compiling `qbtctl`, ensure your system has:

- **C compiler** (`cc`, `gcc`, or `clang`)
- **libcurl** (for connecting to qbittorent api)
- **libsodium** (for encrypting/decrypting password) 
- POSIX-compatible OS (Linux, macOS, etc.)

A Makefile is included.

Compile with:

```
make
```

For a portable **static build**, see the Docker section below.

---

# Compiling

Clone the repository:

```
git clone https://github.com/creptic/qbtctl.git
cd qbtctl
make
./qbtctl --setup
```

---

# Authentication Setup

`qbtctl` requires qBittorrent Web API credentials.

Credentials can be provided:

- via **auth.txt**
- via **command-line arguments**

Auth file search order:

1. `~/.qbtctl/auth.txt`
2. `./auth.txt` (inside the qbtctl directory)
3. Custom path using `-c <path>`

Command-line credentials **always override auth.txt**.

If no auth file is found, defaults are used:

```
username: admin
password: (empty)
```

---

# Interactive Setup

Run:

```
./qbtctl --setup
```

You will be prompted for:

- qBittorrent URL (default: `http://localhost`)
- Port (default: 8080)
- Username (default: admin)
- Password (empty allowed)

Credentials are saved to:

```
~/.qbtctl/auth.txt
```

---

# Command-Line Overrides

You may override credentials when running commands:

```
./qbtctl --url http://host:8080 --user admin --pass mypass [command]
```

---

# Options

| Option | Description |
|------|-------------|
| --help | Show help message |
| -i, --setup | Setup server credentials interactively |
| -c <path> | Alternate auth.txt path |
| -r, --raw | Raw output mode (bytes, seconds, true/false) |
| --url <url> | qBittorrent WebUI URL (ex: `http://localhost:8080`) |
| --user <user> | qBittorrent username (default: admin) |
| --pass <pass> | qBittorrent password (NULL allowed) |
| -h, --hash <hash> | Torrent hash (minimum 6 characters) |
| -a, --show-all | Show basic torrent info for all torrents |
| -c, --show-all-clean | Show torrents info (borderless output) |
| -aj, --show-all-json | Show all torrents info as JSON |
| -s, --show-single | Show single torrent info (requires hash) |
| -sj, --show-single-json | Show single torrent info as JSON |

---

# Actions (requires --hash)

| Command | Description |
|--------|-------------|
| -am <path>, --move <path> | Move torrent to `<path>` on the server |
| -as, --start | Start torrent |
| -af, --force-start | Force start torrent |
| -ap, --pause | Pause or stop torrent |
| -ar, --remove | Stop and remove torrent (keeps data) |
| -ad, --delete | Stop, remove, and delete torrent and data |

---

# Getters (requires --hash)

| Command | Standard Output | --raw Output |
|--------|-----------------|-------------|
| -gn, --get-name | Ubuntu.ISO | "Ubuntu.ISO" |
| -gt, --get-tags | Linux,ISO | ["Linux","ISO"] |
| -gc, --get-category | Linux ISOs | "Linux ISOs" |
| -gu, --get-up-limit | 512 (KB/s) | 524288 (bytes) |
| -gd, --get-dl-limit | 1024 (KB/s) | 1048576 (bytes) |
| -gdp, --get-dl-path | /downloads/linux/iso.iso | "/downloads/linux/iso.iso" |
| -gr, --get-ratio | 2.0 | 2.0 |
| -gs, --get-seedtime | 12:34:56 | 45296 |
| -gsl, --get-seedtime-limit | 24:00:00 | 86400 |
| -gsd, --get-seqdl | 0 (1=ON,0=OFF) | false |
| -gat, --get-autotmm | 0 (1=ON,0=OFF) | false |
| -gss, --get-superseed | 1 (1=ON,0=OFF) | true |
| -gtr, --get-tracker | tracker.example.com | "tracker.example.com:port/announce" |
| -gtl, --get-tracker-list | tracker1,tracker2 | ["udp.tracker1:port","tracker2:port"] |
| -gp, --get-private | 1 (1=YES,0=NO) | true |
| -gst, --get-status | ForcedUP | "ForcedUP" |

---

# Setters (requires --hash)

| Command | Standard Input | --raw Input |
|--------|---------------|-------------|
| -sc <cat>, --set-category <cat> | Linux | Linux |
| -st <tag>, --set-tags <tag> | Linux,ISO | Linux,ISO |
| -sul <#>, --set-up-limit <#> | 512 (KB/s) | 524288 |
| -sdl <#>, --set-dl-limit <#> | 1024 (KB/s) | 1048576 |
| -srl <#>, --set-ratio-limit <#> | 2.0 | 2.0 |
| -sst <#>, --set-seedtime-limit <#> | 24:00:00 | 86400 |
| -ssd <val>, --set-seqdl <val> | 1 (1=ON,0=OFF) | true |
| -sat <val>, --set-autotmm <val> | 0 (1=ON,0=OFF) | false |
| -sss <val>, --set-superseed <val> | 0 (1=ON,0=OFF) | false |

---

# Static Build Using Docker

Run this inside the source directory:

```
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
