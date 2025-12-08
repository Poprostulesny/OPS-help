#define _XOPEN_SOURCE 700

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <ftw.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/uio.h>
#include <unistd.h>

#define ERR(source) (perror(source), fprintf(stderr, "%s:%d\n", __FILE__, __LINE__), exit(EXIT_FAILURE))
struct{
    char* title=NULL;
    char* author=NULL;
    char* genre=NULL;

}typedef book_t;
// join 2 path. returned pointer is for newly allocated memory and must be freed
char *join_paths(const char *path1, const char *path2)
{
    char *res;
    const int l1 = strlen(path1);
    if (path1[l1 - 1] == '/')
    {
        res = malloc(strlen(path1) + strlen(path2) + 1);
        if (!res)
            ERR("malloc");
        strcpy(res, path1);
    }
    else
    {
        res = malloc(strlen(path1) + strlen(path2) + 2); // additional space for "/"
        if (!res)
            ERR("malloc");
        strcpy(res, path1);
        res[l1] = '/';
        res[l1 + 1] = 0;
    }
    return strcat(res, path2);
}

void usage(int argc, char **argv)
{
    (void)argc;
    fprintf(stderr, "USAGE: %s path\n", argv[0]);
    exit(EXIT_FAILURE);
}

book_t parser(FILE *fptr)
{
    char *lineptr = NULL;
    size_t n = -1;
    char* token;
    char* title=NULL;
    char* author=NULL;
    char* title= NULL;
    char* genre = NULL;
    while (getline(&lineptr, &n, fptr) != -1)
    {   lineptr[strlen(lineptr)-1]='\0';
        puts(lineptr);
        token =  strtok(lineptr, ":");
        
        if((token = strtok(NULL, ":"))==NULL){
            continue;
        }
        if(strcmp(lineptr, "author")==0){
           author=  strdup(token);
        }  
         if(strcmp(lineptr, "title")==0){
            title=strdup(token);
         }
          if(strcmp(lineptr, "genre")==0){
            genre=strdup(token);
          }    
        


    }
    int walk()
    free(lineptr);
    if(author!=NULL){
        printf("author: %s\n",author);
        // free(author);
    }
    else{
        printf("author: missing!\n");
    }
    if(title!=NULL){
        printf("title: %s\n",title);
        // free(title);
    }
    else{
        printf("title: missing!\n");
    }
    if(genre!=NULL){
        printf("gerne: %s\n",genre);
        // free(genre);
    }
    else{
        printf("genre: missing!\n");
    }
    book.title=title;
    book.genre=genre;
    book.author=author;
    if(strlen(book.title)>64){
        book.title[64X ]='\0';
    }
    return book;
}
int walk(const char* name, const struct stat *s, int type, struct FTW *f){
    
    puts(name);
    if(type ==FTW_F){
        if(chdir("index/by_visible_title")==-1){
            ERR("chdir");
        }
        puts(name);
        char*k = join_paths("../../",name);
        char* j= join_paths(k, &name[f->base]);
        symlink(name, j );
        free(j);
        free(k);
        if(chdir("../..")==-1){
            ERR("chdir");
        }
        FILE * fptr;;
        fptr=fopen(path, 'r');
        book_t book = parser(fptr);
        fclose(fptr);

        if(chdir("index/by_title")==-1){
            ERR("chdir3");
        }
        char* str2 = join_paths("../../", path);
        
        symlink(str2, book.title);
        free(str2);
        if(chdir("../..")==-1){
            ERR("chdir");
        }
        
        free(book.title);
        free(book.author);
        free(book.genre);


    }
    
    return 0;
}
int main(int argc, char **argv)
{
    
    
    if(mkdir("index", 0755)==-1){
        ERR("mkdir");
    }
    if(mkdir("index/by_visible_title", 0755)==-1){
        ERR("mkdir");
    }
    if(mkdir("index/by_title", 0755)==-1){
        ERR("mkdir");
    }
    if(mkdir("index/by_genre", 0755)==-1){
        ERR("mkdir");
    }
    if(nftw("library", walk, 100, FTW_PHYS)!=0){
        ERR("nftw");
    }
    // fclose(fptr);
}
