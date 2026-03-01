/*======== AUTH HEADER ========*/
/* Handles qBittorrent authentication: credentials, file I/O, and CLI overrides */

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
bool init_auth(int argc, char **argv);

#endif /* AUTH_H */
