/*Jonas Stagnaro, formerly Jonas Vinter-Jensen
  San Francisco State University, Spring-18
  How to execute on Unixlab:
    Copy system2.c to the relevant Unixlab folder with the scp command,
    log into Unixlab and run the following commands in Terminal:
    1) gcc -o sys2 system2.c -lm
    2) ./sys2 /unixlab/whsu/csc656/Traces/S18/P1/gcc.xac 2
        (with all the cache size variations and for each trace file)
  */

#define MISS_PENALTY 80
#define BLOCK_SIZE 16

#include <errno.h>
#include <inttypes.h>
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*Verbose mode header*/
char* str1 = "order";
char* str2 = "MEM";
char* str3 = "Index";
char* str4 = "tag";
char* str5 = "V";
char* str6 = "D";
char* str7 = "LU";
char* str8 = "cTag";
char* str9 = "dbit";
char* str10 = "hitMiss";
char* str11 = "Case";

char* caseNum = "NULL";
double input_cachesize = 0.0;
double missRate = 0.0;
int cachesize = 0;
int caseCompleted = 0;
int chosenBlock = 0;
int dataAccesses = 0;
int dataReads = 0;
int dataWrites = 0;
int dReadMisses = 0;
int dWriteMisses = 0;
int firstEmptyBlock = -1;
int foundAddress = 0;
int foundPair = 0;
int ic1 = 0;
int ic2 = 0;
int hitOrMiss = 0;
int k = 0;
int index_size = 0;
int lastUsed = 0;
int minIndex = 0;
int offset_size = 0;
int order = 0;
int readMisses = 0;
int set_size = 0;
int tag_size = 0;
int totalMisses = 0;
int valid = 0;
int verboseState = 0;
int writeMisses = 0;
unsigned long int bytesRead = 0;
unsigned long int bytesWritten = 0;
unsigned long int dataMisses = 0;
unsigned long int readCycles = 0;
unsigned long int writeCycles = 0;

uint64_t cTag = 0;
uint64_t Index = 0;
uint64_t offset = 0;
uint64_t tag = 0;

uint64_t ProgramCounter = 0;
char Ld_St = '\0';
uint64_t MEM = 0;

void setVerbose(int);

int verbose(const char* restrict, ...);

struct KwayCache
{
    unsigned int* valid; /*valid only takes up 1 bit of space*/
    unsigned int* dbit;
    uint64_t* tag; /*Tag can at most be 63 bits, if the PC is 64 bits and the Index takes up 1 bit*/
    unsigned int* LU; /*Indicates when the given cache block was last used*/
};


int main(int argc, const char* argv[])
{
    const char* filename;
    FILE* fp = NULL;
    int currentLine = 1;

    /*Arguments: tracefile cachesize set-associativity [-v ic1 ic2], where cachesize is a double*/
    if (argc == 4 || argc == 7) /*There must be either 2 or 5 arguments*/
    {
        int count = 0; /*used for verbose mode*/
        int dbit = 0;
        int i;
        int selectedBlock = -1; /*The block # of the given block, selected within the given set of the cache*/
        struct KwayCache* kCache;

        filename = argv[1]; /*filename = first argument*/
        fp = fopen(filename, "r"); /*read-only mode*/
        if (fp == NULL)
        {
            perror("File could not be found in the current working directory\nExiting...\n");
            exit(EXIT_FAILURE);
        }


        input_cachesize = atof(argv[2]);
        if (ceil(1024 * input_cachesize) == 1024 * input_cachesize &&
            floor(1024 * input_cachesize) == 1024 * input_cachesize)
        {
            cachesize = (int) 1024 * input_cachesize;
        }
        else
        {
            printf("cachesize*1024 must be an integer\n");
            exit(EXIT_FAILURE);
        }


        k = strtol(argv[3], NULL, 10);
        if (k == 0)
        {
            perror("A problem occurred when reading the 2nd argument (set-associativity)");
            exit(EXIT_FAILURE);
        }
        else if(k<2)
        {
            printf("k must be >= 2\n");
            exit(EXIT_FAILURE);
        }

        /*Cache size minimum: k blocks of 16B*/
        if (cachesize % 2 != 0 || cachesize < (k * 16))
        {
            printf("cachesize must be a power of 2 bytes. If it is,"
                   "then the crash may have occurred because the cache size must be at least 2^-6 = 0.015625");
            exit(EXIT_FAILURE);
        }

        if (argv[4] != NULL && strcmp(argv[4], "-v") == 0)
        {
            setVerbose(1);
        }

        if (argc == 7)
        {
            int result1 = strtol(argv[5], NULL, 10);
            int result2 = strtol(argv[6], NULL, 10);
            if (errno == 22 || errno == 34) /*If the base was unsupported or if overflow occurred*/
            {
                perror("Could not convert 5th or 6th argument to a base 10 decimal");
                exit(EXIT_FAILURE);
            }
            ic1 = result1;
            ic2 = result2;
        }


        set_size = cachesize / (k * 16);
        kCache = calloc(set_size, set_size * sizeof(kCache));
        /*For every set in the cache, allocate k blocks per set*/
        for (i = 0; i < set_size; i++)
        {
            kCache[i].valid = calloc(k, k * sizeof(kCache[i].valid));
            kCache[i].dbit = calloc(k, k * sizeof(kCache[i].dbit));
            kCache[i].tag = calloc(k, k * sizeof(kCache[i].tag));
            kCache[i].LU = calloc(k, k * sizeof(kCache[i].LU));
        }


        offset_size = (int) log2((double) BLOCK_SIZE); /*This is safe because BLOCK_SIZE%2 = 0*/
        index_size = (int) log2((double) set_size); /*set_size is guaranteed to be a power of 2, so this is safe*/
        tag_size = 64 - index_size - offset_size;

        while (1)
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

            if (scanLine == EOF)
            {
                break;
            }

            /*Only 3 fscanf units will be passed as arguments: ProgramCounter, Ld_St and MEM*/
            if (scanLine != 3)
            {
                printf("Had trouble with reading line %i of trace\nExiting...\n", currentLine);
                exit(EXIT_FAILURE);
            }

            if (Ld_St == 'L' || Ld_St == 'S')
            {

                offset = MEM << (index_size + tag_size);
                offset = offset >> (index_size + tag_size);

                Index = MEM >> offset_size; /*erase first offset_size bits of ProgramCounter*/
                Index = Index << (offset_size + tag_size); /*erase upper tag_size bits of ProgramCounter*/
                Index = Index >> (tag_size + offset_size); /*Move Index bits all the way to the right side*/

                tag = MEM >> (index_size + offset_size);

                if (Ld_St == 'L')
                {
                    /*Case 1: Hit, read*/
                    for (i = 0; i < k; i++)
                    {
                        if (tag == kCache[Index].tag[i] && kCache[Index].valid[i] == 1)
                        {
                            selectedBlock = i;
                            foundAddress = 1;
                            break;
                        }
                    }
                    /*the block containing A is found in index I id D in the data cache*/
                    if (foundAddress)
                    {
                        dbit = kCache[Index].dbit[selectedBlock]; /*for verbose mode*/
                        lastUsed = kCache[Index].LU[selectedBlock]; /*for verbose mode*/
                        chosenBlock = selectedBlock; /*for verbose mode*/
                        cTag = kCache[Index].tag[selectedBlock]; /*for verbose mode*/
                        valid = kCache[Index].valid[selectedBlock]; /*for verbose mode*/
                        caseCompleted = 1;
                        caseNum = "1";
                        kCache[Index].LU[selectedBlock] = order;
                        readCycles += 1;
                        hitOrMiss = 1;
                    }


                    /*Case 2a: Clean miss, read*/
                    if (!caseCompleted)
                    {
                        for (i = 0; i < k; i++)
                        {
                            if (kCache[Index].valid[i] == 0)
                            {
                                if (firstEmptyBlock == -1)
                                {
                                    firstEmptyBlock = i;
                                    dbit = kCache[Index].dbit[firstEmptyBlock]; /*for verbose mode*/
                                    lastUsed = kCache[Index].LU[firstEmptyBlock]; /*for verbose mode*/
                                    chosenBlock = firstEmptyBlock; /*for verbose mode*/
                                    cTag = kCache[Index].tag[firstEmptyBlock]; /*for verbose mode*/
                                    valid = kCache[Index].valid[firstEmptyBlock]; /*for verbose mode*/
                                    kCache[Index].tag[firstEmptyBlock] = tag;
                                    kCache[Index].valid[firstEmptyBlock] = 1;
                                    kCache[Index].dbit[firstEmptyBlock] = 0;
                                    kCache[Index].LU[firstEmptyBlock] = order;
                                    readCycles += (1 + MISS_PENALTY);
                                    bytesRead += 16;
                                    caseNum = "2a";
                                    caseCompleted = 1;
                                    hitOrMiss = 0;
                                    readMisses++;
                                    dataMisses++;
                                }
                                break;
                            }
                        }
                    }
                    /*Case 2a: clean miss, read*/
                    /*Since no empty blocks were found, replace the block with the smallest LU*/
                    if (firstEmptyBlock == -1 && !caseCompleted)
                    {
                        foundPair = 0;
                        minIndex = 0;
                        for (i = 0; i < (k - 1); i++)
                        {
                            if (tag != kCache[Index].tag[minIndex] && kCache[Index].dbit[minIndex] == 0 &&
                                kCache[Index].valid[minIndex] == 1)
                            {
                                if (tag != kCache[Index].tag[i + 1] && kCache[Index].dbit[i + 1] == 0
                                    && kCache[Index].valid[i + 1] == 1)
                                {
                                    foundPair = 1;

                                    /*Compare block k and block k+1 in pairs, for all possible pairs,
                                     * in order to find the block with the smallest LU value*/
                                    if (kCache[Index].LU[minIndex] <= kCache[Index].LU[i+1])
                                    {
                                        continue;
                                    }
                                    else if (kCache[Index].LU[i] > kCache[Index].LU[i])
                                    {
                                        minIndex = i + 1;
                                    }
                                }
                            }
                        }
                        selectedBlock = minIndex;
                        /*If a clean miss occurred*/
                        if(foundPair)
                        {
                            dbit = kCache[Index].dbit[selectedBlock]; /*for verbose mode*/
                            lastUsed = kCache[Index].LU[selectedBlock]; /*for verbose mode*/
                            chosenBlock = selectedBlock; /*for verbose mode*/
                            cTag = kCache[Index].tag[selectedBlock]; /*for verbose mode*/
                            valid = kCache[Index].valid[selectedBlock]; /*for verbose mode*/
                            kCache[Index].tag[selectedBlock] = tag;
                            kCache[Index].valid[selectedBlock] = 1;
                            kCache[Index].dbit[selectedBlock] = 0;
                            kCache[Index].LU[selectedBlock] = order;
                            readCycles += (1 + MISS_PENALTY);
                            bytesRead += 16;
                            caseCompleted = 1;
                            caseNum = "2a";
                            hitOrMiss = 0;
                            readMisses++;
                            dataMisses++;
                        }
                    }


                    /*Case 2b: Dirty miss, read*/
                    /*It is assumed that there is only 1 hit/miss per scanned line, even if both
                    * dirty and a clean misses occur within the same set of blocks, all matched
                     * to the same tag*/
                    if (!caseCompleted)
                    {
                        foundPair = 0;
                        minIndex = 0;
                        for (i = 0; i < k - 1; i++)
                        {
                            if (tag != kCache[Index].tag[minIndex] && kCache[Index].dbit[minIndex] == 1 &&
                                kCache[Index].valid[minIndex] == 1)
                            {

                                if (tag != kCache[Index].tag[i + 1] && kCache[Index].dbit[i + 1] == 0 &&
                                    kCache[Index].valid[i + 1] == 1)
                                {
                                    foundPair = 1;
                                    if (kCache[Index].LU[minIndex] <= kCache[Index].LU[i+1])
                                    {
                                        continue;
                                    }
                                    else if (kCache[Index].LU[i] > kCache[Index].LU[i])
                                    {
                                        minIndex = i + 1;
                                    }
                                }
                            }
                        }
                        selectedBlock = minIndex;
                        if(selectedBlock != -1)
                        {
                            dbit = kCache[Index].dbit[selectedBlock]; /*for verbose mode*/
                            lastUsed = kCache[Index].LU[selectedBlock]; /*for verbose mode*/
                            chosenBlock = selectedBlock; /*for verbose mode*/
                            cTag = kCache[Index].tag[selectedBlock]; /*for verbose mode*/
                            valid = kCache[Index].valid[selectedBlock]; /*for verbose mode*/
                            kCache[Index].tag[selectedBlock] = tag;
                            kCache[Index].valid[selectedBlock] = 1;
                            kCache[Index].dbit[selectedBlock] = 0;
                            kCache[Index].LU[selectedBlock] = order;
                            readCycles += (1 + 2 * MISS_PENALTY);
                            bytesRead += 16;
                            bytesWritten += 16;
                            caseCompleted = 1;
                            caseNum = "2b";
                            hitOrMiss = 0;
                            dReadMisses++;
                            readMisses++;
                            dataMisses++;
                        }

                    }

                    dataReads++;
                    firstEmptyBlock = -1;
                } /*end of load*/
                else if (Ld_St == 'S')
                {
                    /*Case 1: Hit, write*/
                    for (i = 0; i < k; i++)
                    {
                        if (tag == kCache[Index].tag[i] && kCache[Index].valid[i] == 1)
                        {
                            selectedBlock = i;
                            foundAddress = 1;
                            caseCompleted = 1;
                            break;
                        }
                    }
                    /*the block containing A is found in index I id D in the data cache*/
                    if (foundAddress)
                    {
                        dbit = kCache[Index].dbit[selectedBlock]; /*for verbose mode*/
                        lastUsed = kCache[Index].LU[selectedBlock]; /*for verbose mode*/
                        chosenBlock = selectedBlock; /*for verbose mode*/
                        cTag = kCache[Index].tag[selectedBlock]; /*for verbose mode*/
                        valid = kCache[Index].valid[selectedBlock]; /*for verbose mode*/
                        caseNum = "1";
                        kCache[Index].LU[selectedBlock] = order;
                        kCache[Index].dbit[selectedBlock] = 1;
                        kCache[Index].valid[selectedBlock] = 1;
                        kCache[Index].tag[selectedBlock] = tag;
                        writeCycles += 1;
                        hitOrMiss = 1;
                    }

                    /*Case 2a: Clean miss, write*/
                    if (!caseCompleted)
                    {
                        /*Look for an empty block to fetch the address from MEM into*/
                        for (i = 0; i < k; i++)
                        {
                            if (kCache[Index].valid[i] == 0)
                            {
                                if (firstEmptyBlock == -1)
                                {
                                    firstEmptyBlock = i;
                                    dbit = kCache[Index].dbit[firstEmptyBlock]; /*for verbose mode*/
                                    lastUsed = kCache[Index].LU[firstEmptyBlock]; /*for verbose mode*/
                                    chosenBlock = firstEmptyBlock; /*for verbose mode*/
                                    cTag = kCache[Index].tag[firstEmptyBlock]; /*for verbose mode*/
                                    valid = kCache[Index].valid[firstEmptyBlock]; /*for verbose mode*/
                                    kCache[Index].tag[firstEmptyBlock] = tag;
                                    kCache[Index].valid[firstEmptyBlock] = 1;
                                    kCache[Index].dbit[firstEmptyBlock] = 1;
                                    kCache[Index].LU[firstEmptyBlock] = order;
                                    writeCycles += (1 + MISS_PENALTY);
                                    bytesWritten += 16;
                                    caseNum = "2a";
                                    caseCompleted = 1;
                                    hitOrMiss = 0;
                                    writeMisses++;
                                    dataMisses++;
                                }
                                break;
                            }
                        }
                    }

                    /*If there were no empty blocks, replace the block with the smallest LU value*/
                    if (firstEmptyBlock == -1 && !caseCompleted)
                    {
                        foundPair = 0;
                        minIndex = 0;
                        for (i = 0; i < (k - 1); i++)
                        {
                            if (tag != kCache[Index].tag[minIndex] && kCache[Index].dbit[minIndex] == 0 &&
                                kCache[Index].valid[minIndex] == 1)
                            {
                                if (tag != kCache[Index].tag[i + 1] && kCache[Index].dbit[i + 1] == 0
                                    && kCache[Index].valid[i + 1] == 1)
                                {
                                    foundPair = 1;
                                    if (kCache[Index].LU[minIndex] <= kCache[Index].LU[i+1])
                                    {
                                        continue;
                                    }
                                    else if (kCache[Index].LU[i] > kCache[Index].LU[i])
                                    {
                                        minIndex = i + 1;
                                    }
                                }
                            }
                        }
                        selectedBlock = minIndex;
                        if(foundPair)
                        {
                            dbit = kCache[Index].dbit[selectedBlock]; /*for verbose mode*/
                            lastUsed = kCache[Index].LU[selectedBlock]; /*for verbose mode*/
                            chosenBlock = selectedBlock; /*for verbose mode*/
                            cTag = kCache[Index].tag[selectedBlock]; /*for verbose mode*/
                            valid = kCache[Index].valid[selectedBlock]; /*for verbose mode*/
                            kCache[Index].tag[selectedBlock] = tag;
                            kCache[Index].valid[selectedBlock] = 1;
                            kCache[Index].dbit[selectedBlock] = 1;
                            kCache[Index].LU[selectedBlock] = order;
                            writeCycles += (1 + MISS_PENALTY);
                            bytesWritten += 16;
                            caseCompleted = 1;
                            caseNum = "2a";
                            hitOrMiss = 0;
                            writeMisses++;
                            dataMisses++;
                        }
                    }

                    /*Case 2b: Dirty miss, write*/
                    /*Replace the block with the smallest LU value*/
                    if (!caseCompleted)
                    {
                        foundPair = 0;
                        minIndex = 0;
                        for (i = 0; i < k - 1; i++)
                        {
                            if (tag != kCache[Index].tag[minIndex] && kCache[Index].dbit[minIndex] == 1 &&
                                kCache[Index].valid[minIndex] == 1)
                            {
                                if (tag != kCache[Index].tag[i + 1] && kCache[Index].dbit[i + 1] == 0
                                    && kCache[Index].valid[i + 1] == 1)
                                {
                                    foundPair = 1;
                                    if (kCache[Index].LU[minIndex] <= kCache[Index].LU[i+1])
                                    {
                                        continue;
                                    }
                                    else if (kCache[Index].LU[i] > kCache[Index].LU[i])
                                    {
                                        minIndex = i + 1;
                                    }
                                }
                            }
                        }
                        selectedBlock = minIndex;
                        if(foundPair)
                        {
                            dbit = kCache[Index].dbit[selectedBlock]; /*for verbose mode*/
                            lastUsed = kCache[Index].LU[selectedBlock]; /*for verbose mode*/
                            chosenBlock = selectedBlock; /*for verbose mode*/
                            cTag = kCache[Index].tag[selectedBlock]; /*for verbose mode*/
                            valid = kCache[Index].valid[selectedBlock]; /*for verbose mode*/
                            kCache[Index].tag[selectedBlock] = tag;
                            kCache[Index].valid[selectedBlock] = 1;
                            kCache[Index].dbit[selectedBlock] = 1;
                            kCache[Index].LU[selectedBlock] = order;
                            writeCycles += (1 + 2 * MISS_PENALTY);
                            bytesWritten += 16;
                            bytesRead += 16;
                            caseCompleted = 1;
                            caseNum = "2b";
                            hitOrMiss = 0;
                            dataMisses++;
                            dWriteMisses++;
                            writeMisses++;
                        }
                    }

                    dataWrites++;
                    firstEmptyBlock = -1;
                    caseCompleted = 0;
                }


                /*Verbose output*/
                if (argc == 7 && (order >= ic1 && order <= ic2))
                {

                    if (count == 0)
                    {
                        printf("\n--The Terminal should be in full screen to ensure that the verbose mode output is " \
                               "printed correctly--\n");
                        verbose("%-10s\t%-12s\t%-12s\t%-12s\t%-10s\t%-10s\t%-10s\t" \
                            "%-12s\t%-10s\t%-10s\t%-10s\n",
                                str1, str2, str3, str4, str5, str6, str7, str8, str9, str10, str11);
                    }

                    verbose("%-10d\t%-12" PRIx64 "\t%-12" PRIx64 "\t%-12" PRIx64 "\t%-10d\t%-10d\t%-10d\t" \
                            "%-12" PRIx64 "\t%-10d\t%-10d\t%-10s\n",
                            order, MEM, Index, tag, valid, chosenBlock, lastUsed, cTag, dbit, hitOrMiss, caseNum);
                    count++;
                }

                order++;
                dataAccesses++;
                foundAddress = 0;
                selectedBlock = -1;
                caseCompleted = 0;
                caseNum = "NULL";
                dbit = 0;
            } /*end of load || store*/



        } /*end of while*/

        printf("\nnumber of data reads = %d\n", dataReads);
        printf("number of data writes = %d\n", dataWrites);
        printf("number of data accesses = %d\n", dataAccesses);
        printf("number of total data read misses = %d\n", readMisses);
        printf("number of total data write misses = %d\n", writeMisses);
        printf("number of data misses = %lu\n", dataMisses);
        printf("number of dirty data read misses = %d\n", dReadMisses);
        printf("number of dirty write misses = %d\n", dWriteMisses);
        printf("number of bytes read from memory = %lu\n", bytesRead);
        printf("number of bytes written to memory = %lu\n", bytesWritten);
        printf("total access time (in cycles) for reads = %lu\n", readCycles);
        printf("total access time (in cycles) for writes = %lu\n", writeCycles);
        if (dataAccesses > 0)
        {
            missRate = (double) (readMisses + writeMisses) / dataAccesses;
        }
        printf("overall data cache miss rate = %f\n", missRate);

        for (i = 0; i < set_size; i++)
        {
            free(kCache[i].valid);
            free(kCache[i].dbit);
            free(kCache[i].tag);
            free(kCache[i].LU);
        }
        free(kCache);

    } /*End of input code block*/
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

int verbose(const char* format, ...)
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
