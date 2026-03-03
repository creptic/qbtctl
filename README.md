# qbtctl

**qbtctl** is a minimal, ultra fast command-line interface for controlling [qBittorrent](https://www.qbittorrent.org/) via its Web API.  

It allows you to manage torrents directly from the terminal, supporting operations like:  
- Pausing, resuming, stopping, and removing torrents  
- Setting categories, tags, and sequential download  
- Monitoring torrent upload/download limits, state, and seed time  
- Interactive authentication setup or command-line credentials  

Designed to be **compact, single-binary, and portable**, easy to compile and deploy.  
Lightweight, yet powerful — perfect for scripting or managing your qBittorrent instance from the terminal.

## Requirements for Compiling

Before compiling and using `qbtctl`, make sure your system has:

- **C compiler** (`cc`, `gcc`, or `clang`)  
- **libcurl** (sudo pacman -S curl on arch) (sudo apt-get install libcurl4-openssl-dev on debian)  
- POSIX-compliant OS (Linux, macOS, etc.)  

---

## Installation & Compilation (prebuilt qbtctl included)

Clone the repository:

```bash
git clone https://github.com/creptic/qbtctl.git
cd qbtctl
make (if you want to compile)
chmod +x qbtctl
./qbtctl --setup  
```
## Authentication Setup

`qbtctl` requires your qBittorrent Web API credentials to function. You can set them up interactively or pass them via CLI flags.

`qbtctl` checks for auth.txt the following order;
- ~/.qbtctl/auth.txt
- */qbtctl-directory/auth.txt>, 
- or you can use -c <custompath/auth.txt>
- **command line arguements will always override auth.txt**
- If no auth.txt is found, it will use the defaults with admin as password.

## Interactive Setup

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
```

### Examples: Limits & Sequential Download (Get and Set)

| Standard Output             | `-raw` Output         |
|-----------------------------|---------------------|
| UL Limit (KB): **1024**         | **1048576** (bytes)     |
| DL Limit (KB): **2048**         | **2097152** (bytes)     |
| Seed Time Limit (DD:HH:MM): **02:00:00**   | **7200** (seconds)       |
| Sequential Download: **1** | **true**                   |

## Exit Codes

| Code | Macro Name           | Description |
|------|----------------------|-------------|
| 0    | `EXIT_OK`            | Successful execution |
| 1    | `EXIT_LOGIN_FAIL`    | Login/authentication failed |
| 2    | `EXIT_FETCH_FAIL`    | Failed to fetch data from API |
| 3    | `EXIT_SET_FAIL`      | Failed to set/update value |
| 4    | `EXIT_BAD_ARGS`      | Invalid or missing command-line arguments |

```
./qbtctl --help
qbtctl - qBittorrent torrent CLI Tool
Usage:
  qbtctl [options] [getters setters or actions]
**Options:
  --help               Show this help message
  -i, --setup          Setup server credentials interactively (save/test auth.txt)
  -c <path>            Alternate auth.txt path (ex: /home/user/auth.txt)
  -r, --raw            Raw output mode (bytes, seconds, true/false
  --url <url>          qBittorrent WebUI URL (ex: http://localhost:8080)
  --user <user>        qBittorrent username (default is admin)
  --pass <pass>        qBittorrent password (NULL allowed)
  -h, --hash <hash>    Torrent hash (minimum 6 chars)
  -a, --show-all       Show basic torrent info, all torrents
  -c, --show-all-clean Show basic torrents info, all torrents (borderless)
  -aj, --show-all-json Show all torrents info as json
  -s, --show-single    Show basic single torrent info (requires hash)
  -sj, --show-single-json   Show single torrent info as json (requires hash)
**Getters (requires hash) (per torrent)
  -gn, --get-name      Get torrent name
  -gt, --get-tags      Get torrent tags
  -gc, --get-category  Get torrent category
  -gu, --get-up-limit  Get upload limit
  -gd, --get-dl-limit  Get download limit
  -gdp, --get-dl-path  Get download path
  -gr, --get-ratio     Get ratio limit
  -gs, --get-seedtime  Get seedtime
  -gsl,--get-seedtime-limit Get seedtime limit
  -gsd, --get-seqdl    Get Sequential download flag
  -gat, --get-autotmm  Get Auto TMM flag
  -gss,--get-superseed Get superseed flag
  -gtr, --get-tracker  Get tracker URL
  -gtl, --get-tracker-list  Get trackers as a CSV list
  -gp, --get-private   Get private flag
  -gst, --get-status   Get torrent status
**Setters (requires hash) (per torrent)
  -sc <cat>, --set-category <cat> Set category
  -st <tag>, --set-tags <tag>     Set tags (CSV, null to clear) (sorted alpha) 
  -sul <#>, --set-up-limit <#>    Set Upload limit (kb) (raw bytes)
  -sdl <#>, --set-dl-limit <#>    Set Download limit (kb) (raw bytes)
  -srl <#>, --set-ratio-limit <#> Set ratio limit (#.#)
  -sst <#>, --set-seedtime-limit <#> Set seedtime limit (DD:HH:MM) (raw secs)
  -ssd <val>, --set-seqdl <val> Set SequentialDownload (0/1) (raw=true/false)
  -sat <val>, --set-autotmm <val> Set Autotmm (0/1) (raw=true/false)
  -sss <val>, --set-superseed <val> Set Superseed (0/1) (raw=true/false)
**Actions (requires hash) (per torrent)
  -am <path>, --move <path> Move torrent to <path on server> (use with caution)
  -as, --start         Start torrent
  -af, --force-start   Force start torrent
  -ap, --pause         Pause/Stop torrent
  -ar, --remove        Stop  and remove torrent (do not delete)
  -ad, --delete        Stop, remove and DELETE torrent (*use with caution*)
**Examples:
  qbtctl --hash 198380 -gn
  qbtctl --show-all
  qbtctl --setup
``` 

## Compiling static link with Docker (cd into the source dir)
```
docker run --rm -v $(pwd):/out alpine:latest /bin/sh -c "
    apk add --no-cache build-base musl-dev zlib-static pkgconf wget tar &&
    cd /tmp &&
    wget https://curl.se/download/curl-8.17.0.tar.gz &&
    tar xzf curl-8.17.0.tar.gz &&
    cd curl-8.17.0 &&
    ./configure --disable-shared --enable-static --without-ssl --disable-ntlm --disable-ldap --disable-ldaps \
                --disable-ftp --disable-file --disable-dict --disable-telnet --disable-pop3 --disable-imap \
                --disable-smtp --disable-gopher --disable-manual --disable-psl --without-libpsl &&
    make -j\$(nproc) &&
    cd /out &&
    gcc -O2 -static -s -I/tmp/curl-8.17.0/include -L/tmp/curl-8.17.0/lib/.libs -o qbtctl *.c -lcurl -lz
"
```
