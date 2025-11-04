#define _XOPEN_SOURCE 700
#include <dirent.h>
#include <errno.h>
#include <ftw.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include "utils.h"
#define ERR(source) (perror(source), fprintf(stderr, "%s:%d\n", __FILE__, __LINE__), exit(EXIT_FAILURE))

void set(char* user, char* path, char* value)
{

    printf("%s SET %s to %s\n", user, path, value);

    char *path2 = concat(3, user,"/", path);
    if (mkdir(user, 0755) == -1)
    {
        fprintf(stderr, "user exists\n");
    }
    checked_mkdir(user);
    make_path(path2);
    
    FILE* f = fopen(path2, "w");
    if (f==NULL)
    {
        ERR("fopen");
    }
    else
    {
        fprintf(f, "%s", value);
        fclose(f);
    }
    free(path2);
}

void get(char* user, char* path)
{
    // char *cwd = getcwd(NULL, 0);
    // puts(cwd);
    printf("%s GET %s\n", user, path);
    char *path2 = concat(3, user,"/", path);

    char buff[1024];
    FILE* f = fopen(path2, "r");
    if (f == NULL)
    {
        ERR("fopen");
    }
    while (fgets(buff, sizeof(buff), f))
    {
        printf("%s", buff);
    }

    puts("\n");
    // free(cwd);
    fclose(f);
    free(path2);
}

void share(char* user, char* path, char* other_user)
{
    printf("%s SHARE %s with %s\n", user, path, other_user);
    //sprawdzamy czy plik istnieje
    //jesli istnieje
    struct stat s;
    char * path2 = concat(3, user,"/", path);
    puts(path2);
    if (lstat(path2, &s)==-1)
    {
        printf("file does not exist\n");
        ERR("lstat");
    }
    if (S_ISDIR(s.st_mode))
    {
        printf("file is a directory\n");
        ERR("wrong file");
    }
    char * name = get_link_name(user, path2);
    char * link_path = concat(3, other_user, "/",name);
    char * from = concat(3, "..", "/", path2);
    if (symlink(from, link_path) == -1)
    {

        ERR("symlink");
    }
    free(from);
    free(name);
    free(path2);
    free(link_path);


}
int dir_is_empty(const char *name) {
    DIR *d = opendir(name);
    if (!d) ERR("opendir");
    struct dirent *ent;
    while ((ent = readdir(d)) != NULL) {
        if (ent->d_name[0] == '.') {
            if (ent->d_name[1] == '\0') continue;            // "."
            if (ent->d_name[1] == '.' && ent->d_name[2]=='\0') continue; // ".."
        }
        closedir(d);
        return 0; // found something
    }
    closedir(d);
    return 1;
}

void erase_helper(const char *path, int pos)
{

    while (path[pos] == '/') pos++;
    if (path[pos] == '\0') return;


    char name[512];
    int start = pos;

    while (path[pos] != '\0' && path[pos] != '/') pos++;

    int len = pos - start;
    if (len <= 0 || len > 1000) ERR("bad component length");

    memcpy(name, path + start, len);

    name[len] = '\0';

    struct stat s;
    if (lstat(name, &s) == -1) ERR("lstat");

    if (S_ISREG(s.st_mode) || S_ISLNK(s.st_mode)) {

        if (unlink(name) == -1) ERR("unlink");
        return;
    }

    if (S_ISDIR(s.st_mode)) {

        checked_chdir(name);
        if (path[pos] == '/') pos++;
        erase_helper(path, pos);
        checked_chdir("..");


        if (dir_is_empty(name)) {
            if (rmdir(name) == -1) ERR("rmdir");
        }
        return;
    }

    // Unsupported file type
    ERR("unsupported file type");
}
int walk(const char *fpath, const struct stat *sb, int typeflag, struct FTW *ftwbuf)
{
    if (S_ISLNK(sb->st_mode))
    {
        struct stat s;
        if (stat(fpath,&s)==-1)
        {
            unlink(fpath);
        }

    }

    return 0;
}
void erase(char* user, char* path)
{
    printf("%s ERASE %s\n", user, path);
    checked_chdir(user);
    char * cwd = getcwd(NULL, 0);
    puts(cwd);
    free(cwd);
    erase_helper(path, 0);
    checked_chdir("..");
    nftw(".",walk,1000,FTW_PHYS);

}

int main(int argc, char** argv)
{

    FILE* f = fopen("script1.txt", "r");
    if (f==NULL)
    {
        ERR("fopen");
    }
    char buff[1024];

    int i = 0;
    while (fgets(buff, sizeof(buff), f))
    {
        buff[strlen(buff)-1] = '\0';
        char** input = split_string(buff, &i);

        if (i <= 1)
        {
            ERR("wrong command");
        }
        if (strcmp(input[0], "SET") == 0 && i == 4)
        {

            set(input[1], input[2], input[3]);
        }
        else if (strcmp(input[0], "GET") == 0 && i == 3)
        {
            get(input[1], input[2]);
        }
        else if (strcmp(input[0], "SHARE") == 0)
        {
            DIR*dir = opendir(".");
            if (dir == NULL){ERR("opendir");}

            struct dirent * ent;
            struct stat s;
            while ((ent = readdir(dir)) != NULL)
            {
                if (lstat(ent->d_name, &s) == -1)
                {
                    ERR("lstat");
                }
                if (S_ISDIR(s.st_mode))
                {
                    if (ent->d_name[0] == '.'||strcmp(ent->d_name, input[1])==0)
                    {
                        continue;
                    }
                    share(input[1], input[2], ent->d_name);
                }
            }
            closedir(dir);
        }
        else if (strcmp(input[0], "ERASE") == 0 && i == 3)
        {
            erase(input[1], input[2]);
        }
        else
        {
            printf("Unknown operation type\n");
            ERR("wrong command");
        }
        free_strings(input, i);
        // printf("%s\n", buff);
    }
    fclose(f);



}
