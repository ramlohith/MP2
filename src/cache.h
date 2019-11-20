/*******************************************************
                          cache.h
********************************************************/

#ifndef CACHE_H
#define CACHE_H
#define MAX_NUMBER_OF_PROCESSORS 20
#include <cmath>
#include <iostream>
#include <string>
#include <cstring>
#include <sstream>
typedef unsigned long ulong;
typedef unsigned char uchar;
typedef unsigned int uint;
//extern int total_processors;
/****add new states, based on the protocol****/
enum{
    INVALID = 0,
    VALID,
    DIRTY,
    SHARED,
    MODIFIED,
    EXCLUSIVE
};

class cacheLine
{
protected:
    ulong tag;
    ulong Flags;   // 0:invalid, 1:valid, 2:dirty
    ulong seq;

public:
    cacheLine()            { tag = 0; Flags = 0; }
    ulong getTag()         { return tag; }
    ulong getFlags()			{ return Flags;}
    ulong getSeq()         { return seq; }
    void setSeq(ulong Seq)			{ seq = Seq;}
    void setFlags(ulong flags)			{  Flags = flags;}
    void setTag(ulong a)   { tag = a; }
    void invalidate()      { tag = 0; Flags = INVALID; }//useful function
    bool isValid()         { return ((Flags) != INVALID); }
};

class Cache
{
protected:
    ulong size, lineSize, assoc, sets, log2Sets, log2Blk, tagMask, numLines;
    ulong reads,readMisses,writes,writeMisses,writeBacks,BusRdX,memory_transactions,interventions,flush,invalidation,cache_to_cache;

    ulong calcTag(ulong addr)     { return (addr >> (log2Blk) );}
    ulong calcIndex(ulong addr)  { return ((addr >> log2Blk) & tagMask);}
    ulong calcAddr4Tag(ulong tag)   { return (tag << (log2Blk));}

public:
    unsigned int total_processors;
    ulong currentCycle;
    cacheLine **cache;
    Cache **processor_cache;
    Cache(int,int,int,unsigned int, Cache *a[]);
    ~Cache() { delete cache;}

    cacheLine *findLineToReplace(ulong addr);
    cacheLine *fillLine(ulong addr);
    cacheLine * findLine(ulong addr);
    cacheLine * getLRU(ulong);

    ulong getRM(){return readMisses;} ulong getWM(){return writeMisses;}
    ulong getReads(){return reads;}ulong getWrites(){return writes;}
    ulong getWB(){return writeBacks;}
    ulong getmemory_transactions(){return memory_transactions;}
    ulong getinterventions(){return interventions;}
    ulong getflush(){return flush;}
    ulong getinvalidations(){return invalidation;}
    ulong getcache_to_cache(){return cache_to_cache;}
    void writeBack(ulong)   {writeBacks++;}
    void Access(ulong,uchar);
    void printStats(ulong);
    void updateLRU(cacheLine *);
    void MSI_Access(unsigned int, ulong, const char*);
    void MESI_Access(unsigned int, ulong, const char*);
    void Dragon_Access(unsigned int, ulong, const char*);
    void incrementreadorwrite(const char *);
    int find_cache_block(ulong);
    void MSI_ReadMiss(unsigned int);
    void MSI_WriteMiss(unsigned int);
    void MSI_WriteHit(unsigned int);
    void MSI_BusTransaction(unsigned int, ulong, const char*);
};
//Cache **processor_cache = new Cache *[MAX_NUMBER_OF_PROCESSORS]; //declare global caches

#endif
