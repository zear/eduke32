// "Build Engine & Tools" Copyright (c) 1993-1997 Ken Silverman
// Ken Silverman's official web site: "http://www.advsys.net/ken"
// See the included license file "BUILDLIC.TXT" for license info.
//
// This file has been modified from Ken Silverman's original release
// by Jonathon Fowler (jonof@edgenetwk.com)

#include "compat.h"
#include "cache1d.h"
#include "pragmas.h"
#include "baselayer.h"

#ifdef WITHKPLIB
#include "kplib.h"

//Insert '|' in front of filename
//Doing this tells kzopen to load the file only if inside a .ZIP file
static intptr_t kzipopen(const char *filnam)
{
    uint32_t i;
    char newst[BMAX_PATH+8];

    newst[0] = '|';
    for (i=0; filnam[i] && (i < sizeof(newst)-2); i++) newst[i+1] = filnam[i];
    newst[i+1] = 0;
    return(kzopen(newst));
}

#endif


//   This module keeps track of a standard linear cacheing system.
//   To use this module, here's all you need to do:
//
//   Step 1: Allocate a nice BIG buffer, like from 1MB-4MB and
//           Call initcache(int32_t cachestart, int32_t cachesize) where
//
//              cachestart = (intptr_t)(pointer to start of BIG buffer)
//              cachesize = length of BIG buffer
//
//   Step 2: Call allocache(intptr_t *bufptr, int32_t bufsiz, char *lockptr)
//              whenever you need to allocate a buffer, where:
//
//              *bufptr = pointer to multi-byte pointer to buffer
//                 Confused?  Using this method, cache2d can remove
//                 previously allocated things from the cache safely by
//                 setting the multi-byte pointer to 0.
//              bufsiz = number of bytes to allocate
//              *lockptr = pointer to locking char which tells whether
//                 the region can be removed or not.  If *lockptr = 0 then
//                 the region is not locked else its locked.
//
//   Step 3: If you need to remove everything from the cache, or every
//           unlocked item from the cache, you can call uninitcache();
//              Call uninitcache(0) to remove all unlocked items, or
//              Call uninitcache(1) to remove everything.
//           After calling uninitcache, it is still ok to call allocache
//           without first calling initcache.

#define MAXCACHEOBJECTS 9216

static int32_t cachesize = 0;
int32_t cachecount = 0;
char zerochar = 0;
intptr_t cachestart = 0;
int32_t cacnum = 0, agecount = 0;
cactype cac[MAXCACHEOBJECTS];
static int32_t lockrecip[200];

static char toupperlookup[256];

static void reportandexit(char *errormessage);

extern char pow2char[8];


void initcache(intptr_t dacachestart, int32_t dacachesize)
{
    int32_t i;

    for (i=1; i<200; i++) lockrecip[i] = (1<<28)/(200-i);

    // The following code was relocated here from engine.c, since this
    // function is only ever called once (from there), and it seems to
    // really belong here:
    //
    //   initcache((FP_OFF(pic)+15)&0xfffffff0,(cachesize-((-FP_OFF(pic))&15))&0xfffffff0);
    //
    // I'm not sure why it's necessary, but the code is making sure the
    // cache starts on a multiple of 16 bytes?  -- SA

//printf("BEFORE: cachestart = %x, cachesize = %d\n", dacachestart, dacachesize);
    cachestart = ((uintptr_t)dacachestart+15)&~(uintptr_t)0xf;
    cachesize = (dacachesize-(((uintptr_t)(dacachestart))&0xf))&~(uintptr_t)0xf;
//printf("AFTER : cachestart = %x, cachesize = %d\n", cachestart, cachesize);

    cac[0].leng = cachesize;
    cac[0].lock = &zerochar;
    cacnum = 1;

    initprintf("Initialized %.1fM cache\n", (float)(dacachesize/1024.f/1024.f));
}

void allocache(intptr_t *newhandle, int32_t newbytes, char *newlockptr)
{
    int32_t i, /*j,*/ z, zz, bestz=0, daval, bestval, besto=0, o1, o2, sucklen, suckz;

//printf("  ==> asking for %d bytes, ", newbytes);
    // Make all requests a multiple of 16 bytes
    newbytes = (newbytes+15)&0xfffffff0;
//printf("allocated %d bytes\n", newbytes);

    if ((unsigned)newbytes > (unsigned)cachesize)
    {
        Bprintf("Cachesize: %d\n",cachesize);
        Bprintf("*Newhandle: 0x%" PRIxPTR ", Newbytes: %d, *Newlock: %d\n",(intptr_t)newhandle,newbytes,*newlockptr);
        reportandexit("BUFFER TOO BIG TO FIT IN CACHE!");
    }

    if (*newlockptr == 0)
    {
        reportandexit("ALLOCACHE CALLED WITH LOCK OF 0!");
    }

    //Find best place
    bestval = 0x7fffffff; o1 = cachesize;
    for (z=cacnum-1; z>=0; z--)
    {
        o1 -= cac[z].leng;
        o2 = o1+newbytes; if (o2 > cachesize) continue;

        daval = 0;
        for (i=o1,zz=z; i<o2; i+=cac[zz++].leng)
        {
            if (*cac[zz].lock == 0) continue;
            if (*cac[zz].lock >= 200) { daval = 0x7fffffff; break; }
            daval += mulscale32(cac[zz].leng+65536,lockrecip[*cac[zz].lock]);
            if (daval >= bestval) break;
        }
        if (daval < bestval)
        {
            bestval = daval; besto = o1; bestz = z;
            if (bestval == 0) break;
        }
    }

    //printf("%d %d %d\n",besto,newbytes,*newlockptr);

    if (bestval == 0x7fffffff)
        reportandexit("CACHE SPACE ALL LOCKED UP!");

    //Suck things out
    for (sucklen=-newbytes,suckz=bestz; sucklen<0; sucklen+=cac[suckz++].leng)
        if (*cac[suckz].lock) *cac[suckz].hand = 0;

    //Remove all blocks except 1
    suckz -= (bestz+1); cacnum -= suckz;
    copybufbyte(&cac[bestz+suckz],&cac[bestz],(cacnum-bestz)*sizeof(cactype));
    cac[bestz].hand = newhandle; *newhandle = cachestart+(intptr_t)besto;
    cac[bestz].leng = newbytes;
    cac[bestz].lock = newlockptr;
    cachecount++;

    //Add new empty block if necessary
    if (sucklen <= 0) return;

    bestz++;
    if (bestz == cacnum)
    {
        cacnum++; if (cacnum > MAXCACHEOBJECTS) reportandexit("Too many objects in cache! (cacnum > MAXCACHEOBJECTS)");
        cac[bestz].leng = sucklen;
        cac[bestz].lock = &zerochar;
        return;
    }

    if (*cac[bestz].lock == 0) { cac[bestz].leng += sucklen; return; }

    cacnum++; if (cacnum > MAXCACHEOBJECTS) reportandexit("Too many objects in cache! (cacnum > MAXCACHEOBJECTS)");
    for (z=cacnum-1; z>bestz; z--) cac[z] = cac[z-1];
    cac[bestz].leng = sucklen;
    cac[bestz].lock = &zerochar;
}

void suckcache(intptr_t *suckptr)
{
    int32_t i;

    //Can't exit early, because invalid pointer might be same even though lock = 0
    for (i=0; i<cacnum; i++)
    {
        if ((intptr_t)(*cac[i].hand) == (intptr_t)suckptr)
        {
            if (*cac[i].lock) *cac[i].hand = 0;
            cac[i].lock = &zerochar;
            cac[i].hand = 0;

            //Combine empty blocks
            if ((i > 0) && (*cac[i-1].lock == 0))
            {
                cac[i-1].leng += cac[i].leng;
                cacnum--; copybuf(&cac[i+1],&cac[i],(cacnum-i)*sizeof(cactype));
            }
            else if ((i < cacnum-1) && (*cac[i+1].lock == 0))
            {
                cac[i+1].leng += cac[i].leng;
                cacnum--; copybuf(&cac[i+1],&cac[i],(cacnum-i)*sizeof(cactype));
            }
        }
    }
}

void agecache(void)
{
    int32_t cnt = (cacnum>>4);

    if (agecount >= cacnum) agecount = cacnum-1;
    if (agecount < 0 || !cnt) return;

    for (; cnt>=0; cnt--)
    {
        if (cac[agecount].lock && (((*cac[agecount].lock)-2)&255) < 198)
            (*cac[agecount].lock)--;

        agecount--;
        if (agecount < 0) agecount = cacnum-1;
    }
}

static void reportandexit(char *errormessage)
{
    int32_t i, j;

    //setvmode(0x3);
    j = 0;
    for (i=0; i<cacnum; i++)
    {
        Bprintf("%d- ",i);
        if (cac[i].hand) Bprintf("ptr: 0x%" PRIxPTR ", ",*cac[i].hand);
        else Bprintf("ptr: NULL, ");
        Bprintf("leng: %d, ",cac[i].leng);
        if (cac[i].lock) Bprintf("lock: %d\n",*cac[i].lock);
        else Bprintf("lock: NULL\n");
        j += cac[i].leng;
    }
    Bprintf("Cachesize = %d\n",cachesize);
    Bprintf("Cacnum = %d\n",cacnum);
    Bprintf("Cache length sum = %d\n",j);
    initprintf("ERROR: %s\n",errormessage);
    exit(0);
}

#include <errno.h>

typedef struct _searchpath
{
    struct _searchpath *next;
    char *path;
    size_t pathlen;		// to save repeated calls to strlen()
} searchpath_t;
static searchpath_t *searchpathhead = NULL;
static size_t maxsearchpathlen = 0;
int32_t pathsearchmode = 0;

int32_t addsearchpath(const char *p)
{
    struct stat st;
    char *s;
    searchpath_t *srch;

    if (Bstat(p, &st) < 0)
    {
        if (errno == ENOENT) return -2;
        return -1;
    }
    if (!(st.st_mode & BS_IFDIR)) return -1;

    srch = (searchpath_t*)Bmalloc(sizeof(searchpath_t));
    if (!srch) return -1;

    srch->next    = searchpathhead;
    srch->pathlen = strlen(p)+1;
    srch->path    = (char*)Bmalloc(srch->pathlen + 1);
    if (!srch->path)
    {
        Bfree(srch);
        return -1;
    }
    strcpy(srch->path, p);
    for (s=srch->path; *s; s++) ; s--; if (s<srch->path || toupperlookup[*s] != '/') strcat(srch->path, "/");

    searchpathhead = srch;
    if (srch->pathlen > maxsearchpathlen) maxsearchpathlen = srch->pathlen;

    Bcorrectfilename(srch->path,0);

    initprintf("Using %s for game data\n", srch->path);

    return 0;
}

int32_t findfrompath(const char *fn, char **where)
{
    searchpath_t *sp;
    char *pfn, *ffn;
    int32_t allocsiz;

    // pathsearchmode == 0: tests current dir and then the dirs of the path stack
    // pathsearchmode == 1: tests fn without modification, then like for pathsearchmode == 0

    if (pathsearchmode)
    {
        // test unmolested filename first
        if (access(fn, F_OK) >= 0)
        {
            *where = Bstrdup(fn);
            return 0;
        }
        else
        {
            char *tfn = Bstrtolower(Bstrdup(fn));

            if (access(tfn, F_OK) >= 0)
            {
                *where = tfn;
                return 0;
            }

            Bstrupr(tfn);

            if (access(tfn, F_OK) >= 0)
            {
                *where = tfn;
                return 0;
            }

            Bfree(tfn);
        }
    }

    for (pfn = (char*)fn; toupperlookup[*pfn] == '/'; pfn++);
    ffn = Bstrdup(pfn);
    if (!ffn) return -1;
    Bcorrectfilename(ffn,0);	// compress relative paths

    allocsiz = max(maxsearchpathlen, 2);	// "./" (aka. curdir)
    allocsiz += strlen(ffn);
    allocsiz += 1;	// a nul

    pfn = (char *)Bmalloc(allocsiz);
    if (!pfn) { Bfree(ffn); return -1; }

    strcpy(pfn, "./");
    strcat(pfn, ffn);
    if (access(pfn, F_OK) >= 0)
    {
        *where = pfn;
        Bfree(ffn);
        return 0;
    }

    for (sp = searchpathhead; sp; sp = sp->next)
    {
        char *tfn = Bstrdup(ffn);

        strcpy(pfn, sp->path);
        strcat(pfn, ffn);
        //initprintf("Trying %s\n", pfn);
        if (access(pfn, F_OK) >= 0)
        {
            *where = pfn;
            Bfree(ffn);
            Bfree(tfn);
            return 0;
        }

        //Check with all lowercase
        strcpy(pfn, sp->path);
        Bstrtolower(tfn);
        strcat(pfn, tfn);
        if (access(pfn, F_OK) >= 0)
        {
            *where = pfn;
            Bfree(ffn);
            Bfree(tfn);
            return 0;
        }

        //Check again with uppercase
        strcpy(pfn, sp->path);
        Bstrupr(tfn);
        strcat(pfn, tfn);
        if (access(pfn, F_OK) >= 0)
        {
            *where = pfn;
            Bfree(ffn);
            Bfree(tfn);
            return 0;
        }

        Bfree(tfn);
    }
    Bfree(pfn); Bfree(ffn);
    return -1;
}

int32_t openfrompath(const char *fn, int32_t flags, int32_t mode)
{
    char *pfn;
    int32_t h;

    if (findfrompath(fn, &pfn) < 0) return -1;
    h = Bopen(pfn, flags, mode);
    Bfree(pfn);

    return h;
}

BFILE* fopenfrompath(const char *fn, const char *mode)
{
    int32_t fh;
    BFILE *h;
    int32_t bmode = 0, smode = 0;
    const char *c;

    for (c=mode; c[0];)
    {
        if (c[0] == 'r' && c[1] == '+') { bmode = BO_RDWR; smode = BS_IREAD|BS_IWRITE; c+=2; }
        else if (c[0] == 'r')                { bmode = BO_RDONLY; smode = BS_IREAD; c+=1; }
        else if (c[0] == 'w' && c[1] == '+') { bmode = BO_RDWR|BO_CREAT|BO_TRUNC; smode = BS_IREAD|BS_IWRITE; c+=2; }
        else if (c[0] == 'w')                { bmode = BO_WRONLY|BO_CREAT|BO_TRUNC; smode = BS_IREAD|BS_IWRITE; c+=2; }
        else if (c[0] == 'a' && c[1] == '+') { bmode = BO_RDWR|BO_CREAT; smode=BS_IREAD|BS_IWRITE; c+=2; }
        else if (c[0] == 'a')                { bmode = BO_WRONLY|BO_CREAT; smode=BS_IREAD|BS_IWRITE; c+=1; }
        else if (c[0] == 'b')                { bmode |= BO_BINARY; c+=1; }
        else if (c[1] == 't')                { bmode |= BO_TEXT; c+=1; }
        else c++;
    }
    fh = openfrompath(fn,bmode,smode);
    if (fh < 0) return NULL;

    h = fdopen(fh,mode);
    if (!h) close(fh);

    return h;
}

static char toupperlookup[256] =
{
    0x00,0x01,0x02,0x03,0x04,0x05,0x06,0x07,0x08,0x09,0x0a,0x0b,0x0c,0x0d,0x0e,0x0f,
    0x10,0x11,0x12,0x13,0x14,0x15,0x16,0x17,0x18,0x19,0x1a,0x1b,0x1c,0x1d,0x1e,0x1f,
    0x20,0x21,0x22,0x23,0x24,0x25,0x26,0x27,0x28,0x29,0x2a,0x2b,0x2c,0x2d,0x2e,0x2f,
    0x30,0x31,0x32,0x33,0x34,0x35,0x36,0x37,0x38,0x39,0x3a,0x3b,0x3c,0x3d,0x3e,0x3f,
    0x40,0x41,0x42,0x43,0x44,0x45,0x46,0x47,0x48,0x49,0x4a,0x4b,0x4c,0x4d,0x4e,0x4f,
    0x50,0x51,0x52,0x53,0x54,0x55,0x56,0x57,0x58,0x59,0x5a,0x5b,0x5c,0x5d,0x5e,0x5f,
    0x60,0x41,0x42,0x43,0x44,0x45,0x46,0x47,0x48,0x49,0x4a,0x4b,0x4c,0x4d,0x4e,0x4f,
    0x50,0x51,0x52,0x53,0x54,0x55,0x56,0x57,0x58,0x59,0x5a,0x7b,0x7c,0x7d,0x7e,0x7f,
    0x80,0x81,0x82,0x83,0x84,0x85,0x86,0x87,0x88,0x89,0x8a,0x8b,0x8c,0x8d,0x8e,0x8f,
    0x90,0x91,0x92,0x93,0x94,0x95,0x96,0x97,0x98,0x99,0x9a,0x9b,0x9c,0x9d,0x9e,0x9f,
    0xa0,0xa1,0xa2,0xa3,0xa4,0xa5,0xa6,0xa7,0xa8,0xa9,0xaa,0xab,0xac,0xad,0xae,0xaf,
    0xb0,0xb1,0xb2,0xb3,0xb4,0xb5,0xb6,0xb7,0xb8,0xb9,0xba,0xbb,0xbc,0xbd,0xbe,0xbf,
    0xc0,0xc1,0xc2,0xc3,0xc4,0xc5,0xc6,0xc7,0xc8,0xc9,0xca,0xcb,0xcc,0xcd,0xce,0xcf,
    0xd0,0xd1,0xd2,0xd3,0xd4,0xd5,0xd6,0xd7,0xd8,0xd9,0xda,0xdb,0xdc,0xdd,0xde,0xdf,
    0xe0,0xe1,0xe2,0xe3,0xe4,0xe5,0xe6,0xe7,0xe8,0xe9,0xea,0xeb,0xec,0xed,0xee,0xef,
    0xf0,0xf1,0xf2,0xf3,0xf4,0xf5,0xf6,0xf7,0xf8,0xf9,0xfa,0xfb,0xfc,0xfd,0xfe,0xff
};

int32_t numgroupfiles = 0;
static int32_t gnumfiles[MAXGROUPFILES];
static int32_t groupfil[MAXGROUPFILES] = {-1,-1,-1,-1,-1,-1,-1,-1};
static int32_t groupfilpos[MAXGROUPFILES];
static char *gfilelist[MAXGROUPFILES];
static int32_t *gfileoffs[MAXGROUPFILES];

char filegrp[MAXOPENFILES];
static int32_t filepos[MAXOPENFILES];
static intptr_t filehan[MAXOPENFILES] =
{
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1
};
#ifdef WITHKPLIB
static char filenamsav[MAXOPENFILES][260];
static int32_t kzcurhand = -1;
#endif

int32_t initgroupfile(const char *filename)
{
    char buf[16];
    int32_t i, j, k;
#ifdef WITHKPLIB
    char *zfn;
#endif

#ifdef _WIN32
    // on Windows, translate all backslashes (0x5c) to forward slashes (0x2f)
    toupperlookup[0x5c] = 0x2f;
#endif

#ifdef WITHKPLIB
    if (findfrompath(filename, &zfn) < 0) return -1;

    // check to see if the file passed is a ZIP and pass it on to kplib if it is
    i = Bopen(zfn,BO_BINARY|BO_RDONLY,BS_IREAD);
    if (i < 0) { Bfree(zfn); return -1; }

    Bread(i, buf, 4);
    if (buf[0] == 0x50 && buf[1] == 0x4B && buf[2] == 0x03 && buf[3] == 0x04)
    {
        Bclose(i);
        j = kzaddstack(zfn);
        Bfree(zfn);
        return j;
    }
    Bfree(zfn);

    if (numgroupfiles >= MAXGROUPFILES) return(-1);

    Blseek(i,0,BSEEK_SET);
    groupfil[numgroupfiles] = i;
#else
    groupfil[numgroupfiles] = openfrompath(filename,BO_BINARY|BO_RDONLY,BS_IREAD);
    if (groupfil[numgroupfiles] != -1)
#endif
    {
        groupfilpos[numgroupfiles] = 0;
        Bread(groupfil[numgroupfiles],buf,16);
        if ((buf[0] != 'K') || (buf[1] != 'e') || (buf[2] != 'n') ||
                (buf[3] != 'S') || (buf[4] != 'i') || (buf[5] != 'l') ||
                (buf[6] != 'v') || (buf[7] != 'e') || (buf[8] != 'r') ||
                (buf[9] != 'm') || (buf[10] != 'a') || (buf[11] != 'n'))
        {
            Bclose(groupfil[numgroupfiles]);
            groupfil[numgroupfiles] = -1;
            return(-1);
        }
        gnumfiles[numgroupfiles] = B_LITTLE32(*((int32_t *)&buf[12]));

        if ((gfilelist[numgroupfiles] = (char *)Bmalloc(gnumfiles[numgroupfiles]<<4)) == 0)
            { Bprintf("Not enough memory for file grouping system\n"); exit(0); }
        if ((gfileoffs[numgroupfiles] = (int32_t *)Bmalloc((gnumfiles[numgroupfiles]+1)<<2)) == 0)
            { Bprintf("Not enough memory for file grouping system\n"); exit(0); }

        Bread(groupfil[numgroupfiles],gfilelist[numgroupfiles],gnumfiles[numgroupfiles]<<4);

        j = 0;
        for (i=0; i<gnumfiles[numgroupfiles]; i++)
        {
            k = B_LITTLE32(*((int32_t *)&gfilelist[numgroupfiles][(i<<4)+12]));
            gfilelist[numgroupfiles][(i<<4)+12] = 0;
            gfileoffs[numgroupfiles][i] = j;
            j += k;
        }
        gfileoffs[numgroupfiles][gnumfiles[numgroupfiles]] = j;
    }
    numgroupfiles++;
    return(groupfil[numgroupfiles-1]);
}

void uninitsinglegroupfile(int32_t grphandle)
{
    int32_t i, grpnum = -1;

    for (i=numgroupfiles-1; i>=0; i--)
        if (groupfil[i] != -1 && groupfil[i] == grphandle)
        {
            Bfree(gfilelist[i]);
            Bfree(gfileoffs[i]);
            Bclose(groupfil[i]);
            groupfil[i] = -1;
            grpnum = i;
            break;
        }

    if (grpnum == -1) return;

    // JBF 20040111
    numgroupfiles--;

    // move any group files following this one back
    for (i=grpnum+1; i<MAXGROUPFILES; i++)
        if (groupfil[i] != -1)
        {
            groupfil[i-1]    = groupfil[i];
            gnumfiles[i-1]   = gnumfiles[i];
            groupfilpos[i-1] = groupfilpos[i];
            gfilelist[i-1]   = gfilelist[i];
            gfileoffs[i-1]   = gfileoffs[i];
            groupfil[i] = -1;
        }

    // fix up the open files that need attention
    for (i=0; i<MAXOPENFILES; i++)
    {
        if (filegrp[i] >= 254)         // external file (255) or ZIPped file (254)
            continue;
        else if (filegrp[i] == grpnum)   // close file in group we closed
            filehan[i] = -1;
        else if (filegrp[i] > grpnum)   // move back a file in a group after the one we closed
            filegrp[i]--;
    }
}

void uninitgroupfile(void)
{
    int32_t i;

    for (i=numgroupfiles-1; i>=0; i--)
        if (groupfil[i] != -1)
        {
            Bfree(gfilelist[i]);
            Bfree(gfileoffs[i]);
            Bclose(groupfil[i]);
            groupfil[i] = -1;
        }
    numgroupfiles = 0;

    // JBF 20040111: "close" any files open in groups
    for (i=0; i<MAXOPENFILES; i++)
    {
        if (filegrp[i] < 254)   // JBF 20040130: not external or ZIPped
            filehan[i] = -1;
    }
}

int32_t kopen4load(const char *filename, char searchfirst)
{
    int32_t  j, k, fil, newhandle = MAXOPENFILES-1;
    char bad, *gfileptr;
    intptr_t i;

    while (filehan[newhandle] != -1)
    {
        newhandle--;
        if (newhandle < 0)
        {
            Bprintf("TOO MANY FILES OPEN IN FILE GROUPING SYSTEM!");
            exit(0);
        }
    }

    if (searchfirst == 0 && (fil = openfrompath(filename,BO_BINARY|BO_RDONLY,S_IREAD)) >= 0)
    {
        filegrp[newhandle] = 255;
        filehan[newhandle] = fil;
        filepos[newhandle] = 0;
        return(newhandle);
    }

    for (; toupperlookup[*filename] == '/'; filename++);

#ifdef WITHKPLIB
    if ((kzcurhand != newhandle) && (kztell() >= 0))
    {
        if (kzcurhand >= 0) filepos[kzcurhand] = kztell();
        kzclose();
    }
    if (searchfirst != 1 && (i = kzipopen(filename)) != 0)
    {
        kzcurhand = newhandle;
        filegrp[newhandle] = 254;
        filehan[newhandle] = i;
        filepos[newhandle] = 0;
        strcpy(filenamsav[newhandle],filename);
        return newhandle;
    }
#endif

    for (k=numgroupfiles-1; k>=0; k--)
    {
        if (searchfirst == 1) k = 0;
        if (groupfil[k] >= 0)
        {
            for (i=gnumfiles[k]-1; i>=0; i--)
            {
                gfileptr = (char *)&gfilelist[k][i<<4];

                bad = 0;
                for (j=0; j<13; j++)
                {
                    if (!filename[j]) break;
                    if (toupperlookup[filename[j]] != toupperlookup[gfileptr[j]])
                        { bad = 1; break; }
                }
                if (bad) continue;
                if (j<13 && gfileptr[j]) continue;   // JBF: because e1l1.map might exist before e1l1
                if (j==13 && filename[j]) continue;   // JBF: long file name

                filegrp[newhandle] = k;
                filehan[newhandle] = i;
                filepos[newhandle] = 0;
                return(newhandle);
            }
        }
    }
    return(-1);
}

int32_t kread(int32_t handle, void *buffer, int32_t leng)
{
    int32_t i;
    int32_t filenum = filehan[handle];
    int32_t groupnum = filegrp[handle];

    if (groupnum == 255) return(Bread(filenum,buffer,leng));
#ifdef WITHKPLIB
    else if (groupnum == 254)
    {
        if (kzcurhand != handle)
        {
            if (kztell() >= 0) { filepos[kzcurhand] = kztell(); kzclose(); }
            kzcurhand = handle;
            kzipopen(filenamsav[handle]);
            kzseek(filepos[handle],SEEK_SET);
        }
        return(kzread(buffer,leng));
    }
#endif

    if (groupfil[groupnum] != -1)
    {
        i = gfileoffs[groupnum][filenum]+filepos[handle];
        if (i != groupfilpos[groupnum])
        {
            Blseek(groupfil[groupnum],i+((gnumfiles[groupnum]+1)<<4),BSEEK_SET);
            groupfilpos[groupnum] = i;
        }
        leng = min(leng,(gfileoffs[groupnum][filenum+1]-gfileoffs[groupnum][filenum])-filepos[handle]);
        leng = Bread(groupfil[groupnum],buffer,leng);
        filepos[handle] += leng;
        groupfilpos[groupnum] += leng;
        return(leng);
    }

    return(0);
}

int32_t klseek(int32_t handle, int32_t offset, int32_t whence)
{
    int32_t i, groupnum;

    groupnum = filegrp[handle];

    if (groupnum == 255) return(Blseek(filehan[handle],offset,whence));
#ifdef WITHKPLIB
    else if (groupnum == 254)
    {
        if (kzcurhand != handle)
        {
            if (kztell() >= 0) { filepos[kzcurhand] = kztell(); kzclose(); }
            kzcurhand = handle;
            kzipopen(filenamsav[handle]);
            kzseek(filepos[handle],SEEK_SET);
        }
        return(kzseek(offset,whence));
    }
#endif

    if (groupfil[groupnum] != -1)
    {
        switch (whence)
        {
        case BSEEK_SET:
            filepos[handle] = offset; break;
        case BSEEK_END:
            i = filehan[handle];
            filepos[handle] = (gfileoffs[groupnum][i+1]-gfileoffs[groupnum][i])+offset;
            break;
        case BSEEK_CUR:
            filepos[handle] += offset; break;
        }
        return(filepos[handle]);
    }
    return(-1);
}

int32_t kfilelength(int32_t handle)
{
    int32_t i, groupnum;

    groupnum = filegrp[handle];
    if (groupnum == 255)
    {
        // return(filelength(filehan[handle]))
        return Bfilelength(filehan[handle]);
    }
#ifdef WITHKPLIB
    else if (groupnum == 254)
    {
        if (kzcurhand != handle)
        {
            if (kztell() >= 0) { filepos[kzcurhand] = kztell(); kzclose(); }
            kzcurhand = handle;
            kzipopen(filenamsav[handle]);
            kzseek(filepos[handle],SEEK_SET);
        }
        return kzfilelength();
    }
#endif
    i = filehan[handle];
    return(gfileoffs[groupnum][i+1]-gfileoffs[groupnum][i]);
}

int32_t ktell(int32_t handle)
{
    int32_t groupnum = filegrp[handle];

    if (groupnum == 255) return(Blseek(filehan[handle],0,BSEEK_CUR));
#ifdef WITHKPLIB
    else if (groupnum == 254)
    {
        if (kzcurhand != handle)
        {
            if (kztell() >= 0) { filepos[kzcurhand] = kztell(); kzclose(); }
            kzcurhand = handle;
            kzipopen(filenamsav[handle]);
            kzseek(filepos[handle],SEEK_SET);
        }
        return kztell();
    }
#endif
    if (groupfil[groupnum] != -1)
        return filepos[handle];
    return(-1);
}

void kclose(int32_t handle)
{
    if (handle < 0) return;
    if (filegrp[handle] == 255) Bclose(filehan[handle]);
#ifdef WITHKPLIB
    else if (filegrp[handle] == 254)
    {
        kzclose();
        kzcurhand = -1;
    }
#endif
    filehan[handle] = -1;
}

static int32_t klistaddentry(CACHE1D_FIND_REC **rec, char *name, int32_t type, int32_t source)
{
    CACHE1D_FIND_REC *r = NULL, *attach = NULL;

    if (*rec)
    {
        int32_t insensitive, v;
        CACHE1D_FIND_REC *last = NULL;

        for (attach = *rec; attach; last = attach, attach = attach->next)
        {
            if (type == CACHE1D_FIND_DRIVE) continue;	// we just want to get to the end for drives
#ifdef _WIN32
            insensitive = 1;
#else
            if (source == CACHE1D_SOURCE_GRP || attach->source == CACHE1D_SOURCE_GRP)
                insensitive = 1;
            else if (source == CACHE1D_SOURCE_ZIP || attach->source == CACHE1D_SOURCE_ZIP)
                insensitive = 1;
            else
                insensitive = 0;
#endif
            if (insensitive) v = Bstrcasecmp(name, attach->name);
            else v = Bstrcmp(name, attach->name);

            // sorted list
            if (v > 0) continue;	// item to add is bigger than the current one
            // so look for something bigger than us
            if (v < 0)  			// item to add is smaller than the current one
            {
                attach = NULL;		// so wedge it between the current item and the one before
                break;
            }

            // matched
            if (source >= attach->source) return 1;	// item to add is of lower priority
            r = attach;
            break;
        }

        // wasn't found in the list, so attach to the end
        if (!attach) attach = last;
    }

    if (r)
    {
        r->type = type;
        r->source = source;
        return 0;
    }

    r = (CACHE1D_FIND_REC *)Bmalloc(sizeof(CACHE1D_FIND_REC)+strlen(name)+1);
    if (!r) return -1;
    r->name = (char*)r + sizeof(CACHE1D_FIND_REC); strcpy(r->name, name);
    r->type = type;
    r->source = source;
    r->usera = r->userb = NULL;

    if (!attach)  	// we are the first item
    {
        r->prev = NULL;
        r->next = *rec;
        if (*rec)(*rec)->prev = r;
        *rec = r;
    }
    else
    {
        r->prev = attach;
        r->next = attach->next;
        if (attach->next) attach->next->prev = r;
        attach->next = r;
    }

    return 0;
}

void klistfree(CACHE1D_FIND_REC *rec)
{
    CACHE1D_FIND_REC *n;

    while (rec)
    {
        n = rec->next;
        Bfree(rec);
        rec = n;
    }
}

CACHE1D_FIND_REC *klistpath(const char *_path, const char *mask, int32_t type)
{
    CACHE1D_FIND_REC *rec = NULL;
    char *path;

    // pathsearchmode == 0: enumerates a path in the virtual filesystem
    // pathsearchmode == 1: enumerates the system filesystem path passed in

    path = Bstrdup(_path);
    if (!path) return NULL;

    // we don't need any leading dots and slashes or trailing slashes either
    {
        int32_t i,j;
        for (i=0; path[i] == '.' || toupperlookup[path[i]] == '/';) i++;
        for (j=0; (path[j] = path[i]); j++,i++) ;
        while (j>0 && toupperlookup[path[j-1]] == '/') j--;
        path[j] = 0;
        //initprintf("Cleaned up path = \"%s\"\n",path);
    }

    if (*path && (type & CACHE1D_FIND_DIR))
    {
        if (klistaddentry(&rec, "..", CACHE1D_FIND_DIR, CACHE1D_SOURCE_CURDIR) < 0) goto failure;
    }

    if (!(type & CACHE1D_OPT_NOSTACK))  	// current directory and paths in the search stack
    {
        searchpath_t *search = NULL;
        BDIR *dir;
        struct Bdirent *dirent;
        // Adjusted for the following "autoload" dir fix - NY00123
        const char *d = "./";
        int32_t stackdepth = CACHE1D_SOURCE_CURDIR;
        char buf[BMAX_PATH];

        if (pathsearchmode) d = _path;

        do
        {
            if (!pathsearchmode)
            {
                // Fix for "autoload" dir in multi-user environments - NY00123
                strcpy(buf, d);
                strcat(buf, path);
                if (*path) strcat(buf, "/");
            }
            else strcpy(buf, d);
            dir = Bopendir(buf);
            if (dir)
            {
                while ((dirent = Breaddir(dir)))
                {
                    if ((dirent->name[0] == '.' && dirent->name[1] == 0) ||
                            (dirent->name[0] == '.' && dirent->name[1] == '.' && dirent->name[2] == 0))
                        continue;
                    if ((type & CACHE1D_FIND_DIR) && !(dirent->mode & BS_IFDIR)) continue;
                    if ((type & CACHE1D_FIND_FILE) && (dirent->mode & BS_IFDIR)) continue;
                    if (!Bwildmatch(dirent->name, mask)) continue;
                    switch (klistaddentry(&rec, dirent->name,
                                          (dirent->mode & BS_IFDIR) ? CACHE1D_FIND_DIR : CACHE1D_FIND_FILE,
                                          stackdepth))
                    {
                    case -1: goto failure;
                        //case 1: initprintf("%s:%s dropped for lower priority\n", d,dirent->name); break;
                        //case 0: initprintf("%s:%s accepted\n", d,dirent->name); break;
                    default:
                        break;
                    }
                }
                Bclosedir(dir);
            }

            if (pathsearchmode) break;

            if (!search)
            {
                search = searchpathhead;
                stackdepth = CACHE1D_SOURCE_PATH;
            }
            else
            {
                search = search->next;
                stackdepth++;
            }
            if (search) d = search->path;
        }
        while (search);
    }

    if (!pathsearchmode)  	// next, zip files
    {
        char buf[BMAX_PATH+4];
        int32_t i, j, ftype;
        strcpy(buf,path);
        if (*path) strcat(buf,"/");
        strcat(buf,mask);
        for (kzfindfilestart(buf); kzfindfile(buf);)
        {
            if (buf[0] != '|') continue;	// local files we don't need

            // scan for the end of the string and shift
            // everything left a char in the process
            for (i=1; (buf[i-1]=buf[i]); i++) ; i-=2;

            // if there's a slash at the end, this is a directory entry
            if (toupperlookup[buf[i]] == '/') { ftype = CACHE1D_FIND_DIR; buf[i] = 0; }
            else ftype = CACHE1D_FIND_FILE;

            // skip over the common characters at the beginning of the base path and the zip entry
            for (j=0; buf[j] && path[j]; j++)
            {
                if (toupperlookup[ path[j] ] == toupperlookup[ buf[j] ]) continue;
                break;
            }
            // we've now hopefully skipped the common path component at the beginning.
            // if that's true, we should be staring at a null byte in path and either any character in buf
            // if j==0, or a slash if j>0
            if ((!path[0] && buf[j]) || (!path[j] && toupperlookup[ buf[j] ] == '/'))
            {
                if (j>0) j++;

                // yep, so now we shift what follows back to the start of buf and while we do that,
                // keep an eye out for any more slashes which would mean this entry has sub-entities
                // and is useless to us.
                for (i = 0; (buf[i] = buf[j]) && toupperlookup[buf[j]] != '/'; i++,j++) ;
                if (toupperlookup[buf[j]] == '/') continue;	// damn, try next entry
            }
            else
            {
                // if we're here it means we have a situation where:
                //   path = foo
                //   buf = foobar...
                // or
                //   path = foobar
                //   buf = foo...
                // which would mean the entry is higher up in the directory tree and is also useless
                continue;
            }

            if ((type & CACHE1D_FIND_DIR) && ftype != CACHE1D_FIND_DIR) continue;
            if ((type & CACHE1D_FIND_FILE) && ftype != CACHE1D_FIND_FILE) continue;

            // the entry is in the clear
            switch (klistaddentry(&rec, buf, ftype, CACHE1D_SOURCE_ZIP))
            {
            case -1:
                goto failure;
                //case 1: initprintf("<ZIP>:%s dropped for lower priority\n", buf); break;
                //case 0: initprintf("<ZIP>:%s accepted\n", buf); break;
            default:
                break;
            }
        }
    }

    // then, grp files
    if (!pathsearchmode && !*path && (type & CACHE1D_FIND_FILE))
    {
        char buf[13];
        int32_t i,j;
        buf[12] = 0;
        for (i=0; i<MAXGROUPFILES; i++)
        {
            if (groupfil[i] == -1) continue;
            for (j=gnumfiles[i]-1; j>=0; j--)
            {
                Bmemcpy(buf,&gfilelist[i][j<<4],12);
                if (!Bwildmatch(buf,mask)) continue;
                switch (klistaddentry(&rec, buf, CACHE1D_FIND_FILE, CACHE1D_SOURCE_GRP))
                {
                case -1:
                    goto failure;
                    //case 1: initprintf("<GRP>:%s dropped for lower priority\n", workspace); break;
                    //case 0: initprintf("<GRP>:%s accepted\n", workspace); break;
                default:
                    break;
                }
            }
        }
    }

    if (pathsearchmode && (type & CACHE1D_FIND_DRIVE))
    {
        char *drives, *drp;
        drives = Bgetsystemdrives();
        if (drives)
        {
            for (drp=drives; *drp; drp+=strlen(drp)+1)
            {
                if (klistaddentry(&rec, drp, CACHE1D_FIND_DRIVE, CACHE1D_SOURCE_DRIVE) < 0)
                {
                    Bfree(drives);
                    goto failure;
                }
            }
            Bfree(drives);
        }
    }

    Bfree(path);
    return rec;
failure:
    Bfree(path);
    klistfree(rec);
    return NULL;
}

//Internal LZW variables
#define LZWSIZE 16384           //Watch out for shorts!
static char *lzwbuf1, *lzwbuf4, *lzwbuf5, lzwbuflock[5];
static int16_t *lzwbuf2, *lzwbuf3;

static int32_t lzwcompress(char *lzwinbuf, int32_t uncompleng, char *lzwoutbuf);
static int32_t lzwuncompress(char *lzwinbuf, int32_t compleng, char *lzwoutbuf);

int32_t kdfread(void *buffer, bsize_t dasizeof, bsize_t count, int32_t fil)
{
    uint32_t i, j, k, kgoal;
    int16_t leng;
    char *ptr;

    lzwbuflock[0] = lzwbuflock[1] = lzwbuflock[2] = lzwbuflock[3] = lzwbuflock[4] = 200;
    if (lzwbuf1 == NULL) allocache((intptr_t *)&lzwbuf1,LZWSIZE+(LZWSIZE>>4),&lzwbuflock[0]);
    if (lzwbuf2 == NULL) allocache((intptr_t *)&lzwbuf2,(LZWSIZE+(LZWSIZE>>4))*2,&lzwbuflock[1]);
    if (lzwbuf3 == NULL) allocache((intptr_t *)&lzwbuf3,(LZWSIZE+(LZWSIZE>>4))*2,&lzwbuflock[2]);
    if (lzwbuf4 == NULL) allocache((intptr_t *)&lzwbuf4,LZWSIZE,&lzwbuflock[3]);
    if (lzwbuf5 == NULL) allocache((intptr_t *)&lzwbuf5,LZWSIZE+(LZWSIZE>>4),&lzwbuflock[4]);

    if (dasizeof > LZWSIZE) { count *= dasizeof; dasizeof = 1; }
    ptr = (char *)buffer;

    if (kread(fil,&leng,2) != 2) return -1; leng = B_LITTLE16(leng);
    if (kread(fil,lzwbuf5,(int32_t)leng) != leng) return -1;
    k = 0; kgoal = lzwuncompress(lzwbuf5,(int32_t)leng,lzwbuf4);

    copybufbyte(lzwbuf4,ptr,(int32_t)dasizeof);
    k += (int32_t)dasizeof;

    for (i=1; i<count; i++)
    {
        if (k >= kgoal)
        {
            if (kread(fil,&leng,2) != 2) return -1; leng = B_LITTLE16(leng);
            if (kread(fil,lzwbuf5,(int32_t)leng) != leng) return -1;
            k = 0; kgoal = lzwuncompress(lzwbuf5,(int32_t)leng,lzwbuf4);
        }
        for (j=0; j<dasizeof; j++) ptr[j+dasizeof] = ((ptr[j]+lzwbuf4[j+k])&255);
        k += dasizeof;
        ptr += dasizeof;
    }
    lzwbuflock[0] = lzwbuflock[1] = lzwbuflock[2] = lzwbuflock[3] = lzwbuflock[4] = 1;
    return count;
}

int32_t dfread(void *buffer, bsize_t dasizeof, bsize_t count, BFILE *fil)
{
    uint32_t i, j, k, kgoal;
    int16_t leng;
    char *ptr;

    lzwbuflock[0] = lzwbuflock[1] = lzwbuflock[2] = lzwbuflock[3] = lzwbuflock[4] = 200;
    if (lzwbuf1 == NULL) allocache((intptr_t *)&lzwbuf1,LZWSIZE+(LZWSIZE>>4),&lzwbuflock[0]);
    if (lzwbuf2 == NULL) allocache((intptr_t *)&lzwbuf2,(LZWSIZE+(LZWSIZE>>4))*2,&lzwbuflock[1]);
    if (lzwbuf3 == NULL) allocache((intptr_t *)&lzwbuf3,(LZWSIZE+(LZWSIZE>>4))*2,&lzwbuflock[2]);
    if (lzwbuf4 == NULL) allocache((intptr_t *)&lzwbuf4,LZWSIZE,&lzwbuflock[3]);
    if (lzwbuf5 == NULL) allocache((intptr_t *)&lzwbuf5,LZWSIZE+(LZWSIZE>>4),&lzwbuflock[4]);

    if (dasizeof > LZWSIZE) { count *= dasizeof; dasizeof = 1; }
    ptr = (char *)buffer;

    if (Bfread(&leng,2,1,fil) != 1) return -1; leng = B_LITTLE16(leng);
    if (Bfread(lzwbuf5,(int32_t)leng,1,fil) != 1) return -1;
    k = 0; kgoal = lzwuncompress(lzwbuf5,(int32_t)leng,lzwbuf4);

    copybufbyte(lzwbuf4,ptr,(int32_t)dasizeof);
    k += (int32_t)dasizeof;

    for (i=1; i<count; i++)
    {
        if (k >= kgoal)
        {
            if (Bfread(&leng,2,1,fil) != 1) return -1; leng = B_LITTLE16(leng);
            if (Bfread(lzwbuf5,(int32_t)leng,1,fil) != 1) return -1;
            k = 0; kgoal = lzwuncompress(lzwbuf5,(int32_t)leng,lzwbuf4);
        }
        for (j=0; j<dasizeof; j++) ptr[j+dasizeof] = ((ptr[j]+lzwbuf4[j+k])&255);
        k += dasizeof;
        ptr += dasizeof;
    }
    lzwbuflock[0] = lzwbuflock[1] = lzwbuflock[2] = lzwbuflock[3] = lzwbuflock[4] = 1;
    return count;
}

void kdfwrite(void *buffer, bsize_t dasizeof, bsize_t count, int32_t fil)
{
    uint32_t i, j, k;
    int16_t leng, swleng;
    char *ptr;

    lzwbuflock[0] = lzwbuflock[1] = lzwbuflock[2] = lzwbuflock[3] = lzwbuflock[4] = 200;
    if (lzwbuf1 == NULL) allocache((intptr_t *)&lzwbuf1,LZWSIZE+(LZWSIZE>>4),&lzwbuflock[0]);
    if (lzwbuf2 == NULL) allocache((intptr_t *)&lzwbuf2,(LZWSIZE+(LZWSIZE>>4))*2,&lzwbuflock[1]);
    if (lzwbuf3 == NULL) allocache((intptr_t *)&lzwbuf3,(LZWSIZE+(LZWSIZE>>4))*2,&lzwbuflock[2]);
    if (lzwbuf4 == NULL) allocache((intptr_t *)&lzwbuf4,LZWSIZE,&lzwbuflock[3]);
    if (lzwbuf5 == NULL) allocache((intptr_t *)&lzwbuf5,LZWSIZE+(LZWSIZE>>4),&lzwbuflock[4]);

    if (dasizeof > LZWSIZE) { count *= dasizeof; dasizeof = 1; }
    ptr = (char *)buffer;

    copybufbyte(ptr,lzwbuf4,(int32_t)dasizeof);
    k = dasizeof;

    if (k > LZWSIZE-dasizeof)
    {
        leng = (int16_t)lzwcompress(lzwbuf4,k,lzwbuf5); k = 0; swleng = B_LITTLE16(leng);
        Bwrite(fil,&swleng,2); Bwrite(fil,lzwbuf5,(int32_t)leng);
    }

    for (i=1; i<count; i++)
    {
        for (j=0; j<dasizeof; j++) lzwbuf4[j+k] = ((ptr[j+dasizeof]-ptr[j])&255);
        k += dasizeof;
        if (k > LZWSIZE-dasizeof)
        {
            leng = (int16_t)lzwcompress(lzwbuf4,k,lzwbuf5); k = 0; swleng = B_LITTLE16(leng);
            Bwrite(fil,&swleng,2); Bwrite(fil,lzwbuf5,(int32_t)leng);
        }
        ptr += dasizeof;
    }
    if (k > 0)
    {
        leng = (int16_t)lzwcompress(lzwbuf4,k,lzwbuf5); swleng = B_LITTLE16(leng);
        Bwrite(fil,&swleng,2); Bwrite(fil,lzwbuf5,(int32_t)leng);
    }
    lzwbuflock[0] = lzwbuflock[1] = lzwbuflock[2] = lzwbuflock[3] = lzwbuflock[4] = 1;
}

void dfwrite(void *buffer, bsize_t dasizeof, bsize_t count, BFILE *fil)
{
    uint32_t i, j, k;
    int16_t leng, swleng;
    char *ptr;

    lzwbuflock[0] = lzwbuflock[1] = lzwbuflock[2] = lzwbuflock[3] = lzwbuflock[4] = 200;
    if (lzwbuf1 == NULL) allocache((intptr_t *)&lzwbuf1,LZWSIZE+(LZWSIZE>>4),&lzwbuflock[0]);
    if (lzwbuf2 == NULL) allocache((intptr_t *)&lzwbuf2,(LZWSIZE+(LZWSIZE>>4))*2,&lzwbuflock[1]);
    if (lzwbuf3 == NULL) allocache((intptr_t *)&lzwbuf3,(LZWSIZE+(LZWSIZE>>4))*2,&lzwbuflock[2]);
    if (lzwbuf4 == NULL) allocache((intptr_t *)&lzwbuf4,LZWSIZE,&lzwbuflock[3]);
    if (lzwbuf5 == NULL) allocache((intptr_t *)&lzwbuf5,LZWSIZE+(LZWSIZE>>4),&lzwbuflock[4]);

    if (dasizeof > LZWSIZE) { count *= dasizeof; dasizeof = 1; }
    ptr = (char *)buffer;

    copybufbyte(ptr,lzwbuf4,(int32_t)dasizeof);
    k = dasizeof;

    if (k > LZWSIZE-dasizeof)
    {
        leng = (int16_t)lzwcompress(lzwbuf4,k,lzwbuf5); k = 0; swleng = B_LITTLE16(leng);
        Bfwrite(&swleng,2,1,fil); Bfwrite(lzwbuf5,(int32_t)leng,1,fil);
    }

    for (i=1; i<count; i++)
    {
        for (j=0; j<dasizeof; j++) lzwbuf4[j+k] = ((ptr[j+dasizeof]-ptr[j])&255);
        k += dasizeof;
        if (k > LZWSIZE-dasizeof)
        {
            leng = (int16_t)lzwcompress(lzwbuf4,k,lzwbuf5); k = 0; swleng = B_LITTLE16(leng);
            Bfwrite(&swleng,2,1,fil); Bfwrite(lzwbuf5,(int32_t)leng,1,fil);
        }
        ptr += dasizeof;
    }
    if (k > 0)
    {
        leng = (int16_t)lzwcompress(lzwbuf4,k,lzwbuf5); swleng = B_LITTLE16(leng);
        Bfwrite(&swleng,2,1,fil); Bfwrite(lzwbuf5,(int32_t)leng,1,fil);
    }
    lzwbuflock[0] = lzwbuflock[1] = lzwbuflock[2] = lzwbuflock[3] = lzwbuflock[4] = 1;
}

static int32_t lzwcompress(char *lzwinbuf, int32_t uncompleng, char *lzwoutbuf)
{
    int32_t i, addr, newaddr, addrcnt, zx, *intptr;
    int32_t bytecnt1, bitcnt, numbits, oneupnumbits;
    int16_t *shortptr;

    for (i=255; i>=0; i--) { lzwbuf1[i] = i; lzwbuf3[i] = (i+1)&255; }
    clearbuf(lzwbuf2,256>>1,0xffffffff);
    clearbuf(lzwoutbuf,((uncompleng+15)+3)>>2,0L);

    addrcnt = 256; bytecnt1 = 0; bitcnt = (4<<3);
    numbits = 8; oneupnumbits = (1<<8);
    do
    {
        addr = lzwinbuf[bytecnt1];
        do
        {
            bytecnt1++;
            if (bytecnt1 == uncompleng) break;
            if (lzwbuf2[addr] < 0) {lzwbuf2[addr] = addrcnt; break;}
            newaddr = lzwbuf2[addr];
            while (lzwbuf1[newaddr] != lzwinbuf[bytecnt1])
            {
                zx = lzwbuf3[newaddr];
                if (zx < 0) {lzwbuf3[newaddr] = addrcnt; break;}
                newaddr = zx;
            }
            if (lzwbuf3[newaddr] == addrcnt) break;
            addr = newaddr;
        }
        while (addr >= 0);
        lzwbuf1[addrcnt] = lzwinbuf[bytecnt1];
        lzwbuf2[addrcnt] = -1;
        lzwbuf3[addrcnt] = -1;

        intptr = (int32_t *)&lzwoutbuf[bitcnt>>3];
        intptr[0] |= B_LITTLE32(addr<<(bitcnt&7));
        bitcnt += numbits;
        if ((addr&((oneupnumbits>>1)-1)) > ((addrcnt-1)&((oneupnumbits>>1)-1)))
            bitcnt--;

        addrcnt++;
        if (addrcnt > oneupnumbits) { numbits++; oneupnumbits <<= 1; }
    }
    while ((bytecnt1 < uncompleng) && (bitcnt < (uncompleng<<3)));

    intptr = (int32_t *)&lzwoutbuf[bitcnt>>3];
    intptr[0] |= B_LITTLE32(addr<<(bitcnt&7));
    bitcnt += numbits;
    if ((addr&((oneupnumbits>>1)-1)) > ((addrcnt-1)&((oneupnumbits>>1)-1)))
        bitcnt--;

    shortptr = (int16_t *)lzwoutbuf;
    shortptr[0] = B_LITTLE16((int16_t)uncompleng);
    if (((bitcnt+7)>>3) < uncompleng)
    {
        shortptr[1] = B_LITTLE16((int16_t)addrcnt);
        return((bitcnt+7)>>3);
    }
    shortptr[1] = (int16_t)0;
    for (i=0; i<uncompleng; i++) lzwoutbuf[i+4] = lzwinbuf[i];
    return(uncompleng+4);
}

static int32_t lzwuncompress(char *lzwinbuf, int32_t compleng, char *lzwoutbuf)
{
    int32_t strtot, currstr, numbits, oneupnumbits;
    int32_t i, dat, leng, bitcnt, outbytecnt, *intptr;
    int16_t *shortptr;

    shortptr = (int16_t *)lzwinbuf;
    strtot = (int32_t)B_LITTLE16(shortptr[1]);
    if (strtot == 0)
    {
        copybuf(lzwinbuf+4,lzwoutbuf,((compleng-4)+3)>>2);
        return((int32_t)B_LITTLE16(shortptr[0])); //uncompleng
    }
    for (i=255; i>=0; i--) { lzwbuf2[i] = i; lzwbuf3[i] = i; }
    currstr = 256; bitcnt = (4<<3); outbytecnt = 0;
    numbits = 8; oneupnumbits = (1<<8);
    do
    {
        intptr = (int32_t *)&lzwinbuf[bitcnt>>3];
        dat = ((B_LITTLE32(intptr[0])>>(bitcnt&7)) & (oneupnumbits-1));
        bitcnt += numbits;
        if ((dat&((oneupnumbits>>1)-1)) > ((currstr-1)&((oneupnumbits>>1)-1)))
            { dat &= ((oneupnumbits>>1)-1); bitcnt--; }

        lzwbuf3[currstr] = dat;

        for (leng=0; dat>=256; leng++,dat=lzwbuf3[dat])
            lzwbuf1[leng] = lzwbuf2[dat];

        lzwoutbuf[outbytecnt++] = dat;
        for (i=leng-1; i>=0; i--) lzwoutbuf[outbytecnt++] = lzwbuf1[i];

        lzwbuf2[currstr-1] = dat; lzwbuf2[currstr] = dat;
        currstr++;
        if (currstr > oneupnumbits) { numbits++; oneupnumbits <<= 1; }
    }
    while (currstr < strtot);
    return((int32_t)B_LITTLE16(shortptr[0])); //uncompleng
}

/*
 * vim:ts=4:sw=4:
 */
