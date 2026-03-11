/*
 * qbtctl - qBittorrent CLI by Creptic 2026
 * Ultra-fast streaming parser
 */
#include "help.h"
#include "auth.h"
#include "cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include <curl/curl.h>
#include <ctype.h>

void show_help(void);

#define MAX_JSON 1048576
#define HASH_WIDTH 40

/* ================= EXIT CODES ================= */

#define EXIT_OK             0
#define EXIT_LOGIN_FAIL     1
#define EXIT_FETCH_FAIL     2
#define EXIT_SET_FAIL       3
#define EXIT_BAD_ARGS       4
#define EXIT_FILE           5
#define EXIT_ACTION_FAIL    6


#define ERR(fmt, ...) fprintf(stderr, "[ERROR] " fmt "\n", ##__VA_ARGS__)

int show_all_clean = 0;
int show_json = 0;
int show_all_json = 0;
int raw = 0;
bool if_watch = false;

/* ================= GLOBAL TORRENT ================= */
struct {
    char name[128];
    char hash[64];
    char tags[64];
    char category[64];
    int up_limit;
    int dl_limit;
    char dl_path[512];
    float ratio_limit;
    int seedtime;
    int seedtime_limit;
    bool superseed;
    bool seq_dl;
    bool auto_tmm;
    char tracker[512];
    bool is_private;
    char state[64];
    double progress;     // 0.0 - 100.0
    long long size;      // bytes
    long long downloaded;// bytes
    long long uploaded;  // bytes
    long long dlspeed;         // bytes/sec
    long long upspeed;         // bytes/sec
    float ratio;
    long long eta;       // seconds remaining
} torrent;

/* ================= CURL MEMORY ================= */
struct memory {
    char *data;
    size_t size;
};

/* ================= SAFE COPY ================= */
static void safe_copy(char *dst, size_t dsz, const char *src)
{
    if (!dst) return;
    if (!src) { dst[0] = 0; return; }

    size_t len = strlen(src);
    if (len >= dsz) len = dsz - 1;
    memcpy(dst, src, len);
    dst[len] = 0;
}

/* ================= CURL MEMORY ================= */
static size_t write_cb(void *ptr, size_t size, size_t nmemb, void *userdata)
{
    struct memory *mem = (struct memory *)userdata;
    if (!mem || !mem->data) return 0;

    if (size != 0 && nmemb > SIZE_MAX / size) return 0;

    size_t incoming = size * nmemb;
    if (mem->size >= MAX_JSON - 1) return 0;

    size_t remaining = (MAX_JSON - 1) - mem->size;
    if (incoming > remaining) incoming = remaining;

    memcpy(mem->data + mem->size, ptr, incoming);
    mem->size += incoming;
    mem->data[mem->size] = '\0';

    return incoming;
}

static size_t discard_cb(void *ptr, size_t size, size_t nmemb, void *userdata)
{
    (void)ptr;
    (void)userdata;

    return size * nmemb;
}

/* ================= LOGIN ================= */
static int login_qbt(CURL *curl)
{
    if (!curl) return EXIT_LOGIN_FAIL;

    char url[512];
    char post[256];

    snprintf(url, sizeof(url), "%s/api/v2/auth/login", creds.qbt_url);
    snprintf(post, sizeof(post), "username=%s&password=%s", creds.qbt_user, creds.qbt_pass);

    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_POST, 1L);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, post);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, discard_cb);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 15L);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 5L);
    curl_easy_setopt(curl, CURLOPT_COOKIEFILE, "");
    curl_easy_setopt(curl, CURLOPT_COOKIEJAR, "");

    CURLcode res = curl_easy_perform(curl);
    if (res != CURLE_OK) {
        ERR("CURL error on login: %s", curl_easy_strerror(res));
        return EXIT_LOGIN_FAIL;
    }

    long status = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status);
    if (status != 200) {
        ERR("Login failed, HTTP status: %ld", status);
        return EXIT_LOGIN_FAIL;
    }

    curl_easy_setopt(curl, CURLOPT_POST, 0L);
    curl_easy_setopt(curl, CURLOPT_HTTPGET, 1L);

    return EXIT_OK;
}
/* ================= GET JSON ================= */
char *qbt_get_json(CURL *curl, const char *url)
{
    if (!curl || !url)
    {
        ERR("qbt_get_json: NULL curl or url");
        return NULL;
    }

    struct memory mem;
    mem.data = malloc(MAX_JSON);
    if (!mem.data)
    {
        ERR("qbt_get_json: Memory allocation failed");
        return NULL;
    }

    mem.size = 0;
    mem.data[0] = 0;

    curl_easy_setopt(curl, CURLOPT_HTTPGET, 1L);
    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_cb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &mem);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 15L);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 5L);

    CURLcode res = curl_easy_perform(curl);
    if (res != CURLE_OK)
    {
        ERR("qbt_get_json: curl_easy_perform failed: %s", curl_easy_strerror(res));
        free(mem.data);
        return NULL;
    }

    long status = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status);
    if (status != 200)
    {
        ERR("qbt_get_json: HTTP status %ld for URL: %s", status, url);
        free(mem.data);
        return NULL;
    }

    return mem.data; /* caller must free */
}

/* ================= RESOLVE && VALIDATE SHORT HASH ================= */
bool validate_hash(const char *hash)
{
    if (!hash) return false;

    size_t len = strlen(hash);
    if (len != HASH_WIDTH) return false;

    for (size_t i = 0; i < HASH_WIDTH; i++) {
        char c = hash[i];
        if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F')))
            return false;
    }
    return true;
}

/* ================= RESOLVE SHORT HASH ================= */
int resolve_short_hash(CURL *curl)
{
    if (curl == NULL || creds.qbt_hash[0] == '\0') return 0;

    size_t input_len = strlen(creds.qbt_hash);
    if (input_len == HASH_WIDTH) return 1;
    if (input_len > HASH_WIDTH) return 0;

    char url[512];
    snprintf(url, sizeof(url), "%s/api/v2/torrents/info", creds.qbt_url);

    char *json = qbt_get_json(curl, url);
    if (!json) return 0;

    cJSON *root = cJSON_Parse(json);
    free(json);
    if (!root || !cJSON_IsArray(root))
    {
        if (root) cJSON_Delete(root);
        ERR("resolve_short_hash: invalid JSON response");
        return 0;
    }

    int matches = 0;
    char resolved_hash[HASH_WIDTH + 1] = {0};
    int count = cJSON_GetArraySize(root);

    for (int i = 0; i < count; i++)
    {
        cJSON *obj = cJSON_GetArrayItem(root, i);
        if (!cJSON_IsObject(obj)) continue;

        cJSON *hash_item = cJSON_GetObjectItem(obj, "hash");
        if (!cJSON_IsString(hash_item)) continue;

        const char *full_hash = hash_item->valuestring;
        if (strlen(full_hash) < input_len) continue;

        if (strncmp(full_hash, creds.qbt_hash, input_len) == 0)
        {
            matches++;
            if (matches == 1) safe_copy(resolved_hash, sizeof(resolved_hash), full_hash);
        }
    }

    cJSON_Delete(root);

    if (matches == 0)
    {
        ERR("No torrent matches short hash: %s", creds.qbt_hash);
        return 0;
    }

    if (matches > 1)
    {
        ERR("Short hash is ambiguous: %s", creds.qbt_hash);
        return 0;
    }

    safe_copy(creds.qbt_hash, sizeof(creds.qbt_hash), resolved_hash);
    return 1;
}

static bool resolve_and_validate_hash(CURL *curl, const char *hash)
{

    if (!curl || !hash) return false;

    safe_copy(creds.qbt_hash, sizeof(creds.qbt_hash), hash);

    if (!resolve_short_hash(curl)) {
        ERR("Failed to resolve short hash '%s'", hash);
        exit(EXIT_BAD_ARGS);
    }

    if (!validate_hash(creds.qbt_hash)) {
        ERR("Resolved hash is invalid: '%s'", creds.qbt_hash);
        exit(EXIT_BAD_ARGS);
    }

    return true;
}

/* ================= POPULATE TORRENT STRUCT ================= */
int populate_torrent_info_struct(CURL *curl)
{
    if (!curl)
    {
        ERR("populate_torrent_info_struct: NULL curl");
        return 0;
    }

    char url[512];
    snprintf(url, sizeof(url), "%s/api/v2/torrents/info?hashes=%s", creds.qbt_url, creds.qbt_hash);

    char *json = qbt_get_json(curl, url);
    if (!json) return 0;

    /* ===== Show raw JSON if requested ===== */
    if (show_json == 1)
    {
        printf("%s\n", json);
        free(json);
        return 1;
    }

    cJSON *root = cJSON_Parse(json);
    free(json);
    if (!root)
    {
        ERR("populate_torrent_info_struct: failed to parse JSON");
        return 0;
    }

    if (!cJSON_IsArray(root))
    {
        ERR("populate_torrent_info_struct: JSON is not an array");
        cJSON_Delete(root);
        return 0;
    }

    cJSON *obj = cJSON_GetArrayItem(root, 0);
    if (!obj)
    {
        ERR("populate_torrent_info_struct: no torrent object found");
        cJSON_Delete(root);
        return 0;
    }

    /* ===== Populate global torrent struct ===== */
    memset(&torrent, 0, sizeof(torrent));
    cJSON *item;

    item = cJSON_GetObjectItem(obj, "name");
    if (cJSON_IsString(item)) safe_copy(torrent.name, sizeof(torrent.name), item->valuestring);

    item = cJSON_GetObjectItem(obj, "hash");
    if (cJSON_IsString(item)) safe_copy(torrent.hash, sizeof(torrent.hash), item->valuestring);

    item = cJSON_GetObjectItem(obj, "tags");
    if (cJSON_IsString(item)) safe_copy(torrent.tags, sizeof(torrent.tags), item->valuestring);

    item = cJSON_GetObjectItem(obj, "category");
    if (cJSON_IsString(item)) safe_copy(torrent.category, sizeof(torrent.category), item->valuestring);

    item = cJSON_GetObjectItem(obj, "content_path");
    if (cJSON_IsString(item)) safe_copy(torrent.dl_path, sizeof(torrent.dl_path), item->valuestring);

    item = cJSON_GetObjectItem(obj, "tracker");
    if (cJSON_IsString(item)) safe_copy(torrent.tracker, sizeof(torrent.tracker), item->valuestring);

    item = cJSON_GetObjectItem(obj, "up_limit");
    if (cJSON_IsNumber(item)) torrent.up_limit = item->valueint;

    item = cJSON_GetObjectItem(obj, "dl_limit");
    if (cJSON_IsNumber(item)) torrent.dl_limit = item->valueint;

    item = cJSON_GetObjectItem(obj, "ratio_limit");
    if (cJSON_IsNumber(item)) torrent.ratio_limit = (float)item->valuedouble;

    item = cJSON_GetObjectItem(obj, "seeding_time");
    if (cJSON_IsNumber(item)) torrent.seedtime = item->valueint;

    item = cJSON_GetObjectItem(obj, "seeding_time_limit");
    if (cJSON_IsNumber(item)) torrent.seedtime_limit = item->valueint;

    item = cJSON_GetObjectItem(obj, "seq_dl");
    if (cJSON_IsBool(item)) torrent.seq_dl = cJSON_IsTrue(item);

    item = cJSON_GetObjectItem(obj, "auto_tmm");
    if (cJSON_IsBool(item)) torrent.auto_tmm = cJSON_IsTrue(item);

    item = cJSON_GetObjectItem(obj, "super_seeding");
    if (cJSON_IsBool(item)) torrent.superseed = cJSON_IsTrue(item);

    item = cJSON_GetObjectItem(obj, "private");
    if (cJSON_IsBool(item)) torrent.is_private = cJSON_IsTrue(item);

    item = cJSON_GetObjectItem(obj, "state");
    if (cJSON_IsString(item)) safe_copy(torrent.state, sizeof(torrent.state), item->valuestring);

    item = cJSON_GetObjectItem(obj, "ratio");
    if (cJSON_IsNumber(item)) torrent.ratio = (float)item->valuedouble;

    /* ----- numeric fields ----- */
    item = cJSON_GetObjectItem(obj, "upspeed");
    if(item && cJSON_IsNumber(item))
        torrent.upspeed = (long long)item->valuedouble;

    item = cJSON_GetObjectItem(obj, "dlspeed");
    if(item && cJSON_IsNumber(item))
        torrent.dlspeed = (long long)item->valuedouble;

    item = cJSON_GetObjectItem(obj, "uploaded");
    if(item && cJSON_IsNumber(item))
        torrent.uploaded = (long long)item->valuedouble;

    item = cJSON_GetObjectItem(obj, "downloaded");
    if(item && cJSON_IsNumber(item))
        torrent.downloaded = (long long)item->valuedouble;

    item = cJSON_GetObjectItem(obj, "size");
    if(item && cJSON_IsNumber(item))
        torrent.size = (long long)item->valuedouble;

    item = cJSON_GetObjectItem(obj, "eta");
    if(item && cJSON_IsNumber(item))
        torrent.eta = (long long)item->valuedouble;

    /* ----- progress ----- */
    item = cJSON_GetObjectItem(obj, "progress");
    if(item && cJSON_IsNumber(item))
        torrent.progress = item->valuedouble;

    cJSON_Delete(root);
    return 1;
}

/* ================= ENSURE SINGLE LOADED ================= */
static int ensure_single_loaded(CURL *curl, int *single_loaded)
{
    if (!*single_loaded) {
        if (!populate_torrent_info_struct(curl)) {
            ERR("Failed to populate torrent info in ensure_single_loaded");
            exit (EXIT_FETCH_FAIL);
        }
        *single_loaded = 1;
    }
    return 1;
}

/* ================= GETTERS ================= */
static char *format_bool(bool value) {
    static char buf[8];
    if (raw == 0) {
        if (value) {
            snprintf(buf, sizeof(buf), "1");
        } else {
            snprintf(buf, sizeof(buf), "0");
        }
    } else {
        if (value) {
            safe_copy(buf, sizeof(buf), "true");
        } else {
            safe_copy(buf, sizeof(buf), "false");
        }
    }
    return buf;
}

static char *fmt_bytes(long long val)
{
    static char buf[64];

    if(raw)
    {
        snprintf(buf, sizeof(buf), "%lld", val);
        return buf;
    }

    if(val >= 1073741824)
    {
        double v = (double)val / 1073741824.0;
        snprintf(buf, sizeof(buf), "%.2fG", v);
    }
    else if(val >= 1048576)
    {
        double v = (double)val / 1048576.0;
        snprintf(buf, sizeof(buf), "%.2fM", v);
    }
    else if(val >= 1024)
    {
        double v = (double)val / 1024.0;
        snprintf(buf, sizeof(buf), "%.2fK", v);
    }
    else
    {
        snprintf(buf, sizeof(buf), "%lld", val);
    }

    return buf;
}
char *get_private()  { return format_bool(torrent.is_private); }
char *get_superseed() { return format_bool(torrent.superseed); }
char *get_seq_dl()    { return format_bool(torrent.seq_dl); }
char *get_auto_tmm()  { return format_bool(torrent.auto_tmm); }

char *get_name()     { return torrent.name; }
char *get_hash()     { return torrent.hash; }
char *get_tags()     { return torrent.tags; }
char *get_category() { return torrent.category; }
char *get_dl_path()  { return torrent.dl_path; }

char *get_state()  { return torrent.state; }
char *get_tracker(void)
{
    static char formatted[512];

    if (!torrent.tracker[0])
        return "";

    if (raw==1)
        return torrent.tracker;

    const char *tracker = torrent.tracker;

    const char *scheme = strstr(tracker, "://");
    if (!scheme)
        return torrent.tracker;

    const char *host_start = scheme + 3;
    const char *host_end = host_start;

    while (*host_end)
    {
        if (*host_end == ':' ||
            *host_end == '/' ||
            *host_end == '?')
            break;
        host_end++;
    }

    size_t scheme_len = (scheme - tracker) + 3;
    size_t host_len = host_end - host_start;

    if (scheme_len + host_len >= sizeof(formatted))
        return torrent.tracker;

    memcpy(formatted, tracker, scheme_len);

    for (size_t i = 0; i < host_len; i++)
    {
        formatted[scheme_len + i] =
            (char)tolower((unsigned char)host_start[i]);
    }

    formatted[scheme_len + host_len] = '\0';

    return formatted;
}
char *get_uplimit() {
    static char buf[32];
    if(raw == 1) {
        snprintf(buf, sizeof(buf), "%d", torrent.up_limit);
    } else {
        return fmt_bytes(torrent.up_limit);
    }
    return buf;
}

char *get_downlimit() {
    static char buf[32];
    if(raw == 1) {
        snprintf(buf, sizeof(buf), "%d", torrent.dl_limit);
    } else {
        return fmt_bytes(torrent.up_limit);
    }
    return buf;
}

char *get_ratio_limit() {
    static char buf[32];
    if(raw == 1) {
        snprintf(buf, sizeof(buf), "%f", torrent.ratio_limit);
    } else {
        snprintf(buf, sizeof(buf), "%.2f", torrent.ratio_limit);
    }
    return buf;
}

char *get_seedtime() {
    static char buf[32];
    if(raw == 1) {
        snprintf(buf, sizeof(buf), "%d", torrent.seedtime);
    } else {
        int d = torrent.seedtime / 86400;
        int h = (torrent.seedtime % 86400) / 3600;
        int m = (torrent.seedtime % 3600) / 60;
        snprintf(buf, sizeof(buf), "%02d:%02d:%02d", d, h, m);
    }
    return buf;
}

char *get_seedtime_limit() {
    static char buf[32];
    if(raw == 1) {
        snprintf(buf, sizeof(buf), "%d", torrent.seedtime_limit);
    } else {
        int d = torrent.seedtime_limit / 86400;
        int h = (torrent.seedtime_limit % 86400) / 3600;
        int m = (torrent.seedtime_limit % 3600) / 60;
        snprintf(buf, sizeof(buf), "%02d:%02d:%02d", d, h, m);
    }
    return buf;
}


static char *get_upspeed(void)
{

    static char buf[64];

    if(raw==0)
    {
        return fmt_bytes(torrent.upspeed);

    }
    else
    {
        snprintf(buf, sizeof(buf), "%lld", torrent.upspeed);
    }

    return buf;
    return buf;
}


static char *get_dlspeed(void)
{
    static char buf[64];

    if(raw==0)
    {
        return fmt_bytes(torrent.dlspeed);;

    }
    else
    {
        snprintf(buf, sizeof(buf), "%lld", torrent.dlspeed);
    }

    return buf;
}



static char *get_uploaded(void)
{
    static char buf[64];

    if(raw==0)
    {
        return fmt_bytes(torrent.uploaded);;

    }
    else
    {
        snprintf(buf, sizeof(buf), "%lld", torrent.uploaded);
    }

    return buf;
}

static char *get_downloaded(void)
{
    static char buf[64];

    if(raw==0)
    {
        return fmt_bytes(torrent.downloaded);;

    }
    else
    {
        snprintf(buf, sizeof(buf), "%lld", torrent.downloaded);
    }

    return buf;
}

static char *get_size(void)
{
    static char buf[64];

    if(raw==0)
    {
        return fmt_bytes(torrent.size);;

    }
    else
    {
        snprintf(buf, sizeof(buf), "%lld", torrent.size);
    }

    return buf;
}

static char *get_eta(void)
{
    static char buf[64];

    if(raw==0)
    {
        long long seconds = torrent.eta;
        long long days = seconds / 86400;
        long long hours = (seconds % 86400) / 3600;
        long long minutes = (seconds % 3600) / 60;

        if(if_watch)
            snprintf(buf, sizeof(buf), "%02lldD:%02lldH:%02lldM", days, hours, minutes);
        else
            snprintf(buf, sizeof(buf), "%02lld:%02lld:%02lld", days, hours, minutes);
    }
    else
    {
        snprintf(buf, sizeof(buf), "%lld", torrent.eta);
    }

    return buf;
}

char *get_ratio() {
    static char buf[32];
    if(raw == 1) {
        snprintf(buf, sizeof(buf), "%f", torrent.ratio);
    } else {
        snprintf(buf, sizeof(buf), "%.2f", torrent.ratio);
    }
    return buf;
}

static char *get_progress(void)
{
    static char buf[64];

    if(raw==0)
    {
        int pct = (int)(torrent.progress * 100.0);
        if(pct > 100) pct = 100;
        if(pct < 0) pct = 0;
        snprintf(buf, sizeof(buf), "%d%%", pct);
    }
    else
    {
        snprintf(buf, sizeof(buf), "%.6f", torrent.progress);
    }

    return buf;
}
/* ================= GET TRACKER LIST ================= */
int get_tracker_list(CURL *curl)
{
    if (!curl) {
        ERR("CURL handle is NULL");
        exit(EXIT_FETCH_FAIL);
    }

    if (!creds.qbt_hash[0]) {
        ERR("No torrent hash loaded");
        exit(EXIT_BAD_ARGS);
    }

    char *escaped_hash = curl_easy_escape(curl, creds.qbt_hash, 0);
    if (!escaped_hash) {
        ERR("Failed to escape hash");
        exit(EXIT_BAD_ARGS);
    }

    char url[512];
    int written = snprintf(url, sizeof(url),
                           "%s/api/v2/torrents/trackers?hash=%s",
                           creds.qbt_url,
                           escaped_hash);

    curl_free(escaped_hash);

    if (written < 0 || written >= (int)sizeof(url)) {
        ERR("URL buffer overflow");
        exit(EXIT_FETCH_FAIL);
    }

    char *json = qbt_get_json(curl, url);
    if (!json) {
        ERR("Failed to fetch tracker list");
        exit(EXIT_FETCH_FAIL);
    }

    cJSON *root = cJSON_Parse(json);
    free(json);

    if (!root || !cJSON_IsArray(root)) {
        if (root) cJSON_Delete(root);
        ERR("Invalid JSON from tracker endpoint");
        exit(EXIT_FETCH_FAIL);
    }

    /* Used only when raw_mode == 0 */
    char seen_hosts[128][256];
    int seen_count = 0;

    int count = cJSON_GetArraySize(root);
    int printed = 0;

    for (int i = 0; i < count; i++) {

        cJSON *obj = cJSON_GetArrayItem(root, i);
        if (!cJSON_IsObject(obj))
            continue;

        cJSON *url_item = cJSON_GetObjectItem(obj, "url");
        if (!cJSON_IsString(url_item) || !url_item->valuestring)
            continue;

        const char *tracker = url_item->valuestring;

        /* Skip internal trackers */
        if (strcmp(tracker, "** [DHT] **") == 0)
            continue;
        if (strcmp(tracker, "** [PeX] **") == 0)
            continue;
        if (strcmp(tracker, "** [LSD] **") == 0)
            continue;

        /* ---------------- RAW MODE ---------------- */
        if (raw==1) {
            if (printed > 0)
                printf(",");
            printf("%s", tracker);
            printed++;
            continue;
        }

        /* -------- NORMALIZED MODE (Deduplicate) -------- */

        const char *scheme = strstr(tracker, "://");
        if (!scheme)
            continue;

        const char *host_start = scheme + 3;
        const char *host_end = host_start;

        while (*host_end &&
            *host_end != ':' &&
            *host_end != '/' &&
            *host_end != '?')
            host_end++;

        size_t host_len = host_end - host_start;
        if (host_len == 0 || host_len >= 256)
            continue;

        char host[256];
        memcpy(host, host_start, host_len);
        host[host_len] = '\0';

        /* Deduplicate by hostname */
        int duplicate = 0;

        for (int j = 0; j < seen_count; j++) {
            if (strcasecmp(seen_hosts[j], host) == 0) {
                duplicate = 1;
                break;
            }
        }

        if (duplicate)
            continue;

        if (seen_count >= 128)
            continue;

        safe_copy(seen_hosts[seen_count], sizeof(seen_hosts[seen_count]), host);
        seen_count++;

        /* Build scheme://host */
        char output[512];
        size_t scheme_len = (scheme - tracker) + 3;
        size_t total_len = scheme_len + host_len;

        if (total_len >= sizeof(output))
            continue;

        memcpy(output, tracker, scheme_len);
        memcpy(output + scheme_len, host, host_len);
        output[total_len] = '\0';

        if (printed > 0)
            printf(",");
        printf("%s", output);
        printed++;
    }

    if (printed > 0)
        printf("\n");

    cJSON_Delete(root);
    return printed;
}

/* ================= SHOW ALL TORRENTS INFO AS JSON ================= */
int show_all_torrents_info_json(CURL *curl)
{
    char url[512];
    snprintf(url, sizeof(url), "%s/api/v2/torrents/info", creds.qbt_url);

    char *json = qbt_get_json(curl, url);
    if (!json) return EXIT_FETCH_FAIL;

    printf("%s\n", json);
    free(json);

    return EXIT_OK;
}

/* ================= SHOW SINGLE TORRENT INFO ================= */
void show_single_torrent_info()
{
    printf("+------------------------------------------+\n");
    printf("Name: %s\n", get_name());
    printf("Hash: %s\n", get_hash());
    printf("Tags: %s\n", get_tags());
    printf("Category: %s\n", get_category());
    printf("Upload Limit: %s\n", get_uplimit());
    printf("Download Limit: %s\n", get_downlimit());
    printf("Full Path: %s\n", get_dl_path());
    printf("Ratio Limit: %s\n", get_ratio_limit());
    printf("Seedtime: %s\n", get_seedtime());
    printf("Seedtime Limit: %s\n", get_seedtime_limit());
    printf("Sequential Download: %s\n", get_seq_dl());
    printf("Auto TMM: %s\n", get_auto_tmm());
    printf("Superseed: %s\n", get_superseed());
    printf("Tracker: %s\n", get_tracker());
    printf("Private: %s\n", get_private());
    printf("Ratio: %s\n",get_ratio());
    printf("Upload Speed: %s\n",get_upspeed());
    printf("Download Speed: %s\n",get_dlspeed());
    printf("Size: %s\n",get_size());
    printf("Uploaded: %s\n",get_uploaded());
    printf("Downloaded: %s\n",get_downloaded());
    printf("ETA: %s\n",get_eta());
    printf("State: %s\n",get_state());
    printf("Progress: %s\n",get_progress());
    printf("+------------------------------------------+\n");
}

/* ================= SHOW ALL TORRENTS INFO ================= */
int show_all_torrents_info(CURL *curl)
{
    char url[512];
    snprintf(url, sizeof(url), "%s/api/v2/torrents/info", creds.qbt_url);

    char *json = qbt_get_json(curl, url);
    if (!json) return EXIT_FETCH_FAIL;

    cJSON *root = cJSON_Parse(json);
    free(json);
    if (!root || !cJSON_IsArray(root))
    {
        if (root) cJSON_Delete(root);
        ERR("show_all_torrents_info: invalid JSON response");
        return EXIT_FETCH_FAIL;
    }

    int count = cJSON_GetArraySize(root);

    if (show_all_clean == 0)
    {
        printf("+------------------------------------------+--------+--------------+--------------+--------------+--------------+--------------+----------+------------------+\n");
        if (raw)
        {
            printf("| %-40s | %-6s | %-12s | %-12s | %-12s | %-12s | %-12s | %-8s | %-16s |\n",
                   "Name", "Hash", "UL Limit (b)", "DL Limit (b)", "State",
                   "Seed Time", "Limit(secs)", "Category", "Tags");
        }
        else
        {
            printf("| %-40s | %-6s | %-12s | %-12s | %-12s | %-12s | %-12s | %-8s | %-16s |\n",
                   "Name", "Hash", "UL Limit(Kb)", "DL Limit(Kb)", "State",
                   "Seed Time", "Limit D:H:M", "Category", "Tags");
        }
        printf("+------------------------------------------+--------+--------------+--------------+--------------+--------------+--------------+----------+------------------+\n");
    }
    else
    {
        if (raw)
        {
            printf("%-40s %-6s %-12s %-12s %-12s %-12s %-12s %-8s %-16s\n",
                   "Name", "Hash", "UL Limit (b)", "DL Limit (b)", "State",
                   "Seed Time", "Limit(secs)", "Category", "Tags");
        }
        else
        {
            printf("%-40s %-6s %-12s %-12s %-12s %-12s %-12s %-8s %-16s\n",
                   "Name", "Hash", "UL Limit(Kb)", "DL Limit(Kb)", "State",
                   "Seed Time", "Limit D:H:M", "Category", "Tags");
        }
    }

    for (int i = 0; i < count; i++)
    {
        cJSON *item = cJSON_GetArrayItem(root, i);
        if (!cJSON_IsObject(item)) continue;

        char name[128] = "";
        char hash[64] = "";
        char tags[64] = "";
        char category[64] = "";
        int up_limit = 0;
        int dl_limit = 0;
        char state[64] = "";
        int seedtime = 0;
        int seedtime_limit = 0;

        cJSON *obj;

        obj = cJSON_GetObjectItem(item, "name");
        if (cJSON_IsString(obj)) safe_copy(name, sizeof(name), obj->valuestring);

        obj = cJSON_GetObjectItem(item, "hash");
        if (cJSON_IsString(obj)) safe_copy(hash, sizeof(hash), obj->valuestring);

        obj = cJSON_GetObjectItem(item, "tags");
        if (cJSON_IsString(obj)) safe_copy(tags, sizeof(tags), obj->valuestring);

        obj = cJSON_GetObjectItem(item, "category");
        if (cJSON_IsString(obj)) safe_copy(category, sizeof(category), obj->valuestring);

        obj = cJSON_GetObjectItem(item, "up_limit");
        if (cJSON_IsNumber(obj)) up_limit = obj->valueint;

        obj = cJSON_GetObjectItem(item, "dl_limit");
        if (cJSON_IsNumber(obj)) dl_limit = obj->valueint;

        obj = cJSON_GetObjectItem(item, "state");
        if (cJSON_IsString(obj)) safe_copy(state, sizeof(state), obj->valuestring);

        obj = cJSON_GetObjectItem(item, "seeding_time");
        if (cJSON_IsNumber(obj)) seedtime = obj->valueint;

        obj = cJSON_GetObjectItem(item, "seeding_time_limit");
        if (cJSON_IsNumber(obj)) seedtime_limit = obj->valueint;

        // Safe 6-character hash prefix
        char hash6[7] = "";
        size_t copy_len = 6;
        if (copy_len >= sizeof(hash6)) copy_len = sizeof(hash6) - 1;
        memcpy(hash6, hash, copy_len);
        hash6[copy_len] = '\0';

        char up_buf[32], down_buf[32], seed_buf[32], limit_buf[32];

        if (raw)
        {
            snprintf(up_buf, sizeof(up_buf), "%d", up_limit);
            snprintf(down_buf, sizeof(down_buf), "%d", dl_limit);
            snprintf(seed_buf, sizeof(seed_buf), "%d", seedtime);
            snprintf(limit_buf, sizeof(limit_buf), "%d", seedtime_limit);
        }
        else
        {
            snprintf(up_buf, sizeof(up_buf), "%d", up_limit / 1024);
            snprintf(down_buf, sizeof(down_buf), "%d", dl_limit / 1024);

            int d = seedtime / 86400;
            int h = (seedtime % 86400) / 3600;
            int m = (seedtime % 3600) / 60;
            snprintf(seed_buf, sizeof(seed_buf), "%02d:%02d:%02d", d, h, m);

            d = seedtime_limit / 86400;
            h = (seedtime_limit % 86400) / 3600;
            m = (seedtime_limit % 3600) / 60;
            snprintf(limit_buf, sizeof(limit_buf), "%02d:%02d:%02d", d, h, m);
        }

        if (show_all_clean == 0)
        {
            printf("| %-40.40s | %-6s | %-12s | %-12s | %-12s | %-12s | %-12s | %-8s | %-16s |\n",
                   name, hash6, up_buf, down_buf, state, seed_buf, limit_buf, category, tags);
        }
        else
        {
            printf("%-40.40s %-6s %-12s %-12s %-12s %-12s %-12s %-8s %-16s\n",
                   name, hash6, up_buf, down_buf, state, seed_buf, limit_buf, category, tags);
        }
    }

    if (show_all_clean == 0)
    {
        printf("+------------------------------------------+--------+--------------+--------------+--------------+--------------+--------------+----------+------------------+\n");
    }

    cJSON_Delete(root);
    return EXIT_OK;
}

/* !================ SETTERS ================! */

/* request helper */
bool qbt_request(CURL *curl,
                 const char *endpoint,
                 const char *postfields,
                 struct memory *out_mem)
{
    if (!curl || !endpoint) {
        fprintf(stderr, "[ERROR] Invalid arguments to qbt_request\n");
        return false;
    }
    char url[512];
    snprintf(url, sizeof(url), "%s%s", creds.qbt_url, endpoint);

    int attempt = 0;
    retry_request:

    /* Clean state */
    curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, NULL);
    curl_easy_setopt(curl, CURLOPT_HTTPGET, 0L);
    curl_easy_setopt(curl, CURLOPT_POST, 0L);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, NULL);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, NULL);

    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 15L);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 5L);

    if (postfields) {
        curl_easy_setopt(curl, CURLOPT_POST, 1L);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, postfields);
    } else {
        curl_easy_setopt(curl, CURLOPT_HTTPGET, 1L);
    }
    if (out_mem) {
        out_mem->size = 0;
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_cb);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, out_mem);
    } else {
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, discard_cb);
    }
    CURLcode res = curl_easy_perform(curl);

    if (res != CURLE_OK) {
        fprintf(stderr, "[ERROR] CURL failed: %s\n",
                curl_easy_strerror(res));
        return false;
    }
    long http_code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);

    /* AUTO RELOGIN LOGIC */
    if (http_code == 403 && attempt == 0) {
        attempt++;

        fprintf(stderr, "[INFO] Session expired. Re-authenticating...\n");

        if (!login_qbt(curl)) {
            fprintf(stderr, "[ERROR] Re-login failed\n");
            return false;
        }
        goto retry_request;
    }
    if (http_code < 200 || http_code >= 300) {
        fprintf(stderr, "[ERROR] HTTP error: %ld\n", http_code);
        return false;
    }
    return true;
}
/* ================= SETTER HELPERS ================= */


/* Helper to parse upload/download limits */
static bool parse_limit(const char *input, long long *out_value)
{
    if (!out_value) return false;

    if (!input || !input[0]) {
        *out_value = -1;
        return true;
    }

    char digits[32];
    size_t in_i = 0;
    size_t out_i = 0;

    if (input[in_i] == '-') {
        digits[out_i++] = '-';
        in_i++;
    }

    while (input[in_i] &&
        isdigit((unsigned char)input[in_i]) &&
        out_i < sizeof(digits) - 1)
    {
        digits[out_i++] = input[in_i++];
    }

    digits[out_i] = '\0';

    if (out_i == 0 || (out_i == 1 && digits[0] == '-'))
        return false;

    long long value = atoll(digits);

    if (input[in_i]) {
        if (tolower((unsigned char)input[in_i]) == 'k')
            value *= 1024LL;
        else
            return false;
    }

    *out_value = value;
    return true;
}

/* Helper for setters (raw/non-raw) */
int int_validate_set_value(const char *value_str)
{
    if (!value_str) return -1;

    char val_lower[16];
    size_t i;
    for (i = 0; i < sizeof(val_lower)-1 && value_str[i]; i++)
        val_lower[i] = tolower((unsigned char)value_str[i]);
    val_lower[i] = '\0';

    if (raw == 1) {
        if (strcmp(val_lower, "true") == 0) return 1;
        if (strcmp(val_lower, "false") == 0) return 0;
        ERR("raw=1 mode: value must be 'true' or 'false'");
        return -1;
    } else {
        if (strcmp(val_lower, "0") == 0) return 0;
        if (strcmp(val_lower, "1") == 0) return 1;
        ERR("Invalid value. Must be 0 or 1 in non-raw mode");
        return -1;
    }
}

/* ================= SET UPLOAD LIMIT ================= */
int set_up_limit(CURL *curl, const char *limit_str)
{
    if (!curl) {
        ERR("CURL handle is NULL");
        return EXIT_SET_FAIL;
    }

    long long parsed_val = -1;
    if (!parse_limit(limit_str, &parsed_val)) {
        ERR("Invalid upload limit: %s", limit_str);
        return EXIT_BAD_ARGS;
    }

    long limit = (parsed_val >= 0 && !raw) ? (long)parsed_val * 1024 : (long)parsed_val;

    char *escaped = curl_easy_escape(curl, creds.qbt_hash, 0);
    if (!escaped) {
        ERR("Failed to escape hash");
        return EXIT_FETCH_FAIL;
    }

    char postdata[128];
    if (snprintf(postdata, sizeof(postdata),
        "hashes=%s&limit=%ld", escaped, limit) >= (int)sizeof(postdata)) {
        curl_free(escaped);
        return EXIT_SET_FAIL;
        }

        curl_free(escaped);

        if (!qbt_request(curl, "/api/v2/torrents/setUploadLimit", postdata, NULL)) {
            ERR("Failed to set upload limit");
            return EXIT_SET_FAIL;
        }

        return EXIT_OK;
}

/* ================= SET DOWNLOAD LIMIT ================= */
int set_dl_limit(CURL *curl, const char *limit_str)
{
    if (!curl) { ERR("CURL handle is NULL");
        return EXIT_SET_FAIL;
    }

    long long parsed_val = -1;
    if (!parse_limit(limit_str, &parsed_val)) {
        ERR("Invalid download limit: %s", limit_str);
        return EXIT_BAD_ARGS;
    }

    long limit = (parsed_val >= 0 && !raw) ? (long)parsed_val * 1024 : (long)parsed_val;

    char *escaped = curl_easy_escape(curl, creds.qbt_hash, 0);
    if (!escaped) {
        ERR("Failed to escape hash");
        return EXIT_SET_FAIL;
    }

    char postdata[128];
    if (snprintf(postdata, sizeof(postdata),
        "hashes=%s&limit=%ld", escaped, limit) >= (int)sizeof(postdata)) {
        ERR("POST buffer overflow");
        curl_free(escaped);
        return EXIT_SET_FAIL;
    }

    curl_free(escaped);

    if (!qbt_request(curl, "/api/v2/torrents/setDownloadLimit", postdata, NULL)) {
        ERR("Failed to set download limit to %ld", limit);
        return EXIT_SET_FAIL;
    }

    return EXIT_OK;
}

/* ================= SET SEEDTIME LIMIT ================= */
int set_seedtime_limit(CURL *curl, const char *seedtime_str)
{
    if (!curl || !seedtime_str) {
        ERR("Invalid arguments");
        return EXIT_BAD_ARGS;
    }
    if (!creds.qbt_hash[0]) {
        ERR("No torrent hash selected");
        return EXIT_BAD_ARGS;
    }

    int single_loaded = 0;
    if (!ensure_single_loaded(curl, &single_loaded)) {
        ERR("Failed to load torrent info");
        return EXIT_FETCH_FAIL;
    }

    long seedtime_seconds = 0;
    if (!raw) {
        int dd = 0, hh = 0, mm = 0;
        if (sscanf(seedtime_str, "%d:%d:%d", &dd, &hh, &mm) != 3) {
            ERR("Invalid DD:HH:MM format '%s'", seedtime_str);
            return EXIT_BAD_ARGS;
        }
        if (dd < 0 || hh < 0 || mm < 0) {
            ERR("Negative values not allowed");
            return EXIT_BAD_ARGS;
        }
        seedtime_seconds = dd*86400L + hh*3600L + mm*60L;
    } else {
        for (size_t i = 0; seedtime_str[i]; i++)
        {
            if (!isdigit((unsigned char)seedtime_str[i]))
            {
                ERR("Invalid numeric seedtime '%s'", seedtime_str);
                return EXIT_BAD_ARGS;
            }
        }

        seedtime_seconds = atol(seedtime_str);
    }

    char *escaped = curl_easy_escape(curl, creds.qbt_hash, 0);
    if (!escaped) {
        ERR("Failed to escape hash");
        return EXIT_SET_FAIL;
    }

    char postdata[512];
    if (snprintf(postdata, sizeof(postdata),
        "hashes=%s&ratioLimit=%.5f&seedingTimeLimit=%ld&inactiveSeedingTimeLimit=-1",
        escaped, torrent.ratio_limit, seedtime_seconds) >= (int)sizeof(postdata)) {
        ERR("POST buffer overflow");
        curl_free(escaped);
        return EXIT_SET_FAIL;
        }

        curl_free(escaped);

    if (!qbt_request(curl, "/api/v2/torrents/setShareLimits", postdata, NULL)) {
        ERR("Failed to set seedtime limit");
        return EXIT_FETCH_FAIL;
    }

    return EXIT_OK;
}

/* ================= SET RATIO LIMIT ================= */
int set_ratio_limit(CURL *curl, const char *ratio_str)
{
    if (!curl || !ratio_str || !creds.qbt_hash[0]) {
        ERR("Invalid arguments");
        return EXIT_BAD_ARGS;
    }

    int single_loaded = 0;
    if (!ensure_single_loaded(curl, &single_loaded)) {
        ERR("Failed to load torrent info");
        return EXIT_FETCH_FAIL;
    }

    char *endptr = NULL;
    double ratio = strtod(ratio_str, &endptr);
    if (endptr == ratio_str || *endptr != '\0' || ratio < 0.0) {
        ERR("Invalid ratio value: %s", ratio_str);
        return EXIT_BAD_ARGS;
    }

    char ratio_buf[32];
    snprintf(ratio_buf, sizeof(ratio_buf), "%.5f", ratio);

    char *escaped = curl_easy_escape(curl, creds.qbt_hash, 0);
    if (!escaped) {
        ERR("Failed to escape hash");
        return EXIT_SET_FAIL;
    }

    char postdata[512];
    if (snprintf(postdata, sizeof(postdata),
        "hashes=%s&ratioLimit=%s&seedingTimeLimit=%d&inactiveSeedingTimeLimit=-1",
        escaped, ratio_buf, torrent.seedtime_limit) >= (int)sizeof(postdata)) {
        ERR("POST buffer overflow");
        curl_free(escaped);
        return EXIT_SET_FAIL;
    }
        curl_free(escaped);

    if (!qbt_request(curl, "/api/v2/torrents/setShareLimits", postdata, NULL)) {
        ERR("Failed to set ratio limit");
        return EXIT_SET_FAIL;
    }

    return EXIT_OK;
}

/* ================= SET CATEGORY ================= */
int set_category(CURL *curl, const char *category)
{
    if (!curl) {
        ERR("CURL handle is NULL");
        return EXIT_BAD_ARGS;
    }

    if (!creds.qbt_hash[0]) {
        ERR("No torrent hash selected");
        return EXIT_BAD_ARGS;
    }

    char *escaped_hash = curl_easy_escape(curl, creds.qbt_hash, 0);
    if (!escaped_hash) {
        ERR("Failed to escape hash");
        return EXIT_SET_FAIL;
    }

    /* ---------- CLEAR CATEGORY ---------- */
    if (!category || !category[0]) {

        char postdata[256];
        if (snprintf(postdata, sizeof(postdata),
            "hashes=%s&category=", escaped_hash) >= (int)sizeof(postdata)) {
            ERR("POST buffer overflow");
            curl_free(escaped_hash);
            return EXIT_SET_FAIL;
        }

        curl_free(escaped_hash);

        if (!qbt_request(curl,
            "/api/v2/torrents/setCategory",
            postdata,
            NULL)) {
            ERR("Failed to clear category");
            return EXIT_SET_FAIL;
        }

        return EXIT_OK;
    }

    /* ---------- CHECK IF CATEGORY EXISTS ---------- */

    struct memory mem;
    mem.data = malloc(MAX_JSON);
    if (!mem.data) {
        ERR("Memory allocation failed");
        curl_free(escaped_hash);
        return EXIT_SET_FAIL;
    }

    mem.size = 0;
    mem.data[0] = 0;

    if (!qbt_request(curl,
        "/api/v2/torrents/categories",
        NULL,
        &mem)) {
        ERR("Failed to fetch categories");
        free(mem.data);
        curl_free(escaped_hash);
        return EXIT_FETCH_FAIL;
    }

    bool exists = false;

    cJSON *root = cJSON_Parse(mem.data);
    free(mem.data);

    if (root && cJSON_IsObject(root)) {
        for (cJSON *child = root->child; child; child = child->next) {
            if (child->string) {
                if (strcasecmp(child->string, category) == 0) {
                    exists = true;
                    break;
                }
            }
        }
    }

    if (root) {
        cJSON_Delete(root);
    }

    /* ---------- CREATE CATEGORY IF MISSING ---------- */

    if (!exists) {

        char *escaped_create = curl_easy_escape(curl, category, 0);
        if (!escaped_create) {
            ERR("Failed to escape category for creation");
            curl_free(escaped_hash);
            return EXIT_SET_FAIL;
        }

        char postcreate[256];
        if (snprintf(postcreate, sizeof(postcreate),
            "category=%s", escaped_create) >= (int)sizeof(postcreate)) {
            ERR("POST buffer overflow for category creation");
            curl_free(escaped_create);
            curl_free(escaped_hash);
            return EXIT_SET_FAIL;
        }

        curl_free(escaped_create);

        if (!qbt_request(curl,
            "/api/v2/torrents/createCategory",
            postcreate,
            NULL)) {
            ERR("Failed to create category '%s'", category);
            curl_free(escaped_hash);
            return EXIT_SET_FAIL;
        }
    }

    /* ---------- SET CATEGORY ---------- */

    char *escaped_category = curl_easy_escape(curl, category, 0);
        if (!escaped_category) {
            ERR("Failed to escape category");
            curl_free(escaped_hash);
            return EXIT_SET_FAIL;
        }

        char postdata[512];
        if (snprintf(postdata, sizeof(postdata),
            "hashes=%s&category=%s",
            escaped_hash,
            escaped_category) >= (int)sizeof(postdata)) {
            ERR("POST buffer overflow");
            curl_free(escaped_hash);
            curl_free(escaped_category);
            return EXIT_SET_FAIL;
        }

        curl_free(escaped_hash);
        curl_free(escaped_category);

        if (!qbt_request(curl,
            "/api/v2/torrents/setCategory",
            postdata,
            NULL)) {
            ERR("Failed to set category '%s'", category);
            return EXIT_SET_FAIL;
        }
  return EXIT_OK;
}

/* ================= SET TAGS ================= */
int set_tags(CURL *curl,const char *tags_str)
{
    if(!curl) {
        ERR("CURL handle is NULL");
        return EXIT_BAD_ARGS;
    }
    if(!tags_str || !tags_str[0]) {
        ERR("Invalid tags string");
        return EXIT_BAD_ARGS;
    }
    if(!creds.qbt_hash[0]) {
        ERR("No torrent hash selected");
        return EXIT_BAD_ARGS;
    }

    char *escaped_hash=curl_easy_escape(curl,creds.qbt_hash,0);
    if(!escaped_hash) {
        ERR("Failed to escape hash");
        return EXIT_SET_FAIL;
    }
    char *escaped_tags=curl_easy_escape(curl,tags_str,0);
    if(!escaped_tags){
        curl_free(escaped_hash);
        ERR("Failed to escape tags");
        return EXIT_SET_FAIL;
    }

    char postdata[1024];
    if(snprintf(postdata,sizeof(postdata),"hashes=%s&tags=%s",escaped_hash,escaped_tags)>= (int)sizeof(postdata)) {
        ERR("POST buffer overflow"); curl_free(escaped_hash); curl_free(escaped_tags);
        return EXIT_SET_FAIL;
    }

    curl_free(escaped_hash); curl_free(escaped_tags);

    if(!qbt_request(curl,"/api/v2/torrents/setTags",postdata,NULL)) {
        ERR("Failed to set tags to '%s'",tags_str);
        return EXIT_SET_FAIL;
    }

    return EXIT_OK;
}

/* ================= SET SUPERSEED ================= */
int set_superseed(CURL *curl,const char *val)
{
    if(!curl || !val || !creds.qbt_hash[0]) {
        ERR("Invalid arguments");
        return EXIT_BAD_ARGS;
    }
    int valid=int_validate_set_value(val);
    if(valid==-1) return EXIT_BAD_ARGS;

    const char *bool_str=(valid==1)?"true":"false";

    char *escaped=curl_easy_escape(curl,creds.qbt_hash,0);
    if(!escaped){
        ERR("Failed to escape hash");
        return EXIT_SET_FAIL;
    }

    char postdata[128]; if(snprintf(postdata,sizeof(postdata),"hashes=%s&value=%s",escaped,bool_str)>= (int)sizeof(postdata)){
        ERR("POST buffer overflow");
        curl_free(escaped);
        return EXIT_SET_FAIL;
    }

    curl_free(escaped);

    if(!qbt_request(curl,"/api/v2/torrents/setSuperSeeding",postdata,NULL)){
        ERR("Failed to set superseed");
        return EXIT_SET_FAIL;
    }
    return EXIT_OK;
}

/* ================= SET AUTOTMM ================= */
int set_autotmm(CURL *curl,const char *val)
{
    if(!curl || !val || !creds.qbt_hash[0]) {
        ERR("Invalid arguments");
        return EXIT_BAD_ARGS;

    }
    int valid=int_validate_set_value(val);
    if(valid==-1) return EXIT_BAD_ARGS;
    const char *bool_str=(valid==1)?"true":"false";

    char *escaped=curl_easy_escape(curl,creds.qbt_hash,0); if(!escaped){
        ERR("Failed to escape hash");
        return EXIT_SET_FAIL;
    }

    char postdata[128]; if(snprintf(postdata,sizeof(postdata),"hashes=%s&enable=%s",escaped,bool_str)>= (int)sizeof(postdata)){
        ERR("POST buffer overflow");
        curl_free(escaped);
        return EXIT_SET_FAIL;
    }

    curl_free(escaped);

    if(!qbt_request(curl,"/api/v2/torrents/setAutoManagement",postdata,NULL)){
        ERR("Failed to set autotmm");
        return EXIT_SET_FAIL;
    }

    return EXIT_OK;
}

/* ================= SET SEQUENTIAL DOWNLOAD ================= */
int set_seqdl(CURL *curl, const char *val)
{
    if (!curl || !val || !creds.qbt_hash[0]) {
        ERR("Invalid arguments");
        return EXIT_BAD_ARGS;
    }

    int valid = int_validate_set_value(val);
    if (valid == -1) return EXIT_BAD_ARGS;

    bool desired = (valid != 0);
    // Load current torrent info
    if (!populate_torrent_info_struct(curl)) {
        ERR("Failed to load torrent info");
        return EXIT_FETCH_FAIL;
    }
    if (torrent.seq_dl == desired) return EXIT_OK; // already correct

    // Toggle sequential download
    char *escaped = curl_easy_escape(curl, creds.qbt_hash, 0);
    if (!escaped) {
        ERR("Failed to escape hash");
        return EXIT_SET_FAIL;
    }
    char postdata[128];
    if (snprintf(postdata, sizeof(postdata), "hashes=%s", escaped) >= (int)sizeof(postdata)) {
        ERR("POST buffer overflow"); curl_free(escaped);
        return EXIT_SET_FAIL;
    }
    curl_free(escaped);

    if (!qbt_request(curl, "/api/v2/torrents/toggleSequentialDownload", postdata, NULL)) {
        ERR("Failed to toggle sequential download");
        return EXIT_SET_FAIL;
    }
    // Reload and confirm change
    if (!populate_torrent_info_struct(curl)) {
        ERR("Failed to reload torrent info");
        return EXIT_FETCH_FAIL; }
    if (torrent.seq_dl != desired) {
        ERR("Sequential download toggle failed");
        return EXIT_SET_FAIL;
    }
    return EXIT_OK;
}
/* !================ ACTIONS ================! */
/*action helper*/
static int qbt_action(CURL *curl, const char *endpoint, const char *post)
{
    if (!curl || !creds.qbt_hash[0]) {
        ERR("qbt_action: invalid arguments");
        return EXIT_BAD_ARGS;
    }

    char *esc = curl_easy_escape(curl, creds.qbt_hash, 0);
    if (!esc) {
        ERR("qbt_action: hash escape failed");
        return EXIT_BAD_ARGS;
    }

    char postdata[512];

    if (post && post[0] != '\0') {
        if (snprintf(postdata, sizeof(postdata),
            "hashes=%s&%s", esc, post) >= (int)sizeof(postdata)) {
            curl_free(esc);
        return EXIT_ACTION_FAIL;
            }
    } else {
        if (snprintf(postdata, sizeof(postdata),
            "hashes=%s", esc) >= (int)sizeof(postdata)) {
            curl_free(esc);
        return EXIT_ACTION_FAIL;
            }
    }

    curl_free(esc);

    if (!qbt_request(curl, endpoint, postdata, NULL)) {
        ERR("qbt_action: request failed for %s", endpoint);
        return EXIT_ACTION_FAIL;
    }

    return EXIT_OK;
}
int pause_torrent(CURL *curl)
{
    int rc = qbt_action(curl, "/api/v2/torrents/stop", NULL);
    return rc;
}

int start_torrent(CURL *curl)
{
    int rc = qbt_action(curl, "/api/v2/torrents/start", NULL);
    return rc;
}
int force_start_torrent(CURL *curl)
{
   int rc = qbt_action(curl, "/api/v2/torrents/setForceStart", "value=true");
   return rc;
}
int move_torrent(CURL *curl, const char *path)
{
    if (!curl || !path || !path[0]) {
        ERR("move_torrent: invalid path");
        return EXIT_BAD_ARGS;
    }

    char *esc = curl_easy_escape(curl, path, 0);
    if (!esc) {
        ERR("move_torrent: path escape failed");
        return EXIT_ACTION_FAIL;
    }

    char post[256];
    if (snprintf(post, sizeof(post), "location=%s", esc) >= (int)sizeof(post)) {
        curl_free(esc);
        return EXIT_ACTION_FAIL;
    }

    curl_free(esc);
    int rc = qbt_action(curl, "/api/v2/torrents/setLocation", post);
    return rc;
}

int stop_and_remove_torrent(CURL *curl, bool delete_files)
{
    if (!pause_torrent(curl))
        return EXIT_ACTION_FAIL; /* Try to stop torrent first */

    int rc={0};
    if (delete_files) {
        rc = qbt_action(curl, "/api/v2/torrents/delete", "deleteFiles=true");
        return rc;
    }
    rc = qbt_action(curl, "/api/v2/torrents/delete", "deleteFiles=false");
    return rc;
}

/* ================= OPTIONAL HTTPS (todo)SUPPORT ================= */
 static void configure_https_if_needed(CURL *curl)
 {
     if (!curl)
         return;
     if (strncmp(creds.qbt_url, "https://", 8) == 0) {
         /* Enable strict SSL verification */
         curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
         curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 2L);
         /* Optional: safer defaults */
         curl_easy_setopt(curl, CURLOPT_USE_SSL, CURLUSESSL_ALL);
         /* Optional timeouts (hardening) */
         curl_easy_setopt(curl, CURLOPT_TIMEOUT, 15L);
         curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 5L);
     }
 }
/* ================= MAIN ================= */

int main(int argc, char **argv)
{

    if (argc == 1) {
        printf("No command specified. Use qbtctl --help\n");
        exit(EXIT_BAD_ARGS);
    }

    if (strcmp(argv[1], "--help") == 0){
        show_help();
        exit(EXIT_OK);
    }

    /* =========== CHECK AUTH / SET CREDS ============== */
    int rc = init_auth(argc, argv);
    if (rc != EXIT_OK) {
        ERR("Authentication initialization failed. ERROR CODE: [%d]",rc);
        return rc;
    }

    bool did_action = false;
    bool need_single_hash = false;
    int single_loaded = 0;

    /* ================= PARSE CLI ================= */
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--help") == 0) {
            show_help();
            return EXIT_OK;
        }
        if (strcmp(argv[i], "-r") == 0 || strcmp(argv[i], "--raw") == 0)
            raw = 1;

        if ((strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--hash") == 0) && i + 1 < argc)
            safe_copy(creds.qbt_hash, sizeof(creds.qbt_hash), argv[++i]);

        /* Detect commands needing single hash */
        if (strcmp(argv[i],  "-s") == 0 || strcmp(argv[i], "--show-single") == 0 ||
            strcmp(argv[i],  "-sj") == 0 || strcmp(argv[i], "--show-single-json") == 0 ||
            /*Getters*/
            strcmp(argv[i], "-gn") == 0 || strcmp(argv[i], "--get-name") == 0 ||
            strcmp(argv[i], "-gt") == 0 || strcmp(argv[i], "--get-tags") == 0 ||
            strcmp(argv[i], "-gc") == 0 || strcmp(argv[i], "--get-category") == 0 ||
            strcmp(argv[i], "-gul") == 0 || strcmp(argv[i], "--get-up-limit") == 0 ||
            strcmp(argv[i], "-gdl") == 0 || strcmp(argv[i], "--get-dl-limit") == 0 ||
            strcmp(argv[i], "-gdp") == 0 || strcmp(argv[i], "--get-dl-path") == 0 ||
            strcmp(argv[i], "-grl") == 0 || strcmp(argv[i], "--get-ratio-limit") == 0 ||
            strcmp(argv[i], "-gs") == 0 || strcmp(argv[i], "--get-seedtime") == 0 ||
            strcmp(argv[i], "-gsl") == 0 || strcmp(argv[i], "--get-seedtime-limit") == 0 ||
            strcmp(argv[i], "-gsd") == 0 || strcmp(argv[i], "--get-seqdl") == 0 ||
            strcmp(argv[i], "-gat") == 0 || strcmp(argv[i], "--get-autotmm") == 0 ||
            strcmp(argv[i], "-gss") == 0 || strcmp(argv[i], "--get-superseed") == 0 ||
            strcmp(argv[i], "-gtr") == 0 || strcmp(argv[i], "--get-tracker") == 0 ||
            strcmp(argv[i], "-gp") == 0 || strcmp(argv[i], "--get-private") == 0 ||
            strcmp(argv[i], "-gtl") == 0 || strcmp(argv[i], "--get-tracker-list") == 0 ||
            strcmp(argv[i], "-gr") == 0 || strcmp(argv[i], "--get-ratio") == 0 ||
            strcmp(argv[i], "-gus") == 0 || strcmp(argv[i], "--get-up-speed") == 0 ||
            strcmp(argv[i], "-gds") == 0 || strcmp(argv[i], "--get-dl-speed") == 0 ||
            strcmp(argv[i], "-gsz") == 0 || strcmp(argv[i], "--get-size") == 0 ||
            strcmp(argv[i], "-gud") == 0 || strcmp(argv[i], "--get-uploaded") == 0 ||
            strcmp(argv[i], "-gdd") == 0 || strcmp(argv[i], "--get-downloaded") == 0 ||
            strcmp(argv[i], "-ge") == 0 || strcmp(argv[i], "--get-eta") == 0 ||
            strcmp(argv[i], "-gst") == 0 || strcmp(argv[i], "--get-state") == 0 ||
            strcmp(argv[i], "-gpr") == 0 || strcmp(argv[i], "--get-progress") == 0 ||
            /*Setters*/
            strcmp(argv[i], "-sc") == 0 || strcmp(argv[i], "--set-category") == 0 ||
            strcmp(argv[i], "-sul") == 0 || strcmp(argv[i], "--set-up-limit") == 0 ||
            strcmp(argv[i], "-sdl") == 0 || strcmp(argv[i], "--set-dl-limit") == 0 ||
            strcmp(argv[i], "-sst") == 0 || strcmp(argv[i], "--set-seedtime-limit") == 0 ||
            strcmp(argv[i], "-srl") == 0 || strcmp(argv[i], "--set-ratio-limit") == 0 ||
            strcmp(argv[i], "-st") == 0 || strcmp(argv[i], "--set-tags") == 0 ||
            strcmp(argv[i], "-ssd") == 0 || strcmp(argv[i], "--set-seqdl") == 0 ||
            strcmp(argv[i], "-sat") == 0 || strcmp(argv[i], "--set-autotmm") == 0 ||
            strcmp(argv[i], "-sss") == 0 || strcmp(argv[i], "--set-superseed") == 0 ||
            /*Actions*/
            strcmp(argv[i], "-am") == 0 || strcmp(argv[i], "--move") == 0 ||
            strcmp(argv[i], "-as") == 0 || strcmp(argv[i], "--start") == 0 ||
            strcmp(argv[i], "-af") == 0 || strcmp(argv[i], "--force-start") == 0 ||
            strcmp(argv[i], "-ap") == 0 || strcmp(argv[i], "--pause") == 0 ||
            strcmp(argv[i], "-ar") == 0 || strcmp(argv[i], "--remove") == 0 ||
            strcmp(argv[i], "-ad") == 0 || strcmp(argv[i], "--delete") == 0)
        {
            need_single_hash = true;
        }
    }
    /* ================= INIT CURL ================= */
    curl_global_init(CURL_GLOBAL_DEFAULT);
    CURL *curl = curl_easy_init();

    if (!curl) return EXIT_FETCH_FAIL;

    /* Enable HTTPS automatically if URL starts with https:// */
    configure_https_if_needed(curl);

    curl_easy_setopt(curl, CURLOPT_COOKIEFILE, "");
    curl_easy_setopt(curl, CURLOPT_COOKIEJAR, "");

    rc = login_qbt(curl);
    if (rc!= EXIT_OK) {
        curl_easy_cleanup(curl);
        curl_global_cleanup();
        return rc;
    }

 /* =============== RESOLVE AND VALIDATE SHORT HASH ============= */
    if (need_single_hash) {
        if (strlen(creds.qbt_hash) == 0) {
            fprintf(stderr, "Hash required for this operation\n");
            curl_easy_cleanup(curl);
            curl_global_cleanup();
            exit(EXIT_BAD_ARGS);
        }
        if (!resolve_and_validate_hash(curl, creds.qbt_hash)) {
            fprintf(stderr, "Error: invalid or ambiguous hash '%s'\n", creds.qbt_hash);
            curl_easy_cleanup(curl);
            curl_global_cleanup();
            exit(EXIT_FETCH_FAIL);
        }
    }

    /* ================= ACTION LOOP ================= */
    for (int i = 1; i < argc; i++) {
        const char *arg = argv[i];

        if ((strcmp(arg, "-a") == 0 || strcmp(arg, "--show-all") == 0)) {
            rc = show_all_torrents_info(curl);
            if (rc != EXIT_OK) {
                curl_easy_cleanup(curl);
                curl_global_cleanup();
                return rc;
            }
            did_action = true;
        } else if ((strcmp(arg, "-ac") == 0 || strcmp(arg, "--show-all-clean") == 0)) {
            show_all_clean = 1;
            rc = show_all_torrents_info(curl);
            if (rc != EXIT_OK) {
                curl_easy_cleanup(curl);
                curl_global_cleanup();
                return rc;
            }
            show_all_clean = 0;
            did_action = true;
        } else if ((strcmp(arg, "-aj") == 0 || strcmp(arg, "--show-all-json") == 0)) {
            rc = show_all_torrents_info_json(curl);
            if (rc != EXIT_OK) {
                curl_easy_cleanup(curl);
                curl_global_cleanup();
                return rc;
            }
            did_action = true;
        } else if ((strcmp(arg, "-s") == 0 || strcmp(arg, "--show-single") == 0) && ensure_single_loaded(curl, &single_loaded)) {
               show_single_torrent_info();
               exit(EXIT_OK);
            did_action = true;
        } else if ((strcmp(arg, "-sj") == 0 || strcmp(arg, "--show-single-json") == 0)) {
            show_json = 1;
            if (!ensure_single_loaded(curl, &single_loaded)) {
                show_json = 0;
                continue;
            }
            show_json = 0;
            did_action = true;
        } else if ((strcmp(arg, "-gtl") == 0 || strcmp(arg, "--get-tracker-list") == 0) && ensure_single_loaded(curl, &single_loaded)) {
            if (!get_tracker_list(curl))
                ERR("Failed to fetch tracker list\n");
            did_action = true;
        } else if ((strcmp(arg, "-gn") == 0 || strcmp(arg, "--get-name") == 0) && ensure_single_loaded(curl, &single_loaded)) {
            printf("%s\n", get_name());
            did_action = true;
        } else if ((strcmp(arg, "-gt") == 0 || strcmp(arg, "--get-tags") == 0) && ensure_single_loaded(curl, &single_loaded)) {
            printf("%s\n", get_tags());
            did_action = true;
        } else if ((strcmp(arg, "-gc") == 0 || strcmp(arg, "--get-category") == 0) && ensure_single_loaded(curl, &single_loaded)) {
            printf("%s\n", get_category());
            did_action = true;
        } else if ((strcmp(arg, "-gul") == 0 || strcmp(arg, "--get-up-limit") == 0) && ensure_single_loaded(curl, &single_loaded)) {
            printf("%s\n", get_uplimit());
            did_action = true;
        } else if ((strcmp(arg, "-gdl") == 0 || strcmp(arg, "--get-dl-limit") == 0) && ensure_single_loaded(curl, &single_loaded)) {
            printf("%s\n", get_downlimit());
            did_action = true;
        } else if ((strcmp(arg, "-gdp") == 0 || strcmp(arg, "--get-dl-path") == 0) && ensure_single_loaded(curl, &single_loaded)) {
            printf("%s\n", get_dl_path());
            did_action = true;
        } else if ((strcmp(arg, "-grl") == 0 || strcmp(arg, "--get-ratio-limit") == 0) && ensure_single_loaded(curl, &single_loaded)) {
            printf("%s\n", get_ratio_limit());
            did_action = true;
        } else if ((strcmp(arg, "-gs") == 0 || strcmp(arg, "--get-seedtime") == 0) && ensure_single_loaded(curl, &single_loaded)) {
            printf("%s\n", get_seedtime());
            did_action = true;
        } else if ((strcmp(arg, "-gsl") == 0 || strcmp(arg, "--get-seedtime-limit") == 0) && ensure_single_loaded(curl, &single_loaded)) {
            printf("%s\n", get_seedtime_limit());
            did_action = true;
        } else if ((strcmp(arg, "-gsd") == 0 || strcmp(arg, "--get-seqdl") == 0) && ensure_single_loaded(curl, &single_loaded)) {
            printf("%s\n", get_seq_dl());
            did_action = true;
        } else if ((strcmp(arg, "-gat") == 0 || strcmp(arg, "--get-autotmm") == 0) && ensure_single_loaded(curl, &single_loaded)) {
            printf("%s\n", get_auto_tmm());
            did_action = true;
        } else if ((strcmp(arg, "-gss") == 0 || strcmp(arg, "--get-superseed") == 0) && ensure_single_loaded(curl, &single_loaded)) {
            printf("%s\n", get_superseed());
            did_action = true;
        } else if ((strcmp(arg, "-gtr") == 0 || strcmp(arg, "--get-tracker") == 0) && ensure_single_loaded(curl, &single_loaded)) {
            printf("%s\n", get_tracker());
            did_action = true;
        } else if ((strcmp(arg, "-gp") == 0 || strcmp(arg, "--get-private") == 0) && ensure_single_loaded(curl, &single_loaded)) {
            printf("%s\n", get_private());
            did_action = true;
        } else if ((strcmp(arg, "-gr") == 0 || strcmp(arg, "--get-ratio") == 0) && ensure_single_loaded(curl, &single_loaded)) {
            printf("%s\n", get_ratio());
            did_action = true;
        } else if ((strcmp(arg, "-gus") == 0 || strcmp(arg, "--get-up-speed") == 0) && ensure_single_loaded(curl, &single_loaded)) {
            printf("%s\n", get_upspeed());
            did_action = true;
        } else if ((strcmp(arg, "-gds") == 0 || strcmp(arg, "--get-dl-speed") == 0) && ensure_single_loaded(curl, &single_loaded)) {
            printf("%s\n", get_dlspeed());
            did_action = true;
        } else if ((strcmp(arg, "-gsz") == 0 || strcmp(arg, "--get-size") == 0) && ensure_single_loaded(curl, &single_loaded)) {
            printf("%s\n", get_size());
            did_action = true;
        } else if ((strcmp(arg, "-gud") == 0 || strcmp(arg, "--get-uploaded") == 0) && ensure_single_loaded(curl, &single_loaded)) {
            printf("%s\n", get_uploaded());
            did_action = true;
        } else if ((strcmp(arg, "-gdd") == 0 || strcmp(arg, "--get-downloaded") == 0) && ensure_single_loaded(curl, &single_loaded)) {
            printf("%s\n", get_downloaded());
            did_action = true;
        } else if ((strcmp(arg, "-ge") == 0 || strcmp(arg, "--get-eta") == 0) && ensure_single_loaded(curl, &single_loaded)) {
            printf("%s\n", get_eta());
            did_action = true;
        } else if ((strcmp(arg, "-gst") == 0 || strcmp(arg, "--get-state") == 0) && ensure_single_loaded(curl, &single_loaded)) {
            printf("%s\n", get_state());
            did_action = true;
        } else if ((strcmp(arg, "-gpr") == 0 || strcmp(arg, "--get-progess") == 0) && ensure_single_loaded(curl, &single_loaded)) {
            printf("%s\n", get_progress());
            did_action = true;

        } else if ((strcmp(argv[i], "-sc") == 0 || strcmp(argv[i], "--set-category") == 0) && i + 1 < argc) {
            rc = set_category(curl, argv[++i]);
            if (rc != EXIT_OK) {
                curl_easy_cleanup(curl);
                curl_global_cleanup();
                return rc;
            }
            did_action = true;
        } else if ((strcmp(arg, "-st") == 0 || strcmp(arg, "--set-tags") == 0) && i + 1 < argc) {
            rc = set_tags(curl, argv[++i]);
            if (rc != EXIT_OK) {
                curl_easy_cleanup(curl);
                curl_global_cleanup();
                return rc;
            }
            did_action = true;
        } else if ((strcmp(arg, "-sul") == 0 || strcmp(arg, "--set-up-limit") == 0) && i + 1 < argc) {
            rc = set_up_limit(curl, argv[++i]);
            if (rc != EXIT_OK) {
                curl_easy_cleanup(curl);
                curl_global_cleanup();
                return rc;
            }
            did_action = true;
        } else if ((strcmp(arg, "-sdl") == 0 || strcmp(arg, "--set-dl-limit") == 0) && i + 1 < argc) {
            rc = set_dl_limit(curl, argv[++i]);
            if (rc != EXIT_OK) {
                curl_easy_cleanup(curl);
                curl_global_cleanup();
                return rc;
            }
            did_action = true;
        } else if ((strcmp(arg, "-srl") == 0 || strcmp(arg, "--set-ratio-limit") == 0) && i + 1 < argc) {
            rc = set_ratio_limit(curl, argv[++i]);
            if (rc != EXIT_OK) {
                curl_easy_cleanup(curl);
                curl_global_cleanup();
                return rc;
            }
            did_action = true;
        } else if ((strcmp(arg, "-sst") == 0 || strcmp(arg, "--set-seedtime-limit") == 0) && i + 1 < argc) {
            rc = set_seedtime_limit(curl, argv[++i]);
            if (rc != EXIT_OK) {
                curl_easy_cleanup(curl);
                curl_global_cleanup();
                return rc;
            }
            did_action = true;
        } else if ((strcmp(arg, "-ssd") == 0 || strcmp(arg, "--set-seqdl") == 0) && i + 1 < argc) {
            rc = set_seqdl(curl, argv[++i]);
            if (rc != EXIT_OK) {
                curl_easy_cleanup(curl);
                curl_global_cleanup();
                return rc;
            }
            did_action = true;
        } else if ((strcmp(arg, "-sat") == 0 || strcmp(arg, "--set-autotmm") == 0) && i + 1 < argc) {
            rc = set_autotmm(curl, argv[++i]);
            if (rc != EXIT_OK) {
                curl_easy_cleanup(curl);
                curl_global_cleanup();
                return rc;
            }
            did_action = true;
        } else if ((strcmp(arg, "-sss") == 0 || strcmp(arg, "--set-superseed") == 0) && i + 1 < argc) {
            rc = set_superseed(curl, argv[++i]);
            if (rc != EXIT_OK) {
                curl_easy_cleanup(curl);
                curl_global_cleanup();
                return rc;
            }
            did_action = true;
        } else if ((strcmp(arg, "-as") == 0 || strcmp(arg, "--start") == 0)) {
            rc = start_torrent(curl);
            if (rc != EXIT_OK) {
                curl_easy_cleanup(curl);
                curl_global_cleanup();
                return rc;
            }
            did_action = true;
        } else if ((strcmp(arg, "-ap") == 0 || strcmp(arg, "--pause") == 0)) {
            rc = pause_torrent(curl);
            if (rc != EXIT_OK) {
                curl_easy_cleanup(curl);
                curl_global_cleanup();
                return rc;
            }
            did_action = true;
        } else if ((strcmp(arg, "-af") == 0 || strcmp(arg, "--force-start") == 0)) {
            rc = force_start_torrent(curl);
            if (rc != EXIT_OK) {
                curl_easy_cleanup(curl);
                curl_global_cleanup();
                return rc;
            }
            did_action = true;
        } else if ((strcmp(arg, "-am") == 0 || strcmp(arg, "--move") == 0) && i + 1 < argc) {
            rc = move_torrent(curl, argv[++i]);
            if (rc != EXIT_OK) {
                curl_easy_cleanup(curl);
                curl_global_cleanup();
                return rc;
            }
            did_action = true;
        } else if ((strcmp(arg, "-ar") == 0 || strcmp(arg, "--remove") == 0)) {
            rc = stop_and_remove_torrent(curl, false);
            if (rc != EXIT_OK) {
                curl_easy_cleanup(curl);
                curl_global_cleanup();
                    return rc;
            }
            did_action = true;
        } else if ((strcmp(arg, "-ad") == 0 || strcmp(arg, "--delete") == 0)) {
            rc = stop_and_remove_torrent(curl, true);
            if (rc != EXIT_OK) {
                curl_easy_cleanup(curl);
                curl_global_cleanup();
                return rc;
            }
            did_action = true;
        }
    }

    curl_easy_cleanup(curl);
    curl_global_cleanup();

    if (!did_action) {
        if (!did_action) {
            printf("No command specified. Use qbtctl --help\n");
            return EXIT_BAD_ARGS;
        }
    }
    return EXIT_OK;
}
