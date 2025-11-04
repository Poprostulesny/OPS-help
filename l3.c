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

int main(int argc, char** argv){
    char * name = NULL;
    int c;
    mode_t perms =1;
    ssize_t size=-1;
    while((c=getopt(argc, argv, "p:n:s:"))!=-1){
        switch(c){
            case 'p':
                perms = strtol(optarg, (char**)NULL, 8);
                break;
            case 's':
                size = strtol(optarg, (char**)NULL,10);
                break;

            case 'n':
                name = strdup(optarg);
                break;

            case '?':
            default:
                ERR(argv[0]);

        }
    }
    if(NULL ==name||((mode_t)-1==perms)||(-1==size)){
       ERR(argv[0]);
    }

    if(unlink(name)&&errno!=ENOENT){
       ERR("unlink");
    }
    srand(time(NULL));
    
    FILE * f;
    umask(~perms&0777);
    f=fopen(name, "w+");
    if(f==NULL){
        ERR("fopen");
    }
    for(int i=0;i<size/10; i++){
        if(fseek(f, rand()%size,SEEK_SET)){
            ERR("fseek");
        }
        if(fprintf(f,"%c", 'A'+i%32)){
            ERR("fprintf");
        }
    }
    if(fclose(f)){
        ERR("fclose");
    }
    free(name);






}