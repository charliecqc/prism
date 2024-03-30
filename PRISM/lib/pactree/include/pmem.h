// SPDX-FileCopyrightText: Copyright (c) 2019-2021 Virginia Tech
// SPDX-License-Identifier: Apache-2.0
#ifndef PACTREE_PMEM_H
#define PACTREE_PMEM_H

#include <libpmemobj.h>
#include <iostream>
#include <unistd.h>
#include "arch.h"

#define MASK 0x8000FFFFFFFFFFFF
#define MASK_DIRTY 0xDFFFFFFFFFFFFFFF //DIRTY_BIT
#define MASK_POOL 0x7FFFFFFFFFFFFFFF

//#define MEM_AMOUNT
typedef struct root_obj{
	PMEMoid ptr[2];
	//    PMEMoid ptr2;
}root_obj;
void printMemAmount();
void addMemAmount(unsigned long amt);
void zeroMemAmount();

class PMem {
	private:
		static void *baseAddresses[6]; // dram
		static void* logVaddr[2];
		//	static PMEMoid logSpace[4];
	public:
		static void *getBaseOf(int poolId) {
			return baseAddresses[poolId];
		}
		static bool alloc(int poolId, size_t size, void **p) {
			
			/**p = malloc(size);

			if(*p == nullptr){
				std::cerr<<"alloc error"<<std::endl;	
				return false;
			}
			return true;*/	
			
			
			// allocate a size memory from pool_id
			// and store/persist address to *p
			// return true on succeed
			PMEMobjpool *pop = (PMEMobjpool *)baseAddresses[poolId];            
			PMEMoid oid;
			int ret = pmemobj_alloc(pop, &oid, size, 0, NULL, NULL);

			//printf("%d\n", size); // YJ

			if(ret){
				std::cerr<<"alloc error"<<std::endl;	
				return false;
			}
			//DEBUG
			*p= reinterpret_cast<void*> (((unsigned long)poolId) << 48 | oid.off);
			return true;
		}

		static bool dram_alloc(int poolId, size_t size, void **p) {
			*p = malloc(size);

			if(*p == nullptr){
				std::cerr<<"alloc error"<<std::endl;	
				return false;
			}
			return true;
		}

		static bool alloc(int poolId, size_t size, void **p, PMEMoid *oid) {
			/**p = malloc(size);

			if(*p == nullptr){
				std::cerr<<"alloc error"<<std::endl;	
				return false;
			}
			return true;*/
			
			// allocate a size memory from pool_id
			// and store/persist address to *p
			// return true on succeed
			PMEMobjpool *pop = (PMEMobjpool *)baseAddresses[poolId];            
			int ret = pmemobj_alloc(pop, oid, size, 0, NULL, NULL);

			//printf("%d\n", size); // YJ

			if(ret){
				std::cerr<<"alloc error"<<std::endl;	
				return false;
			}
			//DEBUG
			*p= reinterpret_cast<void*> (((unsigned long)poolId) << 48 | oid->off);
			return true;
		}

		static bool dram_alloc(int poolId, size_t size, void **p, PMEMoid *oid) {
			*p = malloc(size);

			if(*p == nullptr){
				std::cerr<<"alloc error"<<std::endl;	
				return false;
			}
			return true;
		}

		static void free(void *pptr) {
			// p -> pool_id and offset
			// then perform free
			int poolId = (((unsigned long)pptr)&MASK_POOL) >> 48;
			void *rawPtr = (void *)(((unsigned long)pptr)& MASK + (unsigned long)baseAddresses[poolId]);
			PMEMoid ptr = pmemobj_oid(rawPtr);
			pmemobj_free(&ptr);
		}

		static void dram_free(void *pptr) {
			free(pptr);
		}

		static void freeVaddr(void *vaddr) {
			// p -> pool_id and offset
			// then perform free
			PMEMoid ptr = pmemobj_oid(vaddr);
			pmemobj_free(&ptr);
		}

		// case 1
		static bool bind(int poolId, const char *nvm_path, size_t size, void **root_p, int *is_created) {
			// open nvm_path with PMDK api and associate
			PMEMobjpool *pop;
			const char* layout = "phydra";
			if (access(nvm_path, F_OK) != 0) {
				pop = pmemobj_create(nvm_path, layout, size, 0666);
				if(pop == nullptr){
					std::cerr<<"bind create error "<<"path : "<<nvm_path<<std::endl;	
					return false;
				}
				baseAddresses[poolId] = reinterpret_cast<void*>(pop);
				*is_created = 1;
			}
			else{
				pop = pmemobj_open(nvm_path,layout); 
				if(pop == nullptr){
					std::cerr<<"bind open error"<<std::endl;	
					std::cerr<<"pooId: "<<poolId<<" nvm_path : "<<nvm_path<<std::endl;	
					return false;
				}
				baseAddresses[poolId] = reinterpret_cast<void*>(pop);
			}
			PMEMoid g_root = pmemobj_root(pop, sizeof(root_obj));
			*root_p=(root_obj*)pmemobj_direct(g_root);
			zeroMemAmount();
			return true;

		}
		static bool unbind(int poolId) {
			PMEMobjpool *pop = reinterpret_cast<PMEMobjpool *>(baseAddresses[poolId]);
			pmemobj_close(pop);
			return true;
		}

		static bool dram_bind(int poolId, size_t size, void **root_p, int *is_created) {
			// Allocate memory for root object in DRAM
			root_obj* root = static_cast<root_obj*>(malloc(sizeof(root_obj)));
    		if (root == nullptr) {
        		std::cerr << "Failed to allocate memory for root object" << std::endl;
        		return false;
    		}

			//assign root pointer
			*root_p = root;
			*is_created = 1;

			baseAddresses[poolId] = reinterpret_cast<void*>(root);

			// zero initialize memory
			memset(root, 0, sizeof(root_obj));

			return true;

		}
		static bool dram_unbind(int poolId) {
			//nothing to do for unbinding from
			return true;
		}

		static bool bindLog(int poolId, const char *nvm_path, size_t size) {
			PMEMobjpool *pop;
			int ret;

			if (access(nvm_path, F_OK) != 0) {
				pop = pmemobj_create(nvm_path, POBJ_LAYOUT_NAME(nv), size, 0666);
				if(pop == nullptr){
					std::cerr<<"bind create error"<<std::endl;	
					return false;
				}
				baseAddresses[poolId*3+2] = reinterpret_cast<void*>(pop);
				PMEMoid g_root = pmemobj_root(pop, sizeof(PMEMoid));
				//       PMEMoid g_root = pmemobj_root(pop, 64UL*1024UL*1024UL*1024UL);
				int ret = pmemobj_alloc(pop, &g_root, 1UL*512UL*1024UL*1024UL, 0, NULL, NULL);
				if(ret){
					std::cerr<<"!!! alloc error"<<std::endl;	
					return false;
				}
				logVaddr[poolId] = pmemobj_direct(g_root);
				memset((void*)logVaddr[poolId],0,1UL*512UL*1024UL*1024UL);
			}
			else{
				//TODO FIX IT
				pop = pmemobj_open(nvm_path, POBJ_LAYOUT_NAME(nv));
				if(pop == nullptr){
					std::cerr<<"bind log open error"<<std::endl;	
					return false;
				}
				PMEMoid g_root = pmemobj_root(pop, sizeof(PMEMoid));
				baseAddresses[poolId*3+2] = reinterpret_cast<void*>(pop);
				logVaddr[poolId] = pmemobj_direct(g_root);
			}
			return true;
		}

		static bool dram_bindLog(int poolId, size_t size) {
			// allocate memory for log in DRAM
			void* log_memory = malloc(size);
    		if (log_memory == nullptr) {
        		std::cerr << "Failed to allocate memory for log" << std::endl;
        		return false;
    		}

    		baseAddresses[poolId * 3 + 2] = log_memory;
    		logVaddr[poolId] = log_memory;
    		return true;
		}

		

		static bool dram_unbindLog(int poolId) {
			//free the allocated memory
			free(baseAddresses[poolId * 3 + 2]);
			return true;
		}



		static bool unbindLog(int poolId) {
			PMEMobjpool *pop = reinterpret_cast<PMEMobjpool *>(baseAddresses[poolId*3+2]);
			pmemobj_close(pop);
			return true;
		}

		static void* getOpLog(int i){
			unsigned long vaddr = (unsigned long)logVaddr[0];
			//printf("vaddr :%p %p\n",vaddr, (void *)(vaddr+(64*i)));

			return (void*)(vaddr + (64*i));
		}

};

static inline void flushToNVM(char *data, size_t sz){
	volatile char *ptr = (char *)((unsigned long)data & ~(L1_CACHE_BYTES- 1));
	for (; ptr < data + sz; ptr += L1_CACHE_BYTES) {
		clwb(ptr);
	}

	//printf("flushToNVM %d\n", sz);
}

#endif