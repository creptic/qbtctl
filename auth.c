/* auth.c by Creptic 2026 */
/* AES encryption using libsodium; interactive + CLI auth handling */

#include "auth.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <pwd.h>
#include <curl/curl.h>
#include <sodium.h>
#include <termios.h>

#define DEFAULT_AUTH_DIR "/.qbtctl"
#define DEFAULT_AUTH_FILE "auth.txt"
#define NO_CREDS_MARKER "__NO_CREDS__"

#ifndef ERR
#define ERR(fmt, ...) fprintf(stderr,"[ERROR] " fmt "\n",##__VA_ARGS__)
#endif

/* ================= EXIT CODES ================= */

#define EXIT_OK             0
#define EXIT_LOGIN_FAIL     1
#define EXIT_FETCH_FAIL     2
#define EXIT_SET_FAIL       3
#define EXIT_BAD_ARGS       4
#define EXIT_FILE           5
#define EXIT_ACTION_FAIL    6 /* Not used here future maybe */


struct qbt_creds creds = {0};


/* SAFE STRING COPY */

static void safe_strncpy(char *dst, const char *src, size_t dst_size)
{
    if(!dst || !src || dst_size==0) return;

    size_t i=0;

    while(i+1<dst_size && src[i]){
        dst[i]=src[i];
        i++;
    }

    dst[i]='\0';
}


/* HEX conversion */

static void bytes_to_hex(const unsigned char *in, size_t len, char *out, size_t out_size)
{
    const char hex[]="0123456789ABCDEF";

    if(out_size < len*2+1) return;

    for(size_t i=0;i<len;i++){
        out[i*2]   = hex[(in[i]>>4)&0xF];
        out[i*2+1] = hex[in[i]&0xF];
    }

    out[len*2]=0;
}

static void hex_to_bytes(const char *hex, unsigned char *out, size_t out_size)
{
    size_t len=strlen(hex)/2;

    if(len>out_size) return;

    for(size_t i=0;i<len;i++){

        char c1=hex[i*2];
        char c2=hex[i*2+1];

        int hi=(c1>='A')?(c1-'A'+10):(c1>='a')?(c1-'a'+10):(c1-'0');
        int lo=(c2>='A')?(c2-'A'+10):(c2>='a')?(c2-'a'+10):(c2-'0');

        out[i]=(hi<<4)|lo;
    }
}


/* SAVE AUTH FILE */

static int save_auth_file(const char *path)
{
    if(!path){
        ERR("Invalid path");
        exit(EXIT_FILE);
    }

    if(sodium_init()<0){
        ERR("libsodium init failed");
        exit(EXIT_FETCH_FAIL);
    }

    unsigned char key[crypto_secretbox_KEYBYTES]={0};

    memcpy(key,"F2B49F7E6D3C8A5B91E740D6CB28D640",
           crypto_secretbox_KEYBYTES);

    char hex_cipher[256]={0};

    if(strlen(creds.qbt_pass)>0 &&
        strcmp(creds.qbt_pass,NO_CREDS_MARKER)!=0){

        size_t pass_len=strlen(creds.qbt_pass);

    unsigned char nonce[crypto_secretbox_NONCEBYTES]={0};
    unsigned char cipher[128]={0};

    crypto_secretbox_easy(cipher,
                          (unsigned char*)creds.qbt_pass,
                          pass_len,
                          nonce,
                          key);

    bytes_to_hex(cipher,
                 pass_len+crypto_secretbox_MACBYTES,
                 hex_cipher,
                 sizeof(hex_cipher));
        }

        /* ensure directory exists */

        char dir[512]={0};

        safe_strncpy(dir,path,sizeof(dir));

        char *slash=strrchr(dir,'/');

        if(slash){
            *slash=0;
            mkdir(dir,0700);
        }

        FILE *f=fopen(path,"w");

        if(!f){
            ERR("Cannot write auth file: %s",path);
            exit(EXIT_FILE);
        }

        fprintf(f,
                "url=%s\n"
                "user=%s\n"
                "password=%s\n",
                creds.qbt_url,
                creds.qbt_user,
                hex_cipher);

        fclose(f);

        chmod(path,0600);

        return EXIT_OK;
}


/* LOAD AUTH FILE */

static int load_auth_file(const char *path)
{
    if(!path) return 0;

    if(sodium_init()<0){
        ERR("libsodium init failed");
        exit(EXIT_FETCH_FAIL);
    }

    FILE *f=fopen(path,"r");

    if(!f) return 0;   /* not fatal */

        char line[256];

    while(fgets(line,sizeof(line),f)){

        size_t l=strlen(line);

        if(l && (line[l-1]=='\n'||line[l-1]=='\r'))
            line[l-1]=0;

        if(strncmp(line,"user=",5)==0){

            safe_strncpy(creds.qbt_user,
                         line+5,
                         sizeof(creds.qbt_user));
        }

        else if(strncmp(line,"password=",9)==0){

            char hex_pass[128]={0};

            safe_strncpy(hex_pass,
                         line+9,
                         sizeof(hex_pass));

            unsigned char cipher[128]={0};

            hex_to_bytes(hex_pass,
                         cipher,
                         sizeof(cipher));

            size_t cipher_len=strlen(hex_pass)/2;

            unsigned char plain[64]={0};

            unsigned char key[crypto_secretbox_KEYBYTES]={0};

            memcpy(key,
                   "F2B49F7E6D3C8A5B91E740D6CB28D640",
                   crypto_secretbox_KEYBYTES);

            unsigned char nonce[crypto_secretbox_NONCEBYTES]={0};

            if(cipher_len>crypto_secretbox_MACBYTES){

                if(crypto_secretbox_open_easy(
                    plain,
                    cipher,
                    cipher_len,
                    nonce,
                    key)!=0){

                    ERR("Failed to decrypt password in %s",path);

                fclose(f);

                exit(EXIT_FETCH_FAIL);
                    }

                    safe_strncpy(creds.qbt_pass,
                                 (char*)plain,
                                 sizeof(creds.qbt_pass));
            }
        }

        else if(strncmp(line,"url=",4)==0){

            safe_strncpy(creds.qbt_url,
                         line+4,
                         sizeof(creds.qbt_url));
        }
    }

    fclose(f);

    return 1;   /* success */
}


/* Helper */

bool can_attempt_login(void)
{
    return (
        creds.qbt_user[0]!=0 &&
        creds.qbt_pass[0]!=0 &&
        creds.qbt_url[0]!=0 &&
        strcmp(creds.qbt_pass,NO_CREDS_MARKER)!=0
    );
}




/* INTERACTIVE SETUP */
int interactive_setup()
{
    char tmp_url[256]={0}, tmp_user[64]={0}, tmp_pass[64]={0};
    const char *home=getenv("HOME");
    static char fallback_home[256];

    if(!home){
        const char *env_user=getenv("USER");
        if(env_user && env_user[0])
            snprintf(fallback_home,sizeof(fallback_home),"/home/%s",env_user);
        else
            snprintf(fallback_home,sizeof(fallback_home),".");
        home=fallback_home;
    }

    /* Step 1: URL */
    printf("+------------------------------------------+\n");
    printf("| Step 1/6: Enter qBittorrent URL          |\n");
    printf("| Default: http://localhost                |\n");
    printf("| Type 'quit' then ENTER to cancel         |\n");
    printf("+------------------------------------------+\n");
    printf("URL: ");
    fflush(stdout);

    while(1){
        if(fgets(tmp_url,sizeof(tmp_url),stdin)){
            size_t l=strlen(tmp_url);
            if(l && tmp_url[l-1]=='\n') tmp_url[l-1]=0;
            if(strcmp(tmp_url,"quit")==0) exit(EXIT_BAD_ARGS);
            if(strlen(tmp_url)==0) safe_strncpy(tmp_url,"http://localhost",sizeof(tmp_url));
                break;
        }
    }

    /* Step 2: Port */
    char portbuf[16]={0};
    printf("+------------------------------------------+\n");
    printf("| Step 2/6: Enter port                     |\n");
    printf("| Default: 8080                            |\n");
    printf("| Type 'quit' then ENTER to cancel         |\n");
    printf("+------------------------------------------+\n");
    printf("Port: ");
    fflush(stdout);

    if(fgets(portbuf,sizeof(portbuf),stdin)){
        size_t l=strlen(portbuf);
        if(l && portbuf[l-1]=='\n') portbuf[l-1]=0;
        if(strcmp(portbuf,"quit")==0) exit(EXIT_BAD_ARGS);
        if(strlen(portbuf)==0)
            strcat(tmp_url,":8080");
        else{
            strcat(tmp_url,":");
            strcat(tmp_url,portbuf);
        }
    }

    /* Step 3: Username */
    printf("+------------------------------------------+\n");
    printf("| Step 3/6: Enter username                 |\n");
    printf("| Default: admin                           |\n");
    printf("| Type 'quit' then ENTER to cancel         |\n");
    printf("+------------------------------------------+\n");
    printf("Username: ");
    fflush(stdout);

    if(fgets(tmp_user,sizeof(tmp_user),stdin)){
        size_t l=strlen(tmp_user);
        if(l && tmp_user[l-1]=='\n') tmp_user[l-1]=0;
        if(strcmp(tmp_user,"quit")==0) exit(EXIT_BAD_ARGS);
        if(strlen(tmp_user)==0)
            safe_strncpy(tmp_user,"admin",sizeof(tmp_user));
    }

    /* Step 4: Password */
    struct termios oldt,newt;
    tcgetattr(STDIN_FILENO,&oldt);
    newt=oldt;
    newt.c_lflag&=~ECHO;
    tcsetattr(STDIN_FILENO,TCSANOW,&newt);

    printf("+------------------------------------------+\n");
    printf("| Step 4/6: Enter password                 |\n");
    printf("| Empty password will skip saving          |\n");
    printf("| Type 'quit' then ENTER to cancel         |\n");
    printf("+------------------------------------------+\n");
    printf("Password: ");
    fflush(stdout);

    if(fgets(tmp_pass,sizeof(tmp_pass),stdin)){
        size_t l=strlen(tmp_pass);
        if(l && tmp_pass[l-1]=='\n') tmp_pass[l-1]=0;
        if(strcmp(tmp_pass,"quit")==0){
            tcsetattr(STDIN_FILENO,TCSANOW,&oldt);
            printf("\n");
            exit(EXIT_BAD_ARGS);
        }
    } else tmp_pass[0]=0;

    tcsetattr(STDIN_FILENO,TCSANOW,&oldt);
    printf("\n");

    safe_strncpy(creds.qbt_url,tmp_url,sizeof(creds.qbt_url));
    safe_strncpy(creds.qbt_user,tmp_user,sizeof(creds.qbt_user));
    safe_strncpy(creds.qbt_pass,tmp_pass,sizeof(creds.qbt_pass));

    if(strlen(creds.qbt_pass)==0){
        printf("[Empty password, not saving auth file]\n");
        exit(EXIT_BAD_ARGS);
    }

    /* Step 5: Save file path */
    char dir[512]={0};
    char save_path[512]={0};

    snprintf(dir,sizeof(dir),"%s%s",home,DEFAULT_AUTH_DIR);
    mkdir(dir,0700);  // ignore if exists
    snprintf(save_path,sizeof(save_path),"%s/%s",dir,DEFAULT_AUTH_FILE);

    printf("+------------------------------------------+\n");
    printf("| Step 5/6: Enter full save path for file   \n");
    printf("| Default: %s\n", save_path);
    printf("| Press ENTER to accept default path        \n");
    printf("| Type 'quit' then ENTER to cancel          \n");
    printf("+------------------------------------------+\n");
    printf("Save path: ");
    fflush(stdout);

    char input_path[512]={0};
    if(fgets(input_path,sizeof(input_path),stdin)){
        size_t l=strlen(input_path);
        if(l && input_path[l-1]=='\n') input_path[l-1]=0;
        if(strcmp(input_path,"quit")==0) exit(EXIT_BAD_ARGS);
        if(strlen(input_path)>0)
            safe_strncpy(save_path,input_path,sizeof(save_path));
    }

    /* Step 6: Show creds and save */
    printf("+------------------------------------------+\n");
    printf("| Step 6/6: Credentials to be saved        |\n");
    printf("+------------------------------------------+\n");
    printf("URL:      [%s]\n", creds.qbt_url);
    printf("User:     [%s]\n", creds.qbt_user);
    printf("Password: [******]\n");
    printf("Saving to: %s\n", save_path);
    printf("+------------------------------------------+\n");


    int rc = save_auth_file(save_path);
    if(rc != EXIT_OK){
        ERR("Failed to save auth file");
        exit(rc);
    }

    exit (EXIT_OK);
}
/* ----------------- INIT AUTH ----------------- */
int init_auth(int argc, char **argv)
{
    char cli_user[64]={0};
    char cli_pass[64]={0};
    char cli_url[256]={0};

    char *config_path=NULL;

    int loaded=0;

    for(int i=1;i<argc;i++){

        if(strcmp(argv[i],"-i")==0 ||
            strcmp(argv[i],"--setup")==0){

            interactive_setup();
            }

            else if(strcmp(argv[i],"--user")==0 && i+1<argc){
                safe_strncpy(cli_user,argv[i+1],sizeof(cli_user));
                i++;
            }

            else if(strcmp(argv[i],"--pass")==0 && i+1<argc){
                safe_strncpy(cli_pass,argv[i+1],sizeof(cli_pass));
                i++;
            }

            else if(strcmp(argv[i],"--url")==0 && i+1<argc){
                safe_strncpy(cli_url,argv[i+1],sizeof(cli_url));
                i++;
            }

            else if(strcmp(argv[i],"-c")==0){

                if(i+1>=argc){
                    ERR("Option -c requires path");
                    exit(EXIT_BAD_ARGS);
                }
                const char *path = argv[i + 1];

                /* Make sure the file exists */
                if(access(path, R_OK) != 0)
                {
                    ERR("Config file '%s' does not exist or is not readable", path);
                    exit(EXIT_BAD_ARGS);
                }
                config_path=argv[i+1];
                i++;
            }
    }


    /* 1. -c config_path has highest priority */
    if(config_path){
        if(load_auth_file(config_path))
            loaded = 1;
    }

    /* 2. Check current directory auth.txt */
    if(!loaded){
        if(load_auth_file("auth.txt"))
            loaded = 1;
    }

    /* 3. Fallback to HOME ~/.qbtctl/auth.txt */
    if(!loaded){
        const char *home = getenv("HOME");

        if(!home){
            struct passwd *pw = getpwuid(getuid());
            home = pw ? pw->pw_dir : "/tmp";
        }

        char home_path[512] = {0};

        snprintf(home_path, sizeof(home_path),
                 "%s%s/%s",
                 home,
                 DEFAULT_AUTH_DIR,
                 DEFAULT_AUTH_FILE);

        if(load_auth_file(home_path))
            loaded = 1;
    }

    /* CLI override */

    if(cli_user[0])
        safe_strncpy(creds.qbt_user,cli_user,sizeof(creds.qbt_user));

    if(cli_pass[0])
        safe_strncpy(creds.qbt_pass,cli_pass,sizeof(creds.qbt_pass));

    if(cli_url[0])
        safe_strncpy(creds.qbt_url,cli_url,sizeof(creds.qbt_url));

    if(!can_attempt_login()){
        printf("[No credentials supplied]\n");
        exit(EXIT_FILE);
    }

    return EXIT_OK;
}
