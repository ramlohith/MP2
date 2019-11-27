/*******************************************************
                          main.cc
********************************************************/

#include <stdlib.h>
#include <assert.h>
#include <fstream>
#include <iostream>
#include "cache.h"
using namespace std;

int main(int argc, char *argv[])
{
    ifstream fin;
    FILE * pFile;

    if(argv[1] == NULL){
        printf("input format: ");
        printf("./smp_cache <cache_size> <assoc> <block_size> <num_processors> <protocol> <trace_file> \n");
        exit(0);
    }

    /*****uncomment the next five lines*****/
    int cache_size = atoi(argv[1]);
    int cache_assoc= atoi(argv[2]);
    int blk_size   = atoi(argv[3]);
    unsigned int num_processors = atoi(argv[4]);/*1, 2, 4, 8*/
    int protocol   = atoi(argv[5]);	 /*0:MSI, 1:MESI, 2:Dragon*/
    char *fname =  (char *)malloc(20);
    fname = argv[6];
    string filename = "";

    //Printing the initial values, as expected in the .val files provided
    cout<<"===== 506 Personal information =====\n";
    cout<<"FirstName: Ram"<<"\t"<<"LastName: Chavali\n";
    cout<<"UnityID: rlchaval\n"<<"ECE492 Students? NO\n";

    //*******print out simulator configuration here*******//

    cout<<"===== 506 SMP Simulator configuration =====\n";
    cout<<"L1_SIZE:"<<cache_size<<"\n";
    cout<<"L1_ASSOC:"<<cache_assoc<<"\n";
    cout<<"L1_BLOCKSIZE:"<<blk_size<<"\n";
    cout<<"NUMBER OF PROCESSORS:"<<num_processors<<"\n";
    switch(protocol)                                                //check which protocol is to be implemented
    {
        case 0: cout<<"COHERENCE PROTOCOL: MSI";
            cout << "\nTRACE FILE: ";
            while(*fname != '\0') {
		filename = filename + *fname;
                cout <<*fname++;
            }
            break;
        case 1: cout<<"COHERENCE PROTOCOL: MESI";
            cout << "\nTRACE FILE: ";
            while(*fname != '\0') {
		filename = filename + *fname;
                cout<< *fname++;
            }
            break;
        case 2: cout<<"COHERENCE PROTOCOL: DRAGON";
            cout<<"\nTRACE FILE: ";
            while(*fname != '\0') {
		filename = filename + *fname;
                cout<< *fname++;
            }
            break;
        default: cout<<"Wrong Protocol argument passed. Please enter 0 for MSI, 1 for MESI and 2 for DRAGON protocol simulation.";
            printf("input format: ");
            printf("./smp_cache <cache_size> <assoc> <block_size> <num_processors> <protocol> <trace_file> \n");
            exit(0);
    }
   // total_processors = num_processors;
    //*****create an array of caches here**********//

    Cache **processor_cache = new Cache *[num_processors];
    for(unsigned int i=0;i<num_processors;i++)
    {
        processor_cache[i] = new Cache(cache_size, cache_assoc, blk_size,num_processors,processor_cache);
    }
    //**read trace file,line by line,each(processor#,operation,address)**//
    pFile = fopen (filename.c_str(),"r");
    if(pFile == NULL)
    {
        printf("\nTrace file problem\n");
        exit(0);
    }
    unsigned long int processor_num;
    unsigned long int processor_req_addr;
    char operation[2];
    while(fscanf(pFile, "%lu %s %lx", &processor_num, operation, &processor_req_addr) != EOF)
    {
        switch(protocol)                                                //check which protocol is to be implemented
        {
            case 0: processor_cache[processor_num] -> MSI_Access(processor_num, processor_req_addr, operation);
                break;
            case 1: processor_cache[processor_num] -> MESI_Access(processor_num, processor_req_addr, operation);
                break;
            case 2: processor_cache[processor_num] -> Dragon_Access(processor_num, processor_req_addr, operation);
                break;
        }
    }

    fclose(pFile);

    for(unsigned int i=0;i<num_processors;i++)
    {
        cout << "\n============ Simulation results (Cache " << i << ") ============" <<  endl;

        ulong memory_transactions;

        if ( protocol == 0 ) {
            memory_transactions = processor_cache[i]->getmemory_transactions() + processor_cache[i]->getRM() +
                                  processor_cache[i]->getWM() + processor_cache[i]->getWB();
            processor_cache[i]->set_memory_transactions(memory_transactions);
        }

        else if ( protocol == 1 ) {
            memory_transactions =
                    processor_cache[i]->getRM() + processor_cache[i]->getWM() + processor_cache[i]->getWB() -
                    processor_cache[i]->getcache_to_cache();
            processor_cache[i]->set_memory_transactions(memory_transactions);
        }

        else if ( protocol == 2 ) {
            memory_transactions =
                    processor_cache[i]->getRM() + processor_cache[i]->getWM() + processor_cache[i]->getWB();
            processor_cache[i]->set_memory_transactions(memory_transactions);
        }

        processor_cache[i]->printStats();
    }
}
