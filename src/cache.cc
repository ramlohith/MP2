/*******************************************************
                          cache.cc
********************************************************/

#include <stdlib.h>
#include <assert.h>
#include "cache.h"
using namespace std;

Cache::Cache(int s,int a,int b,int total_p, Cache **cache)
{
    ulong i, j;
    reads = readMisses = writes = 0;
    writeMisses = writeBacks = currentCycle = 0;
    total_processors = total_p;
    processor_cache = cache;
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
void Cache::MSI_Access(unsigned int processor_number, ulong address,  const char* operation)
{
    int i;
    currentCycle++; //per cache global counter to maintain LRU order
    incrementreadorwrite(operation);
    if((*operation == 'r') || (*operation == 'w')) {
        cacheLine *Line = findLine(address);

        if (Line == NULL)       //cache miss
        {
            cacheLine *Fill_Line = fillLine(address);
            if (*operation == 'r') {
                readMisses++;
                Fill_Line->setFlags(SHARED);
                //Issue BusRd
                for (i = 0; i < total_processors; i++) {
                    if (i == processor_number)
                        continue;
                    string operation_busRD = "R";
                    processor_cache[i]->MSI_Access(i, address, operation_busRD.c_str());
                }
            } else if (*operation == 'w') {
                writeMisses++;
                Fill_Line->setFlags(MODIFIED);
                BusRdX++;   //Issue BusRdX
                for (i = 0; i < total_processors; i++) {
                    if (i == processor_number)
                        continue;
                    string operation_BusRdX = "X";
                    processor_cache[i]->MSI_Access(i, address, operation_BusRdX.c_str());
                }
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
                        processor_cache[i]->MSI_Access(i, address, operation_BusRdX.c_str());
                    }
                }
            }

        }
    }
        //for recursive call of this function --> to maintain state among other processors for same address, when cache hit.
    else if ( (*operation == 'R') || (*operation == 'X') )
    {
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
                    writeBack(address); //flush
                    flush++;
                }

                if( *operation == 'X' )
                {
                    Line->setFlags(INVALID);
                    invalidation++;
                    writeBack(address);  //issue flush
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
}

void Cache::MESI_Access(unsigned int processor_number, ulong address, const char* operation)
{
    int i,is_unique = 0;;
    currentCycle++;
    incrementreadorwrite(operation);
    for(i=0;i<total_processors;i++)
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
            cacheLine *Fill_Line = fillLine(address);
            if (*operation == 'r')
            {
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
                        processor_cache[i]->MESI_Access(i, address, operation_BusRd.c_str());
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
                        processor_cache[i]->MESI_Access(i, address, operation_BusRd.c_str());
                    }
                }

            }
            else if ( *operation == 'w')
            {
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
                    processor_cache[i]->MESI_Access(i, address, operation_BusRdX.c_str());
                }
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
                        processor_cache[i]->MESI_Access(i, address, operation_BusRdX.c_str());
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
    else if ( (*operation == 'R') || (*operation == 'X') || (*operation == 'M'))
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
                    writeBack(address);
                    flush++;
                }

                if( *operation == 'X' )
                {
                    Line->setFlags(INVALID);
                    invalidation++;
                    writeBack(address);
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

}
void Cache::Dragon_Access(unsigned int p, ulong addr, const char* op)
{

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
        cout << "05. total miss rate:      \t" << setprecision(4) << miss_rate << "%" <<endl;
    else if (miss_rate < 10)
        cout << "05. total miss rate:      \t" << setprecision(3) << miss_rate << "%" << endl;
    cout << "06. number of writebacks: \t" << writeBacks << '\n'
         << "07. number of cache-to-cache transfers: " <<  cache_to_cache << '\n'
         << "08. number of memory transactions: " << m << '\n'
         << "09. number of interventions:\t" << interventions << '\n'
         << "10. number of invalidations:\t" << invalidation << '\n'
         << "11. number of flushes:    \t" << flush << '\n'
         << "12. number of BusRdX:     \t" <<  BusRdX << endl;
}

