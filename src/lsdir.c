#ifndef _GNU_SOURCE
#define _GNU_SOURCE 
#endif
#ifndef _LARGE_FILE_SOURCE
#define _LARGE_FILE_SOURCE 
#endif
#ifndef _FILE_OFFSET_BITS
#define _FILE_OFFSET_BITS 64
#endif

#include <dirent.h>     /* Defines DT_* constants */
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <sys/time.h>
#include <limits.h>
#include <string.h>
#include <fnmatch.h>
#include <stdint.h>
#include <errno.h>

#include "lsdir.h"

#define BUF_SIZE 1024
#define handle_error(msg) \
       do { perror(msg); exit(EXIT_FAILURE); } while (0)
       
static int GetMountPoint(const char *path, char *buffer, const size_t bufferSize)
{
    struct stat cur_stat;
    struct stat last_stat;
    char saved_cwd[PATH_MAX+1];
    char full_path[PATH_MAX+1];
    int ret = -1;

    if (!realpath(path, full_path))
        return -1;

    if (getcwd(saved_cwd, PATH_MAX) == NULL)
        return -1;
    
    if (lstat(full_path, &cur_stat) < 0)
        return -1;

    if (!S_ISDIR(cur_stat.st_mode))
    {
        /* path is a file */
        char *ptr = strrchr(full_path, '/');
        if (!ptr)
            return -1;
        ptr[1] = '\0';
    }
    
    if (chdir(full_path) < 0)
        return -1;
    
    if (lstat(".", &last_stat) < 0)
        goto finish;

    for (;;) 
    {
        if (lstat("..", &cur_stat) < 0)
            goto finish;
        
        if (cur_stat.st_dev != last_stat.st_dev || 
            cur_stat.st_ino == last_stat.st_ino)
            break; /* this is the mount point */
        
        if (chdir("..") < 0)
            goto finish;
        
        last_stat = cur_stat;
    }
    
    if (getcwd(buffer, bufferSize))
        ret = 0;

finish:
    if (chdir(saved_cwd) < 0)
        return -1;

    return ret;
}

static char GetItemType(const char *pPath, int resolveLink, off_t *pFileSize)
{
    struct stat st = {0};
    char iType = '\0';
    if(0 == pPath)
    {
        return '\0';
    }
    if(resolveLink)
    {
        if(-1 == stat(pPath, &st))
        {
            return '\0';
        }
    }
    else
    {
        if(-1 == lstat(pPath, &st))
        {
            return '\0';
        }
    }
    *pFileSize = st.st_size;
    if( S_ISREG(st.st_mode) )
    {
        iType = 'r';
    }
    else if( S_ISDIR(st.st_mode) )
    {
        iType = 'd';
    }
    else if( S_ISFIFO(st.st_mode) )
    {
        iType = 'f';
    }
    else if( S_ISSOCK(st.st_mode) )
    {
        iType = 's';
    }
    else if( S_ISLNK(st.st_mode) )
    {
        iType = 'l';
    }
    else if( S_ISBLK(st.st_mode) )
    {
        iType = 'b';
    }
    else if( S_ISCHR(st.st_mode) )
    {
        iType = 'c';
    }
    return iType;
}

static int MatchWildcards(const char *name, char *wildcards, const unsigned int wildcardsLen)
{
    if (NULL == name || NULL == wildcards)
    {
        return 0;
    }
    const char *end = wildcards + wildcardsLen;
    char *pattern = wildcards;
    while(pattern < end && '\0' != (*pattern))
    {
        if(0 == fnmatch(pattern, name, 0))
        {
            return 0;
        }
        pattern += strlen(pattern) + 1;
    }
    return 1;
}

int lsdir_main(int argc, char **argv)
{
    struct dirent *pDir = 0;
    int bPos = 0;
    char dType = '\0';
    char iType = '\0';
    char lType = '\0';
    char *mainDir = malloc((1 < argc) ? strlen(argv[1]) + 1 : 2);
    char *tmpName = malloc(PATH_MAX+1); 
    char *mountPoint = malloc(PATH_MAX+1); 
    char listTypes[9] = "rdfslbc";
    char listLinkTypes[9] = "rdfsbc";
    
    unsigned int iStart  = 0;
    unsigned int iEnd    = (unsigned int)(-1);
    unsigned int iCurr   = 0;
    char * fWildcards = NULL;
    char * dWildcards = NULL;
    unsigned int fWildcardsLen = 0;
    unsigned int dWildcardsLen = 0;
    unsigned int bWithSize = 0;
    
    if(!mainDir || !tmpName)
    {
        handle_error("malloc");
    }
    
    if(1 < argc)
    {
        strcpy(mainDir, argv[1]);
    }
    else
    {
        strcpy(mainDir, ".");
    }
    
    if(2 < argc)
    {
        strncpy(listTypes, argv[2], sizeof(listTypes) - 1);
    }
    if(3 < argc)
    {
        strncpy(listLinkTypes, argv[3], sizeof(listLinkTypes) - 1);
    }
    if(4 < argc)
    {
        sscanf(argv[4], "%u", &iStart);
    }
    if(5 < argc)
    {
        sscanf(argv[5], "%u", &iEnd);
    }
    if(6 < argc)
    {
        fWildcardsLen = strlen(argv[6]);
        fWildcards = malloc(fWildcardsLen + 3);
        memset(fWildcards, '\0', dWildcardsLen + 3);
        strcpy(fWildcards, argv[6]);
        printf("%s\n", argv[6]);
        char *p = strtok(fWildcards, "|");
        while( (p = strtok(NULL, "|")) );
    }
    if(7 < argc)
    {
        dWildcardsLen = strlen(argv[7]);
        dWildcards = malloc(dWildcardsLen + 3);
        memset(dWildcards, '\0', dWildcardsLen + 3);
        strcpy(dWildcards , argv[7]);
        char *p = strtok(dWildcards, "|");
        while( (p = strtok(NULL, "|")) );
    }
    if(8 < argc)
    {
        sscanf(argv[8], "%u", &bWithSize);
    }
    
    DIR *dirp = opendir(mainDir);
    if(0 == dirp)
    {
       handle_error("opendir");
    }
    
    if (0 == GetMountPoint(mainDir, mountPoint, PATH_MAX))
    {
        fprintf(stderr, "%s\n", mountPoint);
    }
    else
    {
        fprintf(stderr, "\n");
    }

    while(iCurr < iEnd)
    {
        pDir = readdir(dirp);
        if(0 == pDir)
        {
           break;
        }
        
        {
            dType = pDir->d_type;
            switch(dType)
            {
                case DT_REG:
                    iType = 'r';
                break;
                case DT_DIR:
                    iType = 'd';
                break;
                case DT_FIFO:
                    iType = 'f';
                break;
                case DT_SOCK:
                    iType = 's';
                break;
                case DT_LNK:
                    iType = 'l';
                break;
                case DT_BLK:
                    iType = 'b';
                break;
                case DT_CHR:
                    iType = 'c';
                break;
                default:
                    iType = '\0';
                break;
            }
            
            off_t totalFileSizeInBytes = -1;
            snprintf(tmpName, PATH_MAX, "%s/%s", mainDir, pDir->d_name);
            if('\0' == iType || ('l' != iType && bWithSize))
            {
                iType = GetItemType(tmpName, 0, &totalFileSizeInBytes);
            }
            
            if('l' == iType )
            {
                lType = GetItemType(tmpName, 1, &totalFileSizeInBytes);
            }

            if('\0' != iType && 0 != strchr(listTypes, iType) )
            {
                if('l' == iType)
                {
                    if('\0' != lType && 0 != strchr(listLinkTypes, lType)  && 
                       0 == MatchWildcards(pDir->d_name, ('d' == lType) ? dWildcards : fWildcards, ('d' == lType) ? dWildcardsLen : fWildcardsLen))
                    {
                        if(iCurr >= iStart)
                        {
                            fprintf(stderr, "%s//%c//1//", pDir->d_name, lType);
                            if(bWithSize)
                            {
                                fprintf(stderr, "%lld//", (long long) totalFileSizeInBytes);
                            }
                            fprintf(stderr, "\n");
                        }
                        ++iCurr;
                    }
                }
                else if(0 == MatchWildcards(pDir->d_name, ('d' == iType) ? dWildcards : fWildcards, ('d' == iType) ? dWildcardsLen : fWildcardsLen))
                {
                    if(iCurr >= iStart)
                    {
                        fprintf(stderr, "%s//%c//0//", pDir->d_name, iType);
                        if(bWithSize)
                        {
                            fprintf(stderr, "%lld//", (long long) totalFileSizeInBytes);
                        }
                        fprintf(stderr, "\n");
                    }
                    ++iCurr;
                }
            }
            bPos += pDir->d_reclen;
            
            if(iCurr >= iEnd)
            {
                break;
            }
        }
    }
    //close(fd);
    closedir(dirp);
    if(NULL != fWildcards)
    {
        free(fWildcards);
    }
    if(NULL != dWildcards)
    {
        free(dWildcards);
    }
    free(mainDir);
    free(tmpName);
    exit(EXIT_SUCCESS);
}