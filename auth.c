/* auth.c by Creptic 2026*/
/* Purpose: add/load/set credentials to auth.txt password encrypted */
/* cli overrides all, if none will use defaults */
/* handles some cli (--user --pass --url -c and  -i --setup ) */

#include "auth.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <pwd.h>
#include <curl/curl.h>

#define DEFAULT_AUTH_DIR "/.qbtctl"
#define DEFAULT_AUTH_FILE "auth.txt"
#define XOR_KEY "qbtkey"

#ifndef ERR
#define ERR(fmt, ...) fprintf(stderr,"[ERROR] " fmt "\n",##__VA_ARGS__)
#endif

struct qbt_creds creds = {0};

/*======== GLOBAL FLAG: PASSWORD WAS EXPLICITLY EMPTY ========*/
static int password_empty_flag = 0;

/*======== SAFE STRING COPY ========*/
static void safe_strncpy(char *dst, const char *src, size_t dst_size)
{
    if (dst == NULL) return;
    if (src == NULL) return;
    if (dst_size == 0) return;

    size_t i = 0;

    while (i + 1 < dst_size && src[i] != '\0')
    {
        dst[i] = src[i];
        i++;
    }

    dst[i] = '\0';
}

/*======== XOR ENCRYPT/DECRYPT ========*/
static void xor_encrypt_decrypt(char *data, size_t len)
{
    size_t key_len = strlen(XOR_KEY);
    for(size_t i=0;i<len;i++) data[i] ^= XOR_KEY[i % key_len];
}

/*======== HEX CONVERSION ========*/
static void bytes_to_hex(const char *input, size_t len, char *out, size_t out_size)
{
    const char hex[]="0123456789ABCDEF";
    if(out_size < len*2+1) return;
    for(size_t i=0;i<len;i++)
    {
        out[i*2] = hex[(input[i] >> 4) & 0xF];
        out[i*2+1] = hex[input[i] & 0xF];
    }
    out[len*2] = '\0';
}

static void hex_to_bytes(const char *hex, char *out, size_t out_size)
{
    size_t len=strlen(hex)/2;
    if(len>out_size) return;
    for(size_t i=0;i<len;i++)
    {
        char c1=hex[i*2], c2=hex[i*2+1];
        int hi=(c1>='A')?(c1-'A'+10):(c1>='a')?(c1-'a'+10):(c1-'0');
        int lo=(c2>='A')?(c2-'A'+10):(c2>='a')?(c2-'a'+10):(c2-'0');
        out[i]=(hi<<4)|lo;
    }
}

/*======== PROMPT INPUT ========*/
static void prompt_input(const char *prompt, char *buffer, size_t size, int hide)
{
    printf("%s",prompt);
    if(hide) system("stty -echo");
    if(fgets(buffer,size,stdin))
    {
        size_t len=strlen(buffer);
        if(len>0 && (buffer[len-1]=='\n'||buffer[len-1]=='\r')) buffer[len-1]='\0';
    }
    if(hide){ system("stty echo"); printf("\n"); }
}

/*======== LOAD AUTH FILE ========*/
static int load_auth_file(const char *path)
{
    FILE *f=fopen(path,"r");
    if(!f) return 0;
    char line[256];
    while(fgets(line,sizeof(line),f))
    {
        size_t len=strlen(line);
        if(len>0 && (line[len-1]=='\n'||line[len-1]=='\r')) line[len-1]='\0';
        if(strncmp(line,"user=",5)==0) safe_strncpy(creds.qbt_user,line+5,sizeof(creds.qbt_user));
        else if(strncmp(line,"password=",9)==0)
        {
            char hex_pass[128]={0};
            safe_strncpy(hex_pass,line+9,sizeof(hex_pass));
            hex_to_bytes(hex_pass,creds.qbt_pass,sizeof(creds.qbt_pass));
            xor_encrypt_decrypt(creds.qbt_pass,strlen(creds.qbt_pass));
            if(strlen(creds.qbt_pass)==0) password_empty_flag=1;
        }
        else if(strncmp(line,"url=",4)==0) safe_strncpy(creds.qbt_url,line+4,sizeof(creds.qbt_url));
    }
    fclose(f);
    return 1;
}

/*======== SAVE AUTH FILE ========*/
static int save_auth_file(const char *path)
{
    char enc_pass[64];
    safe_strncpy(enc_pass,creds.qbt_pass,sizeof(enc_pass));
    xor_encrypt_decrypt(enc_pass,strlen(enc_pass));

    char hex_pass[128]={0};
    bytes_to_hex(enc_pass,strlen(enc_pass),hex_pass,sizeof(hex_pass));

    char dir[256];
    safe_strncpy(dir,path,sizeof(dir));
    char *slash=strrchr(dir,'/');
    if(slash){ *slash='\0'; mkdir(dir,0700); }

    FILE *f=fopen(path,"w");
    if(!f){ ERR("Cannot write auth file: %s",path); return 0; }
    fprintf(f,"url=%s\nuser=%s\npassword=%s\n",creds.qbt_url,creds.qbt_user,hex_pass);
    fclose(f);
    return 1;
}

/*======== INTERACTIVE SETUP ========*/
static int interactive_setup()
{
    char tmp_url[256]={0}, tmp_user[64]={0}, tmp_pass1[64]={0}, tmp_pass2[64]={0};
    const char *home=getenv("HOME");
    if(!home){ struct passwd *pw=getpwuid(getuid()); home=pw?pw->pw_dir:"."; }

    printf("+------------------------------------------+\n");
    prompt_input("Enter qBittorrent URL (http://host[:port]): ", tmp_url,sizeof(tmp_url),0);
    if(strlen(tmp_url)==0) safe_strncpy(tmp_url,"http://localhost:8080",sizeof(tmp_url));

        prompt_input("Enter username: ", tmp_user,sizeof(tmp_user),0);
    if(strlen(tmp_user)==0) safe_strncpy(tmp_user,"admin",sizeof(tmp_user));

    while(1)
    {
        prompt_input("Enter password (empty allowed): ", tmp_pass1,sizeof(tmp_pass1),1);
        prompt_input("Confirm password: ", tmp_pass2,sizeof(tmp_pass2),1);

        if(strlen(tmp_pass1)==0){ password_empty_flag=1; break; }
        if(strcmp(tmp_pass1,tmp_pass2)==0) break;
        printf("Passwords do not match. Try again.\n");
    }

    safe_strncpy(creds.qbt_url,tmp_url,sizeof(creds.qbt_url));
    safe_strncpy(creds.qbt_user,tmp_user,sizeof(creds.qbt_user));

    if(password_empty_flag) creds.qbt_pass[0]='\0';
    else safe_strncpy(creds.qbt_pass,tmp_pass1,sizeof(creds.qbt_pass));

    char default_path[256];
    snprintf(default_path,sizeof(default_path),"%s%s/%s",home,DEFAULT_AUTH_DIR,DEFAULT_AUTH_FILE);

    char save_path[256];
    prompt_input("Save auth file path (ENTER for default): ", save_path,sizeof(save_path),0);
    if(strlen(save_path)==0) safe_strncpy(save_path,default_path,sizeof(save_path));

    if(!save_auth_file(save_path)){ ERR("Failed to save auth file to %s",save_path); printf("+------------------------------------------+\n"); return 0; }

    printf("Auth saved to: %s\n", save_path);

    /*======== CONNECTION TEST ========*/
    printf("Testing connection...\n");
    CURL *curl=curl_easy_init();
    if(!curl){ ERR("CURL init failed"); return 0; }

    char test_url[512];
    snprintf(test_url,sizeof(test_url),"%s/api/v2/app/version",creds.qbt_url);

    curl_easy_setopt(curl,CURLOPT_URL,test_url);
    curl_easy_setopt(curl,CURLOPT_USERNAME,creds.qbt_user);
    curl_easy_setopt(curl,CURLOPT_PASSWORD,creds.qbt_pass);
    curl_easy_setopt(curl,CURLOPT_NOPROGRESS,1L);
    curl_easy_setopt(curl,CURLOPT_NOBODY,1L);

    CURLcode res = curl_easy_perform(curl);
    if(res != CURLE_OK){ ERR("Connection test failed: %s",curl_easy_strerror(res)); printf("+------------------------------------------+\n"); curl_easy_cleanup(curl); return 0; }

    curl_easy_cleanup(curl);
    printf("Connection successful!\n");
    printf("+------------------------------------------+\n");
    return 1;
}

/*======== INIT AUTH ========*/
bool init_auth(int argc, char **argv)
{
    int i=1, setup_mode=0;
    char *config_path=NULL;
    char cli_user[64]={0}, cli_pass[64]={0}, cli_url[256]={0};

    /* Parse CLI arguments */
    while(i<argc)
    {
        if(strcmp(argv[i],"-i")==0 || strcmp(argv[i],"--setup")==0){ setup_mode=1; }
        else if(strcmp(argv[i],"--user")==0 && i+1<argc){ safe_strncpy(cli_user,argv[i+1],sizeof(cli_user)); i++; }
        else if(strcmp(argv[i],"--pass")==0 && i+1<argc)
        {
            safe_strncpy(cli_pass,argv[i+1],sizeof(cli_pass));
            if(strlen(argv[i+1])==0) password_empty_flag=1;
            i++;
        }
        else if(strcmp(argv[i],"--url")==0 && i+1<argc){ safe_strncpy(cli_url,argv[i+1],sizeof(cli_url)); i++; }
        else if(strcmp(argv[i],"-c")==0 && i+1<argc){ config_path=argv[i+1]; i++; }
        i++;
    }

    if(setup_mode){ interactive_setup(); exit(0); }

    /* Load auth file if exists */
    if(config_path) load_auth_file(config_path);
    else
    {
        const char *home=getenv("HOME");
        if(!home){ struct passwd *pw=getpwuid(getuid()); home=pw?pw->pw_dir:"."; }
        char home_path[256];
        snprintf(home_path,sizeof(home_path),"%s%s/%s",home,DEFAULT_AUTH_DIR,DEFAULT_AUTH_FILE);
        if(!load_auth_file(home_path))
        {
            char prog_dir[256]={0};
            if(argc>0){ char *slash=strrchr(argv[0],'/'); if(slash){ safe_strncpy(prog_dir,argv[0],sizeof(prog_dir)); prog_dir[slash-argv[0]]='\0'; } else safe_strncpy(prog_dir,".",sizeof(prog_dir)); }
            else safe_strncpy(prog_dir,".",sizeof(prog_dir));
            char local_path[256];
            snprintf(local_path,sizeof(local_path),"%s/%s",prog_dir,DEFAULT_AUTH_FILE);
            load_auth_file(local_path);
        }
    }

    /* CLI overrides always take priority */
    if(cli_user[0]!=0) safe_strncpy(creds.qbt_user,cli_user,sizeof(creds.qbt_user));
    if(password_empty_flag) creds.qbt_pass[0]='\0';
    else if(cli_pass[0]!=0) safe_strncpy(creds.qbt_pass,cli_pass,sizeof(creds.qbt_pass));
    if(cli_url[0]!=0) safe_strncpy(creds.qbt_url,cli_url,sizeof(creds.qbt_url));

    /* Apply defaults only if nothing was set and password not explicitly empty */
    if(creds.qbt_user[0]==0) safe_strncpy(creds.qbt_user,"admin",sizeof(creds.qbt_user));
    if(creds.qbt_pass[0]==0 && password_empty_flag==0) safe_strncpy(creds.qbt_pass,"admin",sizeof(creds.qbt_pass));
    if(creds.qbt_url[0]==0) safe_strncpy(creds.qbt_url,"http://localhost:8080",sizeof(creds.qbt_url));

        return true;
}
