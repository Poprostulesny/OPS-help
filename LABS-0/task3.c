#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include<string.h>
#include<unistd.h>
#include <fcntl.h>
#include<time.h>


#define ERR(source) (perror(source), fprintf(stderr, "%s:%d\n", __FILE__, __LINE__), exit(EXIT_FAILURE))

int main(int argc, char ** argv){
    int c=-1;
    char *env=NULL;
    char *ad=NULL;
    char* rm=NULL;
    int t;
    while((t=getopt(argc, argv,"v:ci:r:"))!=-1){
        switch (t)
        {
        case 'v':
            env=strdup(optarg);
            break;
        case 'c':
            c=1;
            break;
        case 'i':
            ad=strdup(optarg);
            break;

        case 'r':
            rm=strdup(optarg);
            break;

        case '?':
            puts("WRONG ARGS");
            ERR("args");
        default:
            break;
        }
    }

    if(env==NULL){
        puts("NO ENV");
        ERR("no env");
    }
    puts(env);
    puts(ad);
    puts(rm);
    printf("%d", c);


if(env!=NULL)
free(env);

if(ad!=NULL)
free(ad);

if(rm!=NULL)
free(rm);
    return EXIT_SUCCESS;
}