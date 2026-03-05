# qbtctl
---
**qbtctl** is a minimal, ultra fast command-line interface for controlling [qBittorrent](https://www.qbittorrent.org/) via its Web API.  

It allows you to manage torrents directly from the terminal, supporting operations like:  
- Pausing, resuming, stopping, and removing torrents  
- Setting categories, tags, and sequential download  
- Monitoring torrent upload/download limits, state, and seed time  
- Interactive authentication setup or command-line credentials  

Designed to be **compact, single-binary, and portable**, easy to compile and deploy.  
Lightweight, yet powerful — perfect for scripting or managing your qBittorrent instance from the terminal.

Download the latest precompiled binary: [qbtctl](https://github.com/creptic/qbtctl/releases/download/v1.0/qbtctl)
or see latest package.

---

## Requirements for Compiling

Before compiling and using `qbtctl`, make sure your system has:

- **C compiler** (`cc`, `gcc`, or `clang`)  
- **libcurl**   
- **libsodium** 
- POSIX-compliant OS (Linux, macOS, etc.)  
- Makefile included. run make to compile. To compile for portability (static) use the docker command at bottom of this page.

## Compiling (see below for static build with docker)
Clone the repository:
```bash
git clone https://github.com/creptic/qbtctl.git
cd qbtctl
make
./qbtctl --setup  
```
## Authentication Setup
---
`qbtctl` requires your qBittorrent Web API credentials to function. You can set them up interactively or pass them via CLI flags.

Checks for auth.txt the in the following order;
- ~/.qbtctl/auth.txt
- */qbtctl-directory/auth.txt>, 
- or you can use -c <custompath/auth.txt>
- **command line arguements will always override auth.txt**
- If no auth.txt is found, it will use the defaults with admin as password.

## Interactive Setup
---
Run:
```bash
./qbtctl --setup
```
You will be prompted to enter:

- **qBittorrent URL** (default: `http://localhost`)
- **port** (default: `8080`)  
- **Username** (default: `admin`)  
- **Password** (empty allowed)  

You can also specify a custom path for the auth file. By default, credentials are stored in:
```text
~/.qbtctl/auth.txt
```

### Command-Line Overrides
---
- You can override stored credentials directly when running commands:

```bash
./qbtctl --url http://host:8080 --user admin --pass mypass [option/getter/setter/or action]
```

## Options
---
| Option | Description |
|------|-------------|
| `--help` | Show help message |
| `-i, --setup` | Setup server credentials interactively (save/test `auth.txt`) |
| `-c <path>` | Alternate `auth.txt` path (ex: `/home/user/auth.txt`) |
| `-r, --raw` | Raw output mode (bytes, seconds, true/false) |
| `--url <url>` | qBittorrent WebUI URL (ex: `http://localhost:8080`) |
| `--user <user>` | qBittorrent username (default is `admin`) |
| `--pass <pass>` | qBittorrent password (`NULL` allowed) |
| `-h, --hash <hash>` | Torrent hash (minimum 6 chars) |
| `-a, --show-all` | Show basic torrent info, all torrents |
| `-c, --show-all-clean` | Show basic torrents info, all torrents (borderless) |
| `-aj, --show-all-json` | Show all torrents info as JSON |
| `-s, --show-single` | Show basic single torrent info (requires hash) |
| `-sj, --show-single-json` | Show single torrent info as JSON (requires hash) |

### Actions (requires hash) (per torrent)
---
| Command | Description |
|--------|-------------|
| `-am <path>`, `--move <path>` | Move torrent to `<path on server>` *(use with caution)* |
| `-as`, `--start` | Start torrent |
| `-af`, `--force-start` | Force start torrent |
| `-ap`, `--pause` | Pause / stop torrent |
| `-ar`, `--remove` | Stop and remove torrent *(does not delete data)* |
| `-ad`, `--delete` | Stop, remove and **delete torrent and data** *(use with caution)* |

## Getters (requires --hash <hash>)
---
| Command | Standard Output | -r / --raw Output |
|---------|-----------------|-------------------|
| -gn, --get-name | Ubuntu.ISO | "Ubuntu.ISO" |
| -gt, --get-tags | Linux,ISO | ["Linux","ISO"] |
| -gc, --get-category | Linux ISOs | "Linux ISOs" |
| -gu, --get-up-limit | 512 *(kbs)* | 524288 *(bytes)* |
| -gd, --get-dl-limit | 1024 *(kbs)* | 1048576 *(bytes)* |
| -gdp, --get-dl-path | /downloads/linux/iso.iso | "/downloads/linux/iso.iso" |
| -gr, --get-ratio | 2.0 | 2.0 |
| -gs, --get-seedtime | 12:34:56 *(DD:HH:MM)* | 45296 *(seconds)*|
| -gsl, --get-seedtime-limit | 24:00:00 *(DD:HH:MM)* | 86400 *(seconds)* |
| -gsd, --get-seqdl | 0 *(1=ON;0=OFF)* | false |
| -gat, --get-autotmm | 0 *(1=ON;0=OFF)* | false |
| -gss, --get-superseed | 1 *(1=ON;0=OFF)* | true |
| -gtr, --get-tracker | tracker.example.com | "tracker.example.com:port/announce/etc" |
| -gtl, --get-tracker-list | tracker1,tracker2 | ["udp.tracker1:port/etc","tracker2:port/etc"]  |
| -gp, --get-private | 1 *(1=YES;0=NO)* | true |
| -gst, --get-status | ForcedUP | "ForcedUP" |

## Setters (requires --hash <hash>)
---
| Command | Standard Input | -r / --raw Input |
|---------|----------------|------------------|
| -sc <cat>, --set-category <cat> | Linux | Linux |
| -st <tag>, --set-tags <tag> | Linux,ISO | Linux,ISO |
| -sul <#>, --set-up-limit <#> | 512 *(kbs)* | 524288 *(seconds)* |
| -sdl <#>, --set-dl-limit <#> | 1024 *(kbs)* | 1048576 |
| -srl <#>, --set-ratio-limit <#> | 2.00000 | 2.00000 |
| -sst <#>, --set-seedtime-limit <#> | 24:00:00 *(DD:HH:MM)* | 86400 *(seconds)* |
| -ssd <val>, --set-seqdl <val> | 1 *(1=ON;0=OFF)* | true |
| -sat <val>, --set-autotmm <val> | 0 *(1=ON;0=OFF)* | false |
| -sss <val>, --set-superseed <val> | 0 *(1=ON;0=OFF)* | false |

---

## Compiling static link with Docker (cd into the source dir)
```
docker run --rm -v $(pwd):/out alpine:latest /bin/sh -c "
    # Install build tools and dependencies
    apk add --no-cache build-base musl-dev zlib-static pkgconf wget tar libsodium-dev libsodium-static &&
    
    # Build curl statically
    cd /tmp &&
    wget https://curl.se/download/curl-8.17.0.tar.gz &&
    tar xzf curl-8.17.0.tar.gz &&
    cd curl-8.17.0 &&
    ./configure --disable-shared --enable-static --without-ssl --disable-ntlm --disable-ldap --disable-ldaps \
                --disable-ftp --disable-file --disable-dict --disable-telnet --disable-pop3 --disable-imap \
                --disable-smtp --disable-gopher --disable-manual --disable-psl --without-libpsl &&
    make -j\$(nproc) &&

    # Build qbtctl statically
    cd /out &&
    gcc -O2 -static -s \
        -I/tmp/curl-8.17.0/include \
        -I/usr/include \
        -L/tmp/curl-8.17.0/lib/.libs \
        -L/usr/lib \
        -o qbtctl *.c \
        -lcurl -lsodium -lz -lm -ldl -lpthread
"
```
---
