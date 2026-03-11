#include <stdio.h>
#include "help.h"

void show_help()
{
    printf("qbtctl - qBittorrent torrent CLI Tool\n");
    printf("Usage:\n");
    printf("  qbtctl [options] [getters setters or actions]\n");
    printf("**Options:\n");
    printf("  --help               Show this help message\n");
    printf("  -i, --setup          Setup server credentials interactively (save/test auth.txt)\n");
    printf("  -c <path>            Alternate auth.txt path (ex: /home/user/auth.txt)\n");
    printf("  -r, --raw            Raw output mode (bytes, seconds, true/false\n");
    printf("  --url <url>          qBittorrent WebUI URL (ex: http://localhost:8080)\n");
    printf("  --user <user>        qBittorrent username (default is admin)\n");
    printf("  --pass <pass>        qBittorrent password (NULL allowed)\n");
    printf("  -h, --hash <hash>    Torrent hash (minimum 6 chars)\n");
    printf("  -a, --show-all       Show basic torrent info, all torrents\n");
    printf("  -c, --show-all-clean Show basic torrents info, all torrents (borderless)\n");
    printf("  -aj, --show-all-json Show all torrents info as json\n");
    printf("  -s, --show-single    Show basic single torrent info (requires hash)\n");
    printf("  -sj, --show-single-json   Show single torrent info as json (requires hash)\n");
    printf("**Actions (requires hash) (per torrent)\n");
    printf("  -am <path>, --move <path> Move torrent to <path on server> (use with caution)\n");
    printf("  -as, --start         Start torrent\n");
    printf("  -af, --force-start   Force start torrent\n");
    printf("  -ap, --pause         Pause/Stop torrent\n");
    printf("  -ar, --remove        Stop  and remove torrent (do not delete)\n");
    printf("  -ad, --delete        Stop, remove and DELETE torrent (*use with caution*)\n");
    printf("**Setters (requires hash) (per torrent)\n");
    printf("  -sc <cat>, --set-category <cat> Set category\n");
    printf("  -st <tag>, --set-tags <tag>     Set tags (CSV, null to clear) (sorted alpha) \n");
    printf("  -sul <#>, --set-up-limit <#>    Set Upload limit (kb) (raw bytes)\n");
    printf("  -sdl <#>, --set-dl-limit <#>    Set Download limit (kb) (raw bytes)\n");
    printf("  -srl <#>, --set-ratio-limit <#> Set ratio limit (#.#)\n");
    printf("  -sst <#>, --set-seedtime-limit <#> Set seedtime limit (DD:HH:MM) (raw secs)\n");
    printf("  -ssd <val>, --set-seqdl <val> Set SequentialDownload (0/1) (raw=true/false)\n");
    printf("  -sat <val>, --set-autotmm <val> Set Autotmm (0/1) (raw=true/false)\n");
    printf("  -sss <val>, --set-superseed <val> Set Superseed (0/1) (raw=true/false)\n");
    printf("**Getters (requires hash) (per torrent)\n");
    printf("  -gn, --get-name      Get torrent name\n");
    printf("  -gt, --get-tags      Get torrent tags\n");
    printf("  -gc, --get-category  Get torrent category\n");
    printf("  -gul, --get-up-limit Get upload limit in (raw=bytes)\n");
    printf("  -gdl, --get-dl-limit Get download limit (raw=bytes)\n");
    printf("  -gdp, --get-dl-path  Get download path\n");
    printf("  -grl, --get-ratio-limit  Get ratio limit\n");
    printf("  -gs, --get-seedtime  Get seedtime (raw=seconds)\n");
    printf("  -gsl,--get-seedtime-limit Get seedtime limit (raw=seconds)\n");
    printf("  -gsd, --get-seqdl    Get Sequential download flag (raw=true/false)\n");
    printf("  -gat, --get-autotmm  Get Auto TMM flag (raw=true/false)\n");
    printf("  -gss,--get-superseed Get superseed flag (raw=true/false)\n");
    printf("  -gtr, --get-tracker  Get tracker URL (if empty maybe using DHT)\n");
    printf("  -gtl, --get-tracker-list  Get trackers as a CSV list\n");
    printf("  -gp, --get-private   Get private flag (raw=true/false)\n");
    printf("  -gr, --get-ratio     Get the current ratio of a torrent\n");
    printf("  -gus, --get-up-speed Get upload speed (raw=bytes)\n");
    printf("  -gds, --get-dl-speed Get download speed (raw=bytes)\n");
    printf("  -gsz, --get-size     Get torrent size (raw=bytes)\n");
    printf("  -gud, --get-uploaded Get amount uploaded (raw=bytes)\n");
    printf("  -gdd, --get-downloaded Get amount downloaded (raw=bytes)\n");
    printf("  -ge, --get-eta       Get ETA of torrent (100:00:00 = infinite)\n");
    printf("  -gst, --get-state    Get torrent state (raw=true/false)\n");
    printf("  -gpr, --get-progress Get progress of a torrent (raw=0-100)\n");
    printf("**Examples:\n");
    printf("  qbtctl --hash 198380 -gn\n");
    printf("  qbtctl --show-all\n");
    printf("  qbtctl --setup\n");
}
