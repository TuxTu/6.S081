#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fs.h"

int match(char*, char*);

void
find(char **pattern, char *path, int pnum)
{
    char buf[512], *p;
    int root, fd;
    struct dirent de;
    struct stat st;

    if(strlen(path) + 1 + DIRSIZ + 1 > sizeof(buf)){
        fprintf(2, "find: path %s too long\n", path);
        return;
    }

    root = open(path, 0);

    strcpy(buf, path);
    p = buf + strlen(buf);
    *p++ = '/';

    while(read(root, &de, sizeof(de)) == sizeof(de)){
        if(de.inum == 0 || strcmp(".", de.name) == 0 || strcmp("..", de.name) == 0){
            continue;
        }

        memmove(p, de.name, DIRSIZ);

        if((fd = open(buf, 0)) < 0){
            fprintf(2, "find: cannot open %s\n", path);
            continue;
        }

        if(fstat(fd, &st) < 0){
            fprintf(2, "find: cannot stat %s\n", path);
            close(fd);
            continue;
        }

        switch(st.type){
            case T_FILE:
                for(int i = 0;i < pnum;i++){
                    if(match(pattern[i], de.name)){
                        printf("%s\n", buf);
                    }
                }
                close(fd);
                break;
            case T_DIR:
                find(pattern, buf, pnum);
                close(fd);
                break;
        }
    }
}

int
main(int argc, char *argv[])
{
    int fd;
    struct stat st;

    if(argc < 3)
    {
        fprintf(2, "usage: find directory pattern.\n");
        exit(1);
    }

    if((fd = open(argv[1], 0)) < 0){
        fprintf(2, "find: cannot open %s.\n", argv[1]);
        exit(1);
    }

    if(fstat(fd, &st) < 0){
        fprintf(2, "find: cannot stat %s.\n", argv[1]);
        close(fd);
        exit(1);
    }

    if(st.type != T_DIR){
        fprintf(2, "find: %s is not a directory.\n", argv[1]);
        close(fd);
        exit(1);
    }

    close(fd);

    find(argv+2, argv[1], argc - 2);

    exit(0);
}

int matchhere(char*, char*);
int matchstar(int, char*, char*);

int
match(char *re, char *text)
{
    if(re[0] == '^')
        return matchhere(re+1, text);

    return matchhere(re, text);
}

int
matchhere(char *re, char *text)
{
    if(re[0] == '\0')
        return 1;
    if(re[1] == '*')
        return matchstar(re[0], re+2, text);
    if(re[0] == '$' && re[1] == '\0')
        return *text == '\0';
    if(*text != '\0' && (re[0] == '.' || re[0] == *text))
        return matchhere(re+1, text+1);
    return 0;
}

int
matchstar(int c, char *re, char *text)
{
    do{
        if(matchhere(re, text))
            return 1;
    }while(*text != '\0' && (*text++ == c || c == '.'));

    return 0;
}

