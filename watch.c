/*
 * watch.c
 * -----------------------------
 * Purpose: Live monitoring module for qbtctl. Handles fetching torrent data,
 *          formatting output, and rendering a dynamic table view with auto-refresh.
 *
 * Version: 1.5.1
 * Date:    2026-03-29
 *
 * Features:
 *   - Fetches all torrents via /api/v2/torrents/info
 *   - Displays live table with:
 *       Name, Hash, Size, Progress, ETA/Seedtime
 *       Download/Upload speeds and totals
 *       Tags, Category, State
 *   - Dynamic column sizing based on terminal width
 *   - Sort-ready structure (by download speed)
 *   - Formatted time output (DD:HH:MM)
 *   - Smart truncation for long fields
 *   - Detects ANSI-capable terminals
 *   - Graceful fallback to single snapshot for non-TTY environments
 *   - Minimal flicker using cursor repositioning
 *
 * Notes:
 *   - Uses shared helpers from qbtctl.c (fmt_bytes, qbt_get_json)
 *   - Requires valid auth context via global creds struct
 *
 * Author:  Creptic
 * GitHub:  https://github.com/creptic/qbtctl
*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include "cJSON.h"
#include "auth.h"
#include "watch.h"

/* ================== HELPERS ================== */


static int term_supports_ansi(void)
{
/**
 * term_supports_ansi()
 *
 * Returns 1 if stdout is a TTY and the terminal is likely ANSI-capable.
 * Returns 0 otherwise.
*/   int is_tty = isatty(fileno(stdout));
    if (!is_tty) return 0;

    const char *no_color = getenv("NO_COLOR");
    if (no_color != NULL) return 0;

    const char *term = getenv("TERM");
    if (term == NULL) return 0;

    if (strcmp(term, "dumb") == 0 || strcmp(term, "unknown") == 0) return 0;

    // Whitelist common ANSI-capable terminals
    if (strncmp(term, "xterm", 5) == 0) return 1;
    if (strncmp(term, "screen", 6) == 0) return 1;
    if (strncmp(term, "tmux", 4) == 0) return 1;
    if (strncmp(term, "linux", 5) == 0) return 1;
    if (strncmp(term, "vt", 2) == 0) return 1;

    // Fallback: assume non-dumb terminal supports ANSI
    return 1;
}

static int term_supports_alt_screen(void)
{
/**
 * term_supports_alt_screen()
 *
 * Returns 1 if the terminal is likely to honor the alternate screen buffer
 * (\033[?1049h), 0 otherwise.
*/
    const char *term = getenv("TERM");
    if (term == NULL) return 0;

    // Only known terminals support the alternate screen reliably
    if (strncmp(term, "xterm", 5) == 0) return 1;
    if (strncmp(term, "screen", 6) == 0) return 1;
    if (strncmp(term, "tmux", 4) == 0) return 1;
    if (strncmp(term, "rxvt", 4) == 0) return 1;

    // Could add more here (konsole, kitty, etc.)
    return 0;
}
static int get_terminal_width(void)
{
/**
 * get_terminal_width()
 *
 * Retrieves the current terminal width using ioctl().
 *
 * Fallback:
 * - Returns 120 if width cannot be determined
 *
 * Returns:
 *   Terminal width in columns
*/
    struct winsize w;
    if(isatty(fileno(stdout)) && ioctl(fileno(stdout), TIOCGWINSZ, &w) == 0)
        return w.ws_col;
    return 120;
}

static void trunc_field(char *out, size_t width, const char *in)
{
/**
 * trunc_field(out, width, in)
 *
 * Truncates a string to fit within a fixed column width.
 *
 * Behavior:
 * - If string fits → copied as-is
 * - If too long → truncated with "..."
 * - If width <= 3 → hard cut, no ellipsis
 *
 * @out: Output buffer (must be width+1)
 * @width: Maximum display width
 * @in: Input string
*/
    if(width == 0) return;

    size_t len = strlen(in);

    if(len <= width)
    {
        snprintf(out, width + 1, "%s", in);
        return;
    }

    if(width <= 3)
    {
        snprintf(out, width + 1, "%.*s", (int)width, in);
        return;
    }

    snprintf(out, width + 1, "%.*s...", (int)(width - 3), in);
}
static void format_ddhhmm(char *buf, size_t len, long long seconds)
{
    long long mins = seconds / 60;
    long long hrs  = mins / 60;
    long long days = hrs / 24;

    mins = mins % 60;
    hrs  = hrs % 24;

    snprintf(buf, len, "%02lld:%02lld:%02lld", days, hrs, mins);
}

/* ================== WATCH LOOP ================== */

int watch_all_torrents(CURL *curl)
{
/**
 * @brief Live monitoring loop for all torrents.
 *
 * Continuously fetches torrent data via qBittorrent Web API
 * and displays it in a formatted ANSI table. Supports:
 * - Alternate screen for clean refresh
 * - Single snapshot for non-TTY or dumb terminals
 * - Dynamic column width adjustment based on terminal size
 *
 * @param curl Initialized CURL handle used for API requests.
 *
 * @return int Returns 1 on success, 0 on failure.
 *
 * @note ANSI-only;
 * @note Updates happen in a single flush per frame to reduce flicker.
 * @note Scroll-safe on most terminals, including XFCE, Konsole, and tmux.
 * @note Displays: Name, Hash, Size, Progress, ETA/Seedtime,
 *       DL/UL speeds, downloaded/uploaded, Tags, Category, State.
*/
if(!curl) return 0;

    int has_ansi = term_supports_ansi();
    int use_alt = has_ansi && term_supports_alt_screen();

    if(use_alt) {
        printf("\033[?1049h"); // enter alternate screen
        fflush(stdout);
    }

    char buf[32768]; // single buffer per frame

    do
    {
        int pos = 0;

        // clear screen at start of frame
        if(has_ansi) pos += snprintf(buf + pos, sizeof(buf) - pos, "\033[2J\033[H");

        int term_width = get_terminal_width();
        int name_w = 25;
        if(term_width > 140) name_w = 30;
        if(term_width > 160) name_w = 35;
        if(name_w > 40) name_w = 40;
        if(name_w < 15) name_w = 15;
        int hash_w = 6, tags_w = 18, cat_w = 12, state_w = 10;

        char url[512];
        snprintf(url,sizeof(url),"%s/api/v2/torrents/info",creds.qbt_url);

        char *json = qbt_get_json(curl,url);
        if(!json) break;

        cJSON *root = cJSON_Parse(json);
        free(json);
        if(!root || !cJSON_IsArray(root)) {
            if(root) cJSON_Delete(root);
            break;
        }

        int count = cJSON_GetArraySize(root);
        if(count <= 0) {
            cJSON_Delete(root);
            pos += snprintf(buf + pos, sizeof(buf) - pos, "No torrents\n");
            fwrite(buf,1,pos,stdout); fflush(stdout);
            sleep(5);
            continue;
        }

        const char *names[count], *states[count], *tags[count], *categories[count], *hashes[count];
        long long sizes[count], dls[count], uls[count], etas[count], seedtimes[count], downloaded[count], uploaded[count];
        double progs[count];
        long long total_dl=0, total_ul=0;
        int active_torrents=0;

        for(int i=0;i<count;i++)
        {
            cJSON *obj = cJSON_GetArrayItem(root,i);
            cJSON *item;

            names[i] = states[i] = tags[i] = categories[i] = hashes[i] = "";
            sizes[i]=dls[i]=uls[i]=etas[i]=seedtimes[i]=downloaded[i]=uploaded[i]=0;
            progs[i]=0.0;

            item=cJSON_GetObjectItem(obj,"name"); if(cJSON_IsString(item)) names[i]=item->valuestring;
            item=cJSON_GetObjectItem(obj,"state"); if(cJSON_IsString(item)) states[i]=item->valuestring;
            item=cJSON_GetObjectItem(obj,"tags"); if(cJSON_IsString(item)) tags[i]=item->valuestring;
            item=cJSON_GetObjectItem(obj,"category"); if(cJSON_IsString(item)) categories[i]=item->valuestring;
            item=cJSON_GetObjectItem(obj,"hash"); if(cJSON_IsString(item)) hashes[i]=item->valuestring;
            item=cJSON_GetObjectItem(obj,"total_size"); if(cJSON_IsNumber(item)) sizes[i]=(long long)item->valuedouble;
            item=cJSON_GetObjectItem(obj,"progress"); if(cJSON_IsNumber(item)) progs[i]=item->valuedouble;
            item=cJSON_GetObjectItem(obj,"eta"); if(cJSON_IsNumber(item)) etas[i]=(long long)item->valuedouble;
            item=cJSON_GetObjectItem(obj,"dlspeed"); if(cJSON_IsNumber(item)) dls[i]=(long long)item->valuedouble;
            item=cJSON_GetObjectItem(obj,"upspeed"); if(cJSON_IsNumber(item)) uls[i]=(long long)item->valuedouble;
            item=cJSON_GetObjectItem(obj,"downloaded"); if(cJSON_IsNumber(item)) downloaded[i]=(long long)item->valuedouble;
            item=cJSON_GetObjectItem(obj,"uploaded"); if(cJSON_IsNumber(item)) uploaded[i]=(long long)item->valuedouble;

            total_dl += dls[i]; total_ul += uls[i];
            if(strcmp(states[i],"downloading")==0 || strcmp(states[i],"uploading")==0)
                active_torrents++;
        }

        // totals
        char total_dl_buf[32], total_ul_buf[32];
        fmt_bytes(total_dl_buf,sizeof(total_dl_buf),total_dl);
        fmt_bytes(total_ul_buf,sizeof(total_ul_buf),total_ul);

        pos += snprintf(buf+pos,sizeof(buf)-pos,"\n Active torrents: %d | Total DL: %s | Total UL: %s\n\n",
                        active_torrents,total_dl_buf,total_ul_buf);

        // header
        pos += snprintf(buf+pos,sizeof(buf)-pos,
                        "+-------------------------------------+--------+----------+------+----------+----------+----------+----------+----------+--------------------+--------------+-----------+\n");
        pos += snprintf(buf+pos,sizeof(buf)-pos,
                        "| %-*s | %-*s | %8s | %4s | %8s | %8s | %8s | %8s | %8s | %-*s | %-*s | %-*s|\n",
                        name_w,"Name",
                        hash_w,"Hash",
                        "Size","Prog","ETA","DL","UL","Download","Upload",
                        tags_w,"Tags",
                        cat_w,"Category",
                        state_w,"State");
        pos += snprintf(buf+pos,sizeof(buf)-pos,
                        "+-------------------------------------+--------+----------+------+----------+----------+----------+----------+----------+--------------------+--------------+-----------+\n");

        // rows
        for(int i=0;i<count;i++)
        {
            char name_buf[128], hash_buf[16], tags_buf[32], cat_buf[32], state_buf[32];
            char size_buf[32], dl_buf[32], ul_buf[32], downloaded_buf[32], uploaded_buf[32], eta_buf[32], prog_buf[16];

            trunc_field(name_buf, name_w, names[i]);
            trunc_field(tags_buf, tags_w, tags[i]);
            trunc_field(cat_buf, cat_w, categories[i]);
            trunc_field(state_buf, state_w, states[i]);
            memset(hash_buf,0,sizeof(hash_buf));
            strncpy(hash_buf, hashes[i], 6); hash_buf[6]='\0';

            fmt_bytes(size_buf,sizeof(size_buf),sizes[i]);
            fmt_bytes(dl_buf,sizeof(dl_buf),dls[i]);
            fmt_bytes(ul_buf,sizeof(ul_buf),uls[i]);
            fmt_bytes(downloaded_buf,sizeof(downloaded_buf),downloaded[i]);
            fmt_bytes(uploaded_buf,sizeof(uploaded_buf),uploaded[i]);

            snprintf(prog_buf,sizeof(prog_buf),"%.0f%%",progs[i]*100.0);

            if(strcmp(states[i],"downloading")==0) strcpy(state_buf,"download");

            const int ETA_COL_WIDTH = 9;
            if(etas[i]==0 || etas[i]>=8640000) {
                int pad=(ETA_COL_WIDTH-1)/2;
                snprintf(eta_buf,sizeof(eta_buf),"%*s∞%*s",pad,"",ETA_COL_WIDTH-pad-2,"");
            } else {
                format_ddhhmm(eta_buf,sizeof(eta_buf),etas[i]);
            }

            pos += snprintf(buf+pos,sizeof(buf)-pos,
                            "| %-*s | %-*s | %8s | %4s | %8s | %8s | %8s | %8s | %8s | %-*s | %-*s | %-*s|\n",
                            name_w,name_buf,
                            hash_w,hash_buf,
                            size_buf,
                            prog_buf,
                            eta_buf,
                            dl_buf,
                            ul_buf,
                            downloaded_buf,
                            uploaded_buf,
                            tags_w,tags_buf,
                            cat_w,cat_buf,
                            state_w,state_buf);
        }

         if(has_ansi) {
            pos += snprintf(buf+pos,sizeof(buf)-pos,
                            "+-------------------------------------+--------+----------+------+----------+----------+----------+----------+----------+--------------------+--------------+-----------+\n"
                            "|       Press <Ctrl>+c to quit        |\n"
                            "+-------------------------------------+\n");
        } else {
            pos += snprintf(buf+pos,sizeof(buf)-pos,
                            "+-------------------------------------+--------+----------+------+----------+----------+----------+----------+----------+--------------------+--------------+-----------+\n"
                            "| No TTY detected showing single snapshot only |\n"
                            "+----------------------------------------------+\n");
            break;
        }

        cJSON_Delete(root);

        fwrite(buf,1,pos,stdout);
        fflush(stdout);

        sleep(3);

    } while(1);

    if(use_alt) {
        printf("\033[?1049l");
        fflush(stdout);
    }

    return 1;
}
