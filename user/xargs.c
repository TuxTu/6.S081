#include "kernel/types.h"
#include "kernel/param.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fs.h"

void
run(char *nargv[])
{
    if(fork() == 0)
    {
        exec(nargv[0], nargv);
        exit(0);
    } else
    {
        wait(0);
    }
}

int
main(int argc, char *argv[])
{
    int i;
    char buf[MAXARG][32];
    char *nargv[32];
    char ch;

    for(i = 0;i < MAXARG;i++)
    {
        nargv[i] = buf[i];
    }

    if(argc < 2)
    {
        fprintf(2, "usage: xargs [-n] ins\n");
        exit(1);
    }

    for(i = 1;i < argc;i++)
    {
        strcpy(buf[i-1], argv[i]);
    }

    i = 0;
    nargv[argc] = 0;
    while(read(0, &ch, 1))
    {
        if(ch == '\n' || ch == ' ')
        {
            buf[argc-1][i] = '\0';
            run(nargv);
            i = 0;
            continue;
        }
        
        if(ch == '\\')
        {
            if(!read(0, &ch, 1))
            {
                fprintf(2, "xargs: quit exceptionally!\n");
                exit(1);
            }

            if(ch == 'n')
            {
                buf[argc-1][i] = '\0';
                run(nargv);
                i = 0;
                continue;
            } else
            {
                buf[argc-1][i++] = '\\';
            }
        }

        buf[argc-1][i++] = ch;
    }

    exit(0);
}
