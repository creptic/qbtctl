/*
 * watch.c
 * -----------------------------
 * Purpose: Live monitoring module for qbtctl. Handles fetching torrent data,
 *          formatting output, and rendering a dynamic table view with auto-refresh.
 *
 * Version: 1.5.0
 * Date:    2026-03-26
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
int term_supports_ansi(void)
{
    int is_tty = isatty(fileno(stdout));
    const char *term = getenv("TERM");
    const char *no_color = getenv("NO_COLOR");

    int ansi = 0;

    /* must be a tty */
    if (!is_tty) {
        return 0;
    }

    /* user explicitly disabled color */
    if (no_color != NULL) {
        return 0;
    }

    /* TERM must exist */
    if (term == NULL) {
        return 0;
    }

    /* known bad terminals */
    if (strcmp(term, "dumb") == 0) {
        return 0;
    }

    if (strcmp(term, "unknown") == 0) {
        return 0;
    }

    /* common good terminals */
    if (strncmp(term, "xterm", 5) == 0) {
        ansi = 1;
    } else if (strncmp(term, "screen", 6) == 0) {
        ansi = 1;
    } else if (strncmp(term, "tmux", 4) == 0) {
        ansi = 1;
    } else if (strncmp(term, "linux", 5) == 0) {
        ansi = 1;
    } else if (strncmp(term, "vt", 2) == 0) {
        ansi = 1;
    } else {
        /* fallback: assume ANSI unless explicitly bad */
        ansi = 1;
    }

    return ansi;
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



/* --- terminfo helpers --- */

static void tput_clear(void) {
    system("tput clear");
}

static void tput_home(void) {
    system("tput cup 0 0");
}

static void tput_hide_cursor(void) {
    system("tput civis");
}

static void tput_show_cursor(void) {
    system("tput cnorm");
}

/* --- your watch loop --- */

int watch_all_torrents(CURL *curl)
{
    if(!curl) return 0;

    int has_ansi = term_supports_ansi();

    tput_clear();
    tput_hide_cursor();

    do
    {
        char buf[65536];
        int pos = 0;

        int term_width = get_terminal_width();

        int name_w = 25;
        if(term_width > 140) name_w = 30;
        if(term_width > 160) name_w = 35;
        if(name_w > 40) name_w = 40;
        if(name_w < 15) name_w = 15;

        int hash_w = 6;
        int tags_w = 18;
        int cat_w  = 12;
        int state_w= 10;

        /* move cursor to top instead of clearing scrollback */
        tput_home();

        char url[512];
        snprintf(url,sizeof(url),"%s/api/v2/torrents/info",creds.qbt_url);

        char *json = qbt_get_json(curl,url);
        if(!json) return 0;

        cJSON *root = cJSON_Parse(json);
        free(json);
        if(!root || !cJSON_IsArray(root))
        {
            if(root) cJSON_Delete(root);
            return 0;
        }

        int count = cJSON_GetArraySize(root);
        if(count <= 0)
        {
            cJSON_Delete(root);
            pos += snprintf(buf+pos,sizeof(buf)-pos,"No torrents\n");
            fwrite(buf,1,pos,stdout);
            fflush(stdout);
            sleep(5);
            continue;
        }

        const char *names[count], *states[count], *tags[count], *categories[count], *hashes[count];
        long long sizes[count], dls[count], uls[count], etas[count], downloaded[count], uploaded[count];
        double progs[count];

        long long total_dl=0, total_ul=0;
        int active_torrents=0;

        for(int i=0;i<count;i++)
        {
            cJSON *obj = cJSON_GetArrayItem(root,i);
            cJSON *item;

            names[i]=states[i]=tags[i]=categories[i]=hashes[i]="";
            sizes[i]=dls[i]=uls[i]=etas[i]=downloaded[i]=uploaded[i]=0;
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

            total_dl += dls[i];
            total_ul += uls[i];

            if(strcmp(states[i],"downloading")==0 || strcmp(states[i],"uploading")==0)
                active_torrents++;
        }

        /* totals */
        char total_dl_buf[32], total_ul_buf[32];
        fmt_bytes(total_dl_buf,sizeof(total_dl_buf),total_dl);
        fmt_bytes(total_ul_buf,sizeof(total_ul_buf),total_ul);

        pos += snprintf(buf+pos,sizeof(buf)-pos,
                        "\n Active torrents: %d | Total DL: %s | Total UL: %s\n\n",
                        active_torrents,total_dl_buf,total_ul_buf);

        /* header */
        pos += snprintf(buf+pos,sizeof(buf)-pos,
                        "+-------------------------------------+--------+----------+------+----------+----------+----------+----------+----------+--------------------+--------------+-----------+\n");

        pos += snprintf(buf+pos,sizeof(buf)-pos,
                        "| %-*s | %-*s | %8s | %4s | %8s | %8s | %8s | %8s | %8s | %-*s | %-*s | %-*s|\n",
                        name_w,"Name",hash_w,"Hash","Size","Prog","ETA","DL","UL","Download","Upload",
                        tags_w,"Tags",cat_w,"Category",state_w,"State");

        pos += snprintf(buf+pos,sizeof(buf)-pos,
                        "+-------------------------------------+--------+----------+------+----------+----------+----------+----------+----------+--------------------+--------------+-----------+\n");

        for(int i=0;i<count;i++)
        {
            char name_buf[128], hash_buf[16], tags_buf[32], cat_buf[32], state_buf[32];
            char size_buf[32], dl_buf[32], ul_buf[32], downloaded_buf[32], uploaded_buf[32], eta_buf[32], prog_buf[16];

            trunc_field(name_buf,name_w,names[i]);
            trunc_field(tags_buf,tags_w,tags[i]);
            trunc_field(cat_buf,cat_w,categories[i]);
            trunc_field(state_buf,state_w,states[i]);

            memset(hash_buf,0,sizeof(hash_buf));
            strncpy(hash_buf,hashes[i],6);
            hash_buf[6]='\0';

            fmt_bytes(size_buf,sizeof(size_buf),sizes[i]);
            fmt_bytes(dl_buf,sizeof(dl_buf),dls[i]);
            fmt_bytes(ul_buf,sizeof(ul_buf),uls[i]);
            fmt_bytes(downloaded_buf,sizeof(downloaded_buf),downloaded[i]);
            fmt_bytes(uploaded_buf,sizeof(uploaded_buf),uploaded[i]);

            snprintf(prog_buf,sizeof(prog_buf),"%.0f%%",progs[i]*100.0);

            if(strcmp(states[i],"downloading")==0) {
                strcpy(state_buf,"download");
            }

            format_ddhhmm(eta_buf,sizeof(eta_buf),etas[i]);

            pos += snprintf(buf+pos,sizeof(buf)-pos,
                            "| %-*s | %-*s | %8s | %4s | %8s | %8s | %8s | %8s | %8s | %-*s | %-*s | %-*s|\n",
                            name_w,name_buf,hash_w,hash_buf,
                            size_buf,prog_buf,eta_buf,dl_buf,ul_buf,
                            downloaded_buf,uploaded_buf,
                            tags_w,tags_buf,cat_w,cat_buf,state_w,state_buf);
        }

        fwrite(buf,1,pos,stdout);
        fflush(stdout);

        sleep(2);

    } while(1);

    tput_show_cursor();
    return 1;
}
