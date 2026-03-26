/*
 * watch.h
 * -----------------------------
 * Purpose: Interface for qbtctl watch (live monitoring) functionality.
 *          Exposes the main watch loop used by the CLI.
 *
 * Version: 1.5.0
 * Date:    2026-03-26
 *
 * Exposed Functions:
 *   - watch_all_torrents(): Starts live monitoring loop
 *
 * Notes:
 *   - Requires initialized CURL handle
 *   - Depends on auth module for credentials
 *
 * Author:  Creptic
 * GitHub:  https://github.com/creptic/qbtctl
*/
#ifndef WATCH_H
#define WATCH_H

#include <curl/curl.h>
#include <stddef.h>

// forward-declare needed helpers from qbtctl.c
void fmt_bytes(char *buf, size_t len, long long bytes);
char *qbt_get_json(CURL *curl, const char *url);

// watch functions
int watch_all_torrents(CURL *curl);

#endif
