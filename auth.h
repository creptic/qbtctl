/*
 * auth.h
 * -----------------------------
 * Purpose: Declares authentication functions, structures, and constants
 *          for the qbtctl CLI. Provides secure handling of credentials
 *          for qBittorrent Web API access.
 *
 * Version: 1.5.1
 * Date:    2026-03-29
 *
 * Features:
 *   - Defines authentication structs for username/password storage
 *   - Declares functions for initializing, validating, and saving credentials
 *   - Integrates with CLI to securely pass credentials without exposing them
 *   - Supports optional encrypted storage for local authentication
 *
 * Author:  Creptic
 * GitHub:  https://github.com/creptic/qbtctl
*/

#ifndef AUTH_H
#define AUTH_H

#include <stdbool.h>

/*======== CREDENTIAL STRUCT ========*/
/* Holds qBittorrent credentials and currently selected torrent hash */
struct qbt_creds {
    char qbt_url[256];    // qBittorrent Web API URL
    char qbt_user[64];    // Username
    char qbt_pass[64];    // Password (AES encrypted in auth file)
    char qbt_hash[41];    // Currently selected torrent hash (40 hex chars + null terminator)
};

/*======== GLOBAL VARIABLE ========*/
/* Global credentials instance */
extern struct qbt_creds creds;

/*======== AUTH INITIALIZATION ========*/
/* Initialize authentication:
   - Parse CLI arguments (--user, --pass, --url, -c, -i)
   - Load auth file if exists
   - Run interactive setup if requested
   - Apply CLI overrides
*/
int init_auth(int argc, char **argv);

#endif /* AUTH_H */
