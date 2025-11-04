#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include<string.h>
#include<unistd.h>

#define ERR(source) (perror(source), fprintf(stderr, "%s:%d\n", __FILE__, __LINE__), exit(EXIT_FAILURE))

void scan_dir()
{
   DIR * d = opendir(".");
   if(d==NULL){
    ERR("dir");
   }
   struct dirent  * s;
   struct stat filestat;
   int dir=0, fil=0, lin=0, oth=0;
   
   while((s=readdir(d))!=NULL){
    
    errno=0;
    if(lstat(s->d_name, &filestat)==-1){
        ERR("lstat");
    }
    else if(S_ISDIR(filestat.st_mode)){ 
        dir++;
    }
    else if(S_ISREG(filestat.st_mode)){
        fil++;
    }
    else if(S_ISLNK(filestat.st_mode)){
        lin++;
    }
    else {
        oth++;
    }
    
   }
   if(errno!=0){
    ERR("readdir");
   }

   
    if(closedir(d)!=0)
{
    ERR("clodedor");
}
 printf("Files: %d, Dirs: %d, Links: %d, Other: %d\n", fil, dir, lin, oth);
}

int main(int argc, char **argv)
{
    char * path = malloc(sizeof(char)*512);
    if( getcwd(path, sizeof(char)*512)==NULL){
        ERR("getcwd");
    }
    puts(path);
    scan_dir();
    
    for(int i=1;i<argc;i++){
        if(chdir(argv[i])==-1){
            ERR("chdir");
        }
        scan_dir();
        if(chdir(path)==-1){
            ERR("chdir back");
        }
    }
    free(path);
    return EXIT_SUCCESS;
}