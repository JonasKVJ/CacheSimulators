/*Jonas Stagnaro, formerly Jonas Vinter-Jensen
  San Francisco State University, Spring-18
  How to execute on Unixlab:
    Copy system1.c to the relevant Unixlab folder with the scp command,
    log into Unixlab and run the following commands in Terminal:
    1) gcc -o sys1 system1.c -lm
    2) ./sys1 /unixlab/whsu/csc656/Traces/S18/P1/gcc.xac 2
        (with all the cache size variations and for each trace file)
  */

#include <errno.h>
#include <inttypes.h>
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MISS_PENALTY 80
#define BLOCK_SIZE 16

char* caseNum = "NULL";

/*Verbose mode header*/
char* str1 = "order";
char* str2 = "MEM";
char* str3 = "Index";
char* str4 = "tag";
char* str5 = "V";
char* str6 = "cTag";
char* str7 = "dbit";
char* str8 = "hitMiss";
char* str9 = "Case";


double input_cachesize = 0.0;
double missRate = 0.0;
FILE *fp = NULL;
int cacherows = 0;
int cachesize = 0;
int ctag = -1;
int currentLine = 1;
int dataAccesses = 0;
int dataIndex = 0;
int dataMisses = 0;
int dataReads = 0;
int dataWrites = 0;
int dbit = -1;
int dReadMisses = 0;
int dWriteMisses = 0;
int ic1 = 0;
int ic2 = 0;
int index_size = 0;
int hitOrMiss = -1;
int offset_size = 0; /* # of bits in block offset = log2(BLOCK_SIZE)*/
int order = 0;
int readBytes = 0;
int readMisses = 0;
int readCycles = 0;
int tag_size = 0;
int verboseState = 0;
int writeCycles = 0;
int writeMisses = 0;
unsigned long int readMEMBytes = 0;
unsigned long int writtenMEMBytes = 0;
uint64_t Index = 0;
uint64_t offset = 0;
uint64_t tag = 0;
int8_t valid = 0;

uint64_t ProgramCounter = 0;
char Ld_St = '\0';
uint64_t MEM = 0;

void setVerbose(int);
int verbose(const char *restrict, ...);


struct DirectCache
{
    unsigned int dbit : 1;
    unsigned int valid : 1; /*valid only takes up 1 bit of space*/
    uint64_t tag; /*Tag can at most be 63 bits, if the PC is 64 bits and the Index takes up 1 bit*/
};

int main(int argc, char *argv[])
{
    const char* filename;

    /*Arguments: tracefile cachesize [-v ic1 ic2], where cachesize is a double*/
    if (argc == 3 || argc == 6) /*There must be either 2 or 5 arguments*/
    {
        int count = 0; /*used for verbose mode*/
        int i;
        struct DirectCache* dCache;

        filename = argv[1]; /*filename = first argument*/
        fp = fopen(filename, "r"); /*read-only mode*/
        if (fp == NULL)
        {
            perror("File could not be found in the current working directory\nExiting...\n");
            exit(EXIT_FAILURE);
        }

        input_cachesize = atof(argv[2]);
        if(ceil(1024*input_cachesize)==1024*input_cachesize && floor(1024*input_cachesize)==1024*input_cachesize)
        {
            /*If the cachesize, represented in bytes, is an integer, then it can be converted to an int.*/
            cachesize = (int)1024 * input_cachesize;
        }
        else
        {
            printf("cachesize*1024 must be an integer\n");
            exit(EXIT_FAILURE);
        }
        /*Cache size minimum: 1 block of 16B*/
        if (cachesize % 2 != 0 || cachesize < 16)
        {
            printf("cachesize must be a power of 2 bytes. If it is,"
                   "then the crash may have occurred because the cache size must be at least 2^-6 = 0.015625");
            exit(EXIT_FAILURE);
        }

        /*Since cachesize is the size of the data section in the cache, and because each row of the data section
        is 16 Bytes, the # of rows in the cache = 1000*cachesize/16, where cachesize and 16 are both in KB.*/
        cacherows = cachesize/16;
        index_size = (int) log2((double)cacherows); /*cacherows is guaranteed to be a power of 2, so this is safe*/
        offset_size = (int) log2((double) BLOCK_SIZE); /*This is safe because BLOCK_SIZE%2 = 0*/
        tag_size = 64-index_size-offset_size;

        if (argv[3] != NULL && strcmp(argv[3], "-v") == 0)
        {
            setVerbose(1);
        }

        if(argc==6)
        {
            int result1 = strtol(argv[4], NULL, 10);
            int result2 = strtol(argv[5], NULL, 10);
            if(errno==22 || errno==34) /*If the base was unsupported or if overflow occurred*/
            {
                perror("Could not convert 4th or 5th argument to a base 10 decimal");
                exit(EXIT_FAILURE);
            }
            ic1 = result1;
            ic2 = result2;
        }



        dCache = calloc(cacherows, cacherows * sizeof(dCache));

        /*Since each block in the data section is 16 Bytes = 32*4 bits = 64*2 bits (for the x86 architecture),
         * each block will always contain two 64-bit addresses. Each data memory address is exactly 44 bits long
         * , so there will be 20 wasted bits per address*/
        for(i=0; i<cacherows; i++) /*initialize direct-mapped cache to 0*/
        {
            dCache[i].valid = 0;
            dCache[i].tag = 0;
            dCache[i].dbit = 0;
        }

        while(1)
        {
            int scanLine;

            scanLine = fscanf(fp,
                              "%*" SCNi32
            "%" SCNx64
            "%*" SCNi32
            "%*" SCNi32
            "%*" SCNi32
            " %*c"
            " %*c"
            " %c"
            "%*" SCNi64
            "%" SCNx64
            "%*" SCNx64
            "%*" SCNx64
            "%*11s"
            "%*22s",
                    &ProgramCounter,
                    &Ld_St,
                    &MEM);

            if(scanLine==EOF)
            {
                break;
            }

            /*Only 3 fscanf units will be passed as arguments: ProgramCounter, Ld_St and MEM*/
            if(scanLine != 3)
            {
                printf("Had trouble with reading line %i of trace\nExiting...\n", currentLine);
                exit(EXIT_FAILURE);
            }


            /*printf("ProgramCounter = %" PRIx64 "\t\tLd_St = %c\t\tMEM = %" PRIx64 "\n", ProgramCounter, Ld_St, MEM);*/
            if(Ld_St=='L' || Ld_St=='S')
            {
                offset = MEM << (index_size + tag_size);
                offset = offset >> (index_size + tag_size);

                Index = MEM >> offset_size; /*erase first offset_size bits of ProgramCounter*/
                Index = Index << (offset_size + tag_size); /*erase upper tag_size bits of ProgramCounter*/
                Index = Index >> (tag_size + offset_size); /*Move Index bits all the way to the right side*/

                tag = MEM >> (index_size + offset_size);

                dbit = dCache[Index].dbit;
                ctag = dCache[Index].tag;
                valid = dCache[Index].valid;

                if(Ld_St=='L')
                {
                    /*Case 1: the block containing A is found in data cache (cache hit)*/
                    /*Read: no state changes, 1 cycle*/
                    if(dCache[Index].valid==1 && tag==dCache[Index].tag)
                    {
                        readCycles += 1;
                        hitOrMiss = 1;
                        caseNum = "1";
                    }
                    /*Case 2a: Clean cache miss, Read*/
                    else if(dCache[Index].dbit==0 || dCache[Index].valid==0)
                    {
                        /*Read, move block containing A from MEM into Index I in data cache*/
                        dCache[Index].tag = tag;
                        dCache[Index].dbit = 0;
                        dCache[Index].valid = 1;

                        readMEMBytes += 16;
                        readCycles += (1 + MISS_PENALTY);
                        readMisses++;
                        dataMisses++;
                        dReadMisses++;
                        hitOrMiss = 0;
                        caseNum = "2a";
                    }
                    /*Case 2b: Dirty cache miss, Read*/
                    else if(dCache[Index].valid==1 && tag != dCache[Index].tag && dCache[Index].dbit==1)
                    {
                        /*Read: write block X to memory, move block containing A from memory into data cache.*/
                        dCache[Index].tag = tag;
                        dCache[Index].dbit = 0;
                        dCache[Index].valid = 1;

                        writtenMEMBytes += 16;
                        readMEMBytes += 16;
                        readMisses++;
                        dataMisses++;
                        readCycles += (1 + 2*MISS_PENALTY);
                        dReadMisses++;
                        hitOrMiss = 0;
                        caseNum = "2b";
                    }

                    dCache[Index].dbit = 0;
                    dataReads++;
                }
                else if(Ld_St=='S')
                {
                    /*Case 1: Cache hit, Write*/
                    if(dCache[Index].valid==1 && tag==dCache[Index].tag)
                    {
                        dCache[Index].tag = tag;
                        dCache[Index].dbit = 1;
                        dCache[Index].valid = 1;

                        writtenMEMBytes += 16;
                        writeCycles += 1;
                        hitOrMiss = 1;
                        caseNum = "1";
                    }
                    /*Case 2a: Clean cache miss, Write*/
                    else if( (tag!=dCache[Index].tag && dCache[Index].dbit==0)
                            || (dCache[Index].valid==0) )
                    {
                        /*Write: move block containing A from memory into Index I data cache, dirty bit = 1*/
                        dCache[Index].tag = tag;
                        dCache[Index].dbit = 1;
                        dCache[Index].valid = 1;

                        readMEMBytes += 16;
                        writeCycles += (1 + MISS_PENALTY);
                        writeMisses++;
                        dataMisses++;
                        dWriteMisses++;
                        hitOrMiss = 0;
                        caseNum = "2a";
                    }
                    /*Case 2b: Dirty cache miss, write*/
                    else if(dCache[Index].dbit==1 && dCache[Index].valid==1 && tag!=dCache[Index].tag)
                    {
                        /*Write: write block X to memory move block containing A from memory into data cache*/
                        dCache[Index].tag = tag;
                        dCache[Index].dbit = 1;
                        dCache[Index].valid = 1;

                        readMEMBytes += 16;
                        writtenMEMBytes += 16;
                        writeCycles += (1 + 2*MISS_PENALTY);
                        writeMisses++;
                        dataMisses++;
                        dWriteMisses++;
                        hitOrMiss = 0;
                        caseNum = "2b";
                    }

                    dataWrites++;
                } /*End of Store*/

                if(argc==6 && order>=ic1 && order<=ic2)
                {
                    if(count==0)
                    {
                        printf("--The ideal window size for the Terminal to display the results is approximately "
                               "half the screen--\n");
                        verbose("%s\t%-10s\t%-12s\t%-12s\t%-10s\t%-12s\t%-10s\t%-10s\t%-10s\n",
                                str1, str2, str3, str4, str5, str6, str7, str8, str9);
                    }
                    verbose("%d\t%-12" PRIx64 "\t%-12" PRIx64 "\t%-12" PRIx64 "\t%-10d\t%-12" PRIx64 "\t%-10d\t%-10d\t%-10s\n",
                            order, MEM, Index, tag, valid, ctag, dbit, hitOrMiss,
                            caseNum);
                    count++;
                }


                order++;
                dataAccesses++;
            } /*End of load-store*/

            currentLine++;
        } /*end of while*/

        printf("number of data reads = %d\n", dataReads);
        printf("number of data writes = %d\n", dataWrites);
        printf("number of data accesses = %d\n", dataAccesses);
        printf("number of total data read misses = %d\n", readMisses);
        printf("number of total data write misses = %d\n", writeMisses);
        printf("number of data misses = %d\n", dataMisses);
        printf("number of dirty data read misses = %d\n", dReadMisses);
        printf("number of dirty write misses = %d\n", dWriteMisses);
        printf("number of bytes read from memory = %lu\n", readMEMBytes);
        printf("number of bytes written to memory = %lu\n", writtenMEMBytes);
        printf("total access time (in cycles) for reads = %d\n", readCycles);
        printf("total access time (in cycles) for writes = %d\n", writeCycles);
        missRate = (double) (readMisses+writeMisses)/dataAccesses;
        printf("overall data cache miss rate = %f\n", missRate);


        free(dCache);
    }
    else
    {
        printf("Arguments required: tracefile cachesize [-v ic1 ic2]\nExiting...\n");
        exit(EXIT_FAILURE);
    }




    return 0;
}

void setVerbose(int state)
{
    verboseState = state;
    return;
}

int verbose(const char *format, ...)
{
    if (!verboseState)
    {
        return 0;
    }
    else
    {
        int result;
        va_list arglist;

        /*Initialize arglist with the variable number of arguments in ... and format*/
        va_start(arglist, format);

        /*Type of printf which can accept a list of arguments of type va_list*/
        result = vprintf(format, arglist);

        /*Prevent arglist from being called again, after all arguments have been used*/
        va_end(arglist);
        return result;
    }
}
