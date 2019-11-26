/*******************************************************
                          cache.cc
********************************************************/

#include <stdlib.h>
#include <assert.h>
#include "cache.h"
using namespace std;

Cache::Cache(int s,int a,int b,unsigned int total_p, Cache **caches)
{
    ulong i, j;
    reads = readMisses = writes = 0;
    writeMisses = writeBacks = currentCycle = 0;
    total_processors = total_p;
    processor_cache = caches;
    size       = (ulong)(s);
    lineSize   = (ulong)(b);
    assoc      = (ulong)(a);
    sets       = (ulong)((s/b)/a);
    numLines   = (ulong)(s/b);
    log2Sets   = (ulong)(log2(sets));
    log2Blk    = (ulong)(log2(b));
    tagMask =0;
    for(i=0;i<log2Sets;i++)
    {
        tagMask <<= 1;
        tagMask |= 1;
    }

    /**create a two dimentional cache, sized as cache[sets][assoc]**/
    cache = new cacheLine*[sets];
    for(i=0; i<sets; i++)
    {
        cache[i] = new cacheLine[assoc];
        for(j=0; j<assoc; j++)
        {
            cache[i][j].invalidate();
        }
    }

}

/**you might add other parameters to Access()
since this function is an entry point
to the memory hierarchy (i.e. caches)**/
void Cache::Access(ulong addr,uchar op)
{
    currentCycle++;/*per cache global counter to maintain LRU order
			among cache ways, updated on every cache access*/

    if(op == 'w') writes++;
    else          reads++;

    cacheLine * line = findLine(addr);
    if(line == NULL)/*miss*/
    {
        if(op == 'w') writeMisses++;
        else readMisses++;

        cacheLine *newline = fillLine(addr);
        if(op == 'w') newline->setFlags(DIRTY);

    }
    else
    {
        /**since it's a hit, update LRU and update dirty flag**/
        updateLRU(line);
        if(op == 'w') line->setFlags(DIRTY);
    }
}

/*look up line*/
cacheLine * Cache::findLine(ulong addr)
{
    ulong i, j, tag, pos;

    pos = assoc;
    tag = calcTag(addr);
    i   = calcIndex(addr);

    for(j=0; j<assoc; j++)
        if(cache[i][j].isValid())
            if(cache[i][j].getTag() == tag)
            {
                pos = j; break;
            }
    if(pos == assoc)
        return NULL;
    else
        return &(cache[i][pos]);
}

/*upgrade LRU line to be MRU line*/
void Cache::updateLRU(cacheLine *line)
{
    line->setSeq(currentCycle);
}

/*return an invalid line as LRU, if any, otherwise return LRU line*/
cacheLine * Cache::getLRU(ulong addr)
{
    ulong i, j, victim, min;

    victim = assoc;
    min    = currentCycle;
    i      = calcIndex(addr);

    for(j=0;j<assoc;j++)
    {
        if(cache[i][j].isValid() == 0) return &(cache[i][j]);
    }
    for(j=0;j<assoc;j++)
    {
        if(cache[i][j].getSeq() <= min) { victim = j; min = cache[i][j].getSeq();}
    }
    assert(victim != assoc);

    return &(cache[i][victim]);
}

/*find a victim, move it to MRU position*/
cacheLine *Cache::findLineToReplace(ulong addr)
{
    cacheLine * victim = getLRU(addr);
    updateLRU(victim);

    return (victim);
}

/*allocate a new line*/
cacheLine *Cache::fillLine(ulong addr)
{
    ulong tag;

    cacheLine *victim = findLineToReplace(addr);
    assert(victim != 0);
    if(victim->getFlags() == DIRTY) writeBack(addr);

    tag = calcTag(addr);
    victim->setTag(tag);
    victim->setFlags(VALID);
    /**note that this cache line has been already
       upgraded to MRU in the previous function (findLineToReplace)**/

    return victim;
}
void Cache::incrementreadorwrite(const char *operation)
{
    switch(*operation)
    {
        case 'r': reads++;
            break;
        case 'w': writes++;
            break;
        default: ; //do nothing
    }
}
int Cache::find_cache_block(ulong address)
{
    int is_present = 0;
    ulong tag, index,i;

    tag = calcTag(address);
    index   = calcIndex(address);

    for(i=0; i<assoc; i++)
        if(cache[index][i].isValid())
            if(cache[index][i].getTag() == tag)
            {
                is_present = 1;
                break;
            }

    return is_present;
}

void Cache::MSI_ReadMiss(unsigned int processor_number, ulong address) {
    unsigned int i;
    cacheLine *Fill_Line = fillLine(address);
    readMisses++;
    Fill_Line->setFlags(SHARED);
    //Issue BusRd
    for (i = 0; i < total_processors; i++) {
        if (i == processor_number)
            continue;
        string operation_busRD = "R";
        processor_cache[i]->MSI_BusTransaction(i, address, operation_busRD.c_str());
    }
}

void Cache::MSI_WriteMiss(unsigned int processor_number, ulong address) {
    unsigned int i;
    cacheLine *Fill_Line = fillLine(address);
    writeMisses++;
    Fill_Line->setFlags(MODIFIED);
    BusRdX++;   //Issue BusRdX
    for (i = 0; i < total_processors; i++) {
        if (i == processor_number)
            continue;
        string operation_BusRdX = "X";
        processor_cache[i]->MSI_BusTransaction(i, address, operation_BusRdX.c_str());
    }
}
void Cache::MSI_BusTransaction(unsigned int processor_number, ulong address, const char* operation) {

    cacheLine * Line = findLine(address);
    if(Line != NULL)        //check for cache hit
        {
            //MODIFIED state
            if ( Line->getFlags() == MODIFIED )
            {
                if ( *operation == 'R')
                {

                    Line->setFlags(SHARED);
                    interventions++;
                    writeBacks++;; //flush
                    flush++;
                }

                if( *operation == 'X' )
                {

                    Line->setFlags(INVALID);
                    invalidation++;
                    writeBacks++;;  //issue flush
                    flush++;
                }
            }
            else if ( Line->getFlags() == SHARED )
            {
                if ( *operation == 'R')
                {
                    Line->setFlags(SHARED);
                }
                if ( *operation == 'X')
                {
                    Line->setFlags(INVALID);
                    invalidation++;
                }
            }
        }
}
void Cache::MSI_Access(unsigned int processor_number, ulong address,  const char* operation)
{
    unsigned int i;
    currentCycle++; //per cache global counter to maintain LRU order
    incrementreadorwrite(operation);
    if((*operation == 'r') || (*operation == 'w')) {
        cacheLine *Line = findLine(address);

        if (Line == NULL)       //cache miss
        {
            if (*operation == 'r') {
                MSI_ReadMiss(processor_number,address);
            } else if (*operation == 'w') {
               MSI_WriteMiss(processor_number,address);
            }
        } else //cache hit --> getFlags/setFlags being used to determine which state the cache block should be in
        {
            updateLRU(Line);
            if (Line->getFlags() == MODIFIED) {
                Line->setFlags(MODIFIED);
            } else if (Line->getFlags() == SHARED) {
                if (*operation == 'r') {
                    Line->setFlags(SHARED);
                }
                if (*operation == 'w') {
                    Line->setFlags(MODIFIED);
                    BusRdX++; //Issue BusRdX
                    memory_transactions++;
                    for (i = 0; i < total_processors; i++) {
                        if (i == processor_number)
                            continue;
                        string operation_BusRdX = "X";
                        processor_cache[i]->MSI_BusTransaction(i, address, operation_BusRdX.c_str());
                    }
                }
            }

        }
    }
}
void Cache::MESI_BusTransaction(unsigned int processor_number, ulong address, const char* operation)
{
    cacheLine * Line = findLine(address);
        if(Line != NULL)  //if block is present
        {
            if ( Line->getFlags() == MODIFIED )
            {
                if ( *operation == 'R')
                {
                    Line->setFlags(SHARED);
                    interventions++;
                    writeBacks++;
                    flush++;
                }

                if( *operation == 'X' )
                {
                    Line->setFlags(INVALID);
                    invalidation++;
                    writeBacks++;
                    flush++;
                }
            }
            else if ( Line->getFlags() == SHARED )
            {
                if ( *operation == 'R')
                {
                    Line->setFlags(SHARED);
                }

                if ( *operation == 'X')
                {
                    Line->setFlags(INVALID);
                    invalidation++;
                }
                if ( *operation == 'M')
                {
                    Line->setFlags(INVALID);
                    invalidation++;
                }
            }
            else if ( Line->getFlags() == EXCLUSIVE)
            {
                if ( *operation == 'R')
                {
                    Line->setFlags(SHARED);
                    interventions++;
                }

                if ( *operation == 'X' )
                {
                    Line->setFlags(INVALID);
                    invalidation++;
                }
            }
        }
}
void Cache::MESI_ReadMiss(unsigned int processor_number, ulong address, int is_unique)
{
    unsigned int i;
    cacheLine *Fill_Line = fillLine(address);
    readMisses++;
    if (is_unique == 1)
    {
        Fill_Line->setFlags(SHARED);
        cache_to_cache++;
        for(i=0;i<total_processors;i++)
        {
            if (i == processor_number)
                continue;
            string operation_BusRd = "R";
            processor_cache[i]->MESI_BusTransaction(i, address, operation_BusRd.c_str());
        }
    }
    else if (is_unique == 0 )
    {
        Fill_Line->setFlags(EXCLUSIVE);
        for(i=0;i<total_processors;i++)
        {
            if (i == processor_number)
                continue;
            string operation_BusRd = "R";
            processor_cache[i]->MESI_BusTransaction(i, address, operation_BusRd.c_str());
        }
    }
}

void Cache::MESI_WriteMiss(unsigned int processor_number, ulong address, int is_unique)
{
    unsigned int i;
    cacheLine *Fill_Line = fillLine(address);
    Fill_Line->setFlags(MODIFIED);
    writeMisses++;
    if (is_unique == 1)
        cache_to_cache++;
    BusRdX++;
    for(i=0;i<total_processors;i++)
    {
        if (i == processor_number)
            continue;
        string operation_BusRdX = "X";
        processor_cache[i]->MESI_BusTransaction(i, address, operation_BusRdX.c_str());
    }
}

void Cache::MESI_Access(unsigned int processor_number, ulong address, const char* operation)
{
    unsigned int i;
    int is_unique = 0;
    currentCycle++;
    incrementreadorwrite(operation);
    for(i=0;i<total_processors;i++)         //find if block is unique to this cache
    {
        if (i == processor_number)
            continue;
        is_unique = processor_cache[i]->find_cache_block(address);
        if (is_unique == 1)
            break;
    }

    if ( (*operation == 'r') || (*operation == 'w') )
    {
        cacheLine * Line = findLine(address);
        if(Line == NULL)  //cache miss
        {
            if (*operation == 'r')
            {
                MESI_ReadMiss(processor_number, address, is_unique);
            }
            else if ( *operation == 'w')
            {
                MESI_WriteMiss(processor_number,address,is_unique);
            }
        }
        else
        {
            updateLRU(Line);
            if( Line->getFlags() == MODIFIED )
            {
                Line->setFlags(MODIFIED);
            }
            else if( Line->getFlags() == SHARED )
            {
                if( *operation == 'r')
                {
                    Line->setFlags(SHARED);
                }

                if( *operation == 'w')
                {
                    Line->setFlags(MODIFIED);
                    for(i=0;i<total_processors;i++)
                    {
                        if(i == processor_number)
                            continue;
                        string operation_BusRdX = "M";
                        processor_cache[i]->MESI_BusTransaction(i, address, operation_BusRdX.c_str());
                    }
                }

            }
            else if ( Line->getFlags() == EXCLUSIVE )
            {
                if ( *operation == 'r')
                {
                    Line->setFlags(EXCLUSIVE);
                }
                if ( *operation == 'w')
                {
                    Line->setFlags(MODIFIED);
                }
            }
        }
    }
}

void Cache::Dragon_Access(unsigned int processor_number, ulong address, const char* operation)
{
    currentCycle++;
    incrementreadorwrite(operation);
    int is_unique = 0;
    unsigned i;

    for(i=0;i<total_processors;i++)
    {
        if ( i == processor_number)
            continue;
        is_unique = processor_cache[i]->find_cache_block(address);
        if (is_unique == 1)
            break;
    }
    if ((*operation == 'r') || (*operation == 'w'))
    {
        cacheLine * Line = findLine(address);
        if(Line == NULL)
        {
            if(*operation == 'r')
            {
                Dragon_ReadMiss(processor_number,address,is_unique);
            }
            if(*operation == 'w')
            {
                Dragon_WriteMiss(processor_number, address, is_unique);
            }
        }
        else {
            updateLRU(Line);
            if (Line->getFlags() == MODIFIED) {
                Line->setFlags(MODIFIED);
            } else if (Line->getFlags() == EXCLUSIVE) {
                if (*operation == 'r')
                    Line->setFlags(EXCLUSIVE);
                else if (*operation == 'w')
                    Line->setFlags(MODIFIED);
            } else if (Line->getFlags() == SM) {
                if (*operation == 'r')
                    Line->setFlags(SM);
                else if (*operation == 'w') {
                    if (is_unique == 1) {
                        Line->setFlags(SM);
                        for (i = 0; i < total_processors; i++) {
                            if (i == processor_number)
                                continue;
                            string operation_BusUp = "U";
                            processor_cache[i]->Dragon_BusTransaction(i, address, operation_BusUp.c_str());
                        }

                    } else if (is_unique == 0) {
                        Line->setFlags(MODIFIED);
                        for (i = 0; i < total_processors; i++) {
                            if (i == processor_number)
                                continue;
                            string operation_BusUp = "U";
                            processor_cache[i]->Dragon_BusTransaction(i, address, operation_BusUp.c_str());
                        }
                    }
                }
            }
            else if (Line->getFlags() == SC)
            {
                if(*operation == 'r')
                Line->setFlags(SC);
                else if(*operation == 'w')
                {
                    if ( is_unique == 1)
                    {
                        Line->setFlags(SM);
                        for(i=0;i<total_processors;i++)
                        {
                            if(i == processor_number)
                                continue;
                            string operation_BusUp = "U";
                            processor_cache[i]->Dragon_BusTransaction(i, address, operation_BusUp.c_str());
                        }
                    }

                    else if (is_unique == 0) {
                        Line->setFlags(MODIFIED);
                        for (i = 0; i < total_processors; i++) {
                            if (i == processor_number)
                                continue;
                            string operation_BusUp = "U";
                            processor_cache[i]->Dragon_BusTransaction(i, address, operation_BusUp.c_str());
                        }
                    }
                }
            }
        }
    }
}
void Cache::Dragon_WriteMiss(unsigned int processor_number, ulong address, int is_unique)
{
    unsigned int i;
    writeMisses++;
    cacheLine *Fill_Line = fillLine(address);
    if(is_unique == 1)
    {
        Fill_Line->setFlags(SM);

        for(i=0;i<total_processors;i++)
        {
            if(i == processor_number)
                continue;
            string operation_BusRd = "R";
            processor_cache[i]->Dragon_BusTransaction(i, address, operation_BusRd.c_str());
        }
        for(i=0;i<total_processors;i++)
        {
            if (i==processor_number)
                continue;
            string operation_BusUp = "U";
            processor_cache[i]->Dragon_BusTransaction(i, address, operation_BusUp.c_str());
        }
    }

    else if(is_unique == 0)
    {
        Fill_Line->setFlags(MODIFIED);
        for(i=0;i<total_processors;i++)
        {
            if( i == processor_number )
                continue;
            string operation_BusRd = "R";
            processor_cache[i]->Dragon_BusTransaction(i, address, operation_BusRd.c_str());
        }
    }
}
void Cache::Dragon_ReadMiss(unsigned int processor_number, ulong address, int is_unique) {
    unsigned i;
    cacheLine *Fill_Line = fillLine(address);
    readMisses++;
    if (is_unique == 1) {
        Fill_Line->setFlags(SC);
        for (i = 0; i < total_processors; i++) {
            if (i == processor_number)
                continue;
            string operation_BusRd = "R";
            processor_cache[i]->Dragon_BusTransaction(i, address, operation_BusRd.c_str());
        }
    } else {
        Fill_Line->setFlags(EXCLUSIVE);
        for (i = 0; i < total_processors; i++) {
            if (i == processor_number)
                continue;
            string operation_BusRd = "R";
            processor_cache[i]->Dragon_BusTransaction(i, address, operation_BusRd.c_str());
        }
    }
}

void Cache::Dragon_BusTransaction(unsigned int processor_number, ulong address, const char* operation)
{
        ulong newtag;
        cacheLine *  Line = findLine(address);
        if (Line != NULL)
        {
            if ( Line->getFlags() == EXCLUSIVE)
            {
                if ( *operation == 'R')
                {
                    Line->setFlags(SC);
                    interventions++;
                }
            }
            else if ( Line->getFlags() == MODIFIED) {
                if (*operation == 'R') {
                    Line->setFlags(SM);
                    interventions++;
                    flush++;
                }
            }
            else if ( Line->getFlags() == SC)
            {
                if ( *operation == 'R')
                {
                    Line->setFlags(SC);
                }
                if ( *operation == 'U' )
                {
                    Line->setFlags(SC);
                    newtag = calcTag(address);
                    Line->setTag(newtag);
                }
            }
            else if ( Line->getFlags() == SM)
            {
                if ( *operation == 'R')
                {
                    Line->setFlags(SM);
                    flush++;
                }
                if ( *operation == 'U')
                {
                    Line->setFlags(SC);
                    newtag = calcTag(address);
                    Line->setTag(newtag);
                }
            }
        }
}

void Cache::printStats(ulong m)
{
    float miss_rate = (float) (100 * ((float)(readMisses + writeMisses)/(float)(reads+writes)));

    cout
            << "01. number of reads:      \t" << reads << '\n'
            << "02. number of read misses:\t" << readMisses << '\n'
            << "03. number of writes:     \t" << writes << '\n'
            << "04. number of write misses:\t" << writeMisses << endl;
    if (miss_rate >= 10)
        cout << "05. total miss rate:      \t" << miss_rate << "%" <<endl;
    else if (miss_rate < 10)
        cout << "05. total miss rate:      \t" << miss_rate << "%" << endl;
    cout << "06. number of writebacks: \t" << writeBacks << '\n'
         << "07. number of cache-to-cache transfers: " <<  cache_to_cache << '\n'
         << "08. number of memory transactions: " << m << '\n'
         << "09. number of interventions:\t" << interventions << '\n'
         << "10. number of invalidations:\t" << invalidation << '\n'
         << "11. number of flushes:    \t" << flush << '\n'
         << "12. number of BusRdX:     \t" <<  BusRdX << endl;
}

