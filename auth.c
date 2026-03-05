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
#ifndef ERR
#define ERR(fmt, ...) fprintf(stderr,"[ERROR] " fmt "\n",##__VA_ARGS__)
#endif

struct qbt_creds creds = {0};
static int password_empty_flag = 0;

/* SAFE STRING COPY */
static void safe_strncpy(char *dst, const char *src, size_t dst_size)
{
    if(!dst || !src || dst_size==0) return;
    size_t i=0;
    while(i+1<dst_size && src[i]) { dst[i]=src[i]; i++; }
    dst[i]='\0';
}

/* HEX conversion */
static void bytes_to_hex(const unsigned char *in, size_t len, char *out, size_t out_size)
{
    const char hex[]="0123456789ABCDEF";
    if(out_size < len*2+1) return;
    for(size_t i=0;i<len;i++){
        out[i*2] = hex[(in[i]>>4)&0xF];
        out[i*2+1] = hex[in[i]&0xF];
    }
    out[len*2]=0;
}

static void hex_to_bytes(const char *hex, unsigned char *out, size_t out_size)
{
    size_t len=strlen(hex)/2;
    if(len>out_size) return;
    for(size_t i=0;i<len;i++){
        char c1=hex[i*2], c2=hex[i*2+1];
        int hi=(c1>='A')?(c1-'A'+10):(c1>='a')?(c1-'a'+10):(c1-'0');
        int lo=(c2>='A')?(c2-'A'+10):(c2>='a')?(c2-'a'+10):(c2-'0');
        out[i]=(hi<<4)|lo;
    }
}

/* SAVE AUTH FILE */
static int save_auth_file(const char *path)
{
    if(!path || sodium_init()<0){ ERR("libsodium init failed"); return 0; }

    unsigned char key[crypto_secretbox_KEYBYTES]={0};
    memcpy(key,"qbtkeyqbtkeyqbtkeyqbtkeyqbtkey12",crypto_secretbox_KEYBYTES);

    char hex_cipher[256]={0};
    if(strlen(creds.qbt_pass) > 0){
        size_t pass_len = strlen(creds.qbt_pass);
        unsigned char nonce[crypto_secretbox_NONCEBYTES]={0};
        unsigned char cipher[128]={0};
        crypto_secretbox_easy(cipher,(unsigned char*)creds.qbt_pass,pass_len,nonce,key);
        bytes_to_hex(cipher, pass_len + crypto_secretbox_MACBYTES, hex_cipher, sizeof(hex_cipher));
    }

    /* Ensure directory exists */
    char dir[512]={0};
    safe_strncpy(dir,path,sizeof(dir));
    char *slash=strrchr(dir,'/');
    if(slash){ *slash=0; mkdir(dir,0700); }

    /* Save file */
    FILE *f=fopen(path,"w");
    if(!f){ ERR("Cannot write auth file: %s", path); return 0; }
    fprintf(f,"url=%s\nuser=%s\npassword=%s\n",creds.qbt_url,creds.qbt_user,hex_cipher);
    fclose(f);
    chmod(path,0600);

    return 1;
}

/* LOAD AUTH FILE */
static int load_auth_file(const char *path)
{
    if(sodium_init()<0){ ERR("libsodium init failed"); return 0; }

    FILE *f=fopen(path,"r");
    if(!f) return 0;

    char line[256];
    while(fgets(line,sizeof(line),f)){
        size_t l=strlen(line);
        if(l && (line[l-1]=='\n'||line[l-1]=='\r')) line[l-1]=0;

        if(strncmp(line,"user=",5)==0) safe_strncpy(creds.qbt_user,line+5,sizeof(creds.qbt_user));
        else if(strncmp(line,"password=",9)==0){
            char hex_pass[128]={0};
            safe_strncpy(hex_pass,line+9,sizeof(hex_pass));

            unsigned char cipher[128]={0};
            hex_to_bytes(hex_pass,cipher,sizeof(cipher));
            size_t cipher_len=strlen(hex_pass)/2;

            unsigned char plain[64]={0};
            unsigned char key[crypto_secretbox_KEYBYTES]={0};
            memcpy(key,"qbtkeyqbtkeyqbtkeyqbtkeyqbtkey12",crypto_secretbox_KEYBYTES);
            unsigned char nonce[crypto_secretbox_NONCEBYTES]={0};

            if(cipher_len>crypto_secretbox_MACBYTES){
                if(crypto_secretbox_open_easy(plain,cipher,cipher_len,nonce,key)!=0){
                    ERR("Failed to decrypt password in %s", path);
                    creds.qbt_pass[0]=0;
                } else safe_strncpy(creds.qbt_pass,(char*)plain,sizeof(creds.qbt_pass));
            } else creds.qbt_pass[0]=0;
        }
        else if(strncmp(line,"url=",4)==0) safe_strncpy(creds.qbt_url,line+4,sizeof(creds.qbt_url));
    }
    fclose(f);
    return 1;
}

/* INTERACTIVE SETUP */
static int interactive_setup()
{
    char tmp_url[256]={0}, tmp_user[64]={0}, tmp_pass[64]={0};
    const char *home=getenv("HOME");
    static char fallback_home[256];
    if(!home){
        const char *env_user=getenv("USER");
        if(env_user && env_user[0]) snprintf(fallback_home,sizeof(fallback_home),"/home/%s",env_user);
        else snprintf(fallback_home,sizeof(fallback_home),".");
        home=fallback_home;
    }

    printf("+------------------------------------------+\n");
    printf("Press 'q' then ENTER at any prompt to cancel.\n");

    /* URL input */
    while(1){
        printf("Enter qBittorrent URL (default http://localhost): ");
        fflush(stdout);
        if(fgets(tmp_url,sizeof(tmp_url),stdin)){
            size_t l=strlen(tmp_url);
            if(l && tmp_url[l-1]=='\n') tmp_url[l-1]=0;
            if(strcmp(tmp_url,"q")==0) return 0;
            if(strlen(tmp_url)==0) safe_strncpy(tmp_url,"http://localhost",sizeof(tmp_url));
                break;
        }
    }

    /* Port input */
    char portbuf[16]={0};
    printf("Enter port (default 8080): ");
    fflush(stdout);
    if(fgets(portbuf,sizeof(portbuf),stdin)){
        size_t l=strlen(portbuf);
        if(l && portbuf[l-1]=='\n') portbuf[l-1]=0;
        if(strcmp(portbuf,"q")==0) return 0;
        if(strlen(portbuf)==0) strcat(tmp_url,":8080");
        else { strcat(tmp_url,":"); strcat(tmp_url,portbuf); }
    }

    /* Username */
    printf("Enter username (default admin): ");
    fflush(stdout);
    if(fgets(tmp_user,sizeof(tmp_user),stdin)){
        size_t l=strlen(tmp_user);
        if(l && tmp_user[l-1]=='\n') tmp_user[l-1]=0;
        if(strcmp(tmp_user,"q")==0) return 0;
        if(strlen(tmp_user)==0) safe_strncpy(tmp_user,"admin",sizeof(tmp_user));
    }

    /* Password (hidden) */
    struct termios oldt,newt;
    tcgetattr(STDIN_FILENO,&oldt);
    newt=oldt;
    newt.c_lflag&=~ECHO;
    tcsetattr(STDIN_FILENO,TCSANOW,&newt);
    printf("Enter password (empty allowed): ");
    fflush(stdout);
    if(fgets(tmp_pass,sizeof(tmp_pass),stdin)) {
        size_t l=strlen(tmp_pass);
        if(l && tmp_pass[l-1]=='\n') tmp_pass[l-1]=0;
        if(strcmp(tmp_pass,"q")==0){
            tcsetattr(STDIN_FILENO,TCSANOW,&oldt);
            printf("\n");
            return 0;
        }
    } else tmp_pass[0]=0;
    tcsetattr(STDIN_FILENO,TCSANOW,&oldt);
    printf("\n");

    /* Copy to creds struct */
    safe_strncpy(creds.qbt_url,tmp_url,sizeof(creds.qbt_url));
    safe_strncpy(creds.qbt_user,tmp_user,sizeof(creds.qbt_user));
    safe_strncpy(creds.qbt_pass,tmp_pass,sizeof(creds.qbt_pass));

    /* Create ~/.qbtctl dir + auth file path safely */
    char dir[512]={0};
    size_t max_home_len = sizeof(dir)-strlen(DEFAULT_AUTH_DIR)-1;
    if(strlen(home) > max_home_len){
        ERR("Home path too long, cannot create auth dir");
        return 0;
    }
    snprintf(dir,sizeof(dir),"%.*s%s",(int)max_home_len,home,DEFAULT_AUTH_DIR);
    dir[sizeof(dir)-1]=0;

    struct stat st;
    if(stat(dir,&st)!=0) mkdir(dir,0700);

    char save_path[512]={0};
    size_t max_dir_len = sizeof(save_path)-strlen(DEFAULT_AUTH_FILE)-2;
    if(strlen(dir) > max_dir_len){
        ERR("Auth dir path too long, cannot create auth file path");
        return 0;
    }
    snprintf(save_path,sizeof(save_path),"%.*s/%s",(int)max_dir_len,dir,DEFAULT_AUTH_FILE);
    save_path[sizeof(save_path)-1]=0;

    /* Save auth file */
    if(!save_auth_file(save_path)){
        ERR("Failed to save auth file to %s",save_path);
        printf("+------------------------------------------+\n");
        return 0;
    }

    printf("+------------------------------------------+\n");
    printf("Auth saved to: %s\n", save_path);
    printf("URL:      [%s]\n", creds.qbt_url);
    printf("User:     [%s]\n", creds.qbt_user);
    printf("Password: [******]\n");
    printf("+------------------------------------------+\n");

    /* Test connection */
    CURL *curl = curl_easy_init();
    if(!curl){ ERR("CURL init failed"); return 0; }

    char test_url[512]={0};
    snprintf(test_url,sizeof(test_url),"%s/api/v2/app/version",creds.qbt_url);

    curl_easy_setopt(curl,CURLOPT_URL,test_url);
    curl_easy_setopt(curl,CURLOPT_USERNAME,creds.qbt_user);
    curl_easy_setopt(curl,CURLOPT_PASSWORD,creds.qbt_pass);
    curl_easy_setopt(curl,CURLOPT_NOPROGRESS,1L);
    curl_easy_setopt(curl,CURLOPT_NOBODY,1L);

    CURLcode res = curl_easy_perform(curl);
    if(res!=CURLE_OK){
        ERR("Connection test failed: %s", curl_easy_strerror(res));
        curl_easy_cleanup(curl);
        return 0;
    }
    curl_easy_cleanup(curl);
    printf("Connection successful!\n");
    printf("+------------------------------------------+\n");

    return 1;
}

/*================ INIT AUTH =================*/
bool init_auth(int argc,char **argv)
{
    int i=1, setup_mode=0;
    char cli_user[64]={0}, cli_pass[64]={0}, cli_url[256]={0};
    char *config_path=NULL;

    while(i<argc){
        if(strcmp(argv[i],"-i")==0 || strcmp(argv[i],"--setup")==0) setup_mode=1;
        else if(strcmp(argv[i],"--user")==0 && i+1<argc){ safe_strncpy(cli_user,argv[i+1],sizeof(cli_user)); i++; }
        else if(strcmp(argv[i],"--pass")==0 && i+1<argc){ safe_strncpy(cli_pass,argv[i+1],sizeof(cli_pass)); if(strlen(argv[i+1])==0) password_empty_flag=1; i++; }
        else if(strcmp(argv[i],"--url")==0 && i+1<argc){ safe_strncpy(cli_url,argv[i+1],sizeof(cli_url)); i++; }
        else if(strcmp(argv[i],"-c")==0 && i+1<argc){ config_path=argv[i+1]; i++; }
        i++;
    }

    if(setup_mode){ interactive_setup(); exit(0); }

    if(config_path) load_auth_file(config_path);
    else {
        const char *home=getenv("HOME");
        if(!home){ struct passwd *pw=getpwuid(getuid()); home=pw?pw->pw_dir:"."; }
        char home_path[512]={0};
        snprintf(home_path,sizeof(home_path),"%s%s/%s",home,DEFAULT_AUTH_DIR,DEFAULT_AUTH_FILE);
        load_auth_file(home_path);
    }

    if(cli_user[0]!=0) safe_strncpy(creds.qbt_user,cli_user,sizeof(creds.qbt_user));
    if(password_empty_flag) creds.qbt_pass[0]=0;
    else if(cli_pass[0]!=0) safe_strncpy(creds.qbt_pass,cli_pass,sizeof(creds.qbt_pass));
    if(cli_url[0]!=0) safe_strncpy(creds.qbt_url,cli_url,sizeof(creds.qbt_url));

    if(creds.qbt_user[0]==0) safe_strncpy(creds.qbt_user,"admin",sizeof(creds.qbt_user));
    if(creds.qbt_pass[0]==0 && password_empty_flag==0) safe_strncpy(creds.qbt_pass,"admin",sizeof(creds.qbt_pass));
    if(creds.qbt_url[0]==0) safe_strncpy(creds.qbt_url,"http://localhost:8080",sizeof(creds.qbt_url));

        return true;
}
