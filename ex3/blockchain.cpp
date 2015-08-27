/*
 * blockchain.cpp
 * A multi threaded blockchain database manager
 *
 * Author: lior_13
 */

#include <stdio.h>
#include <iostream>
#include "blockchain.h"
#include <map>
#include <vector>
#include <algorithm>
#include "Block.h"
#include "hash.h"
#include <pthread.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdlib.h>
#include <signal.h>

#define SUCCESS 0
#define FAIL -1
#define UNSUCCESSFUL -2
#define NOT_FOUND 0
#define FOUND 1

using namespace std;

map<int, Block*> blockMap;
vector<Block*> blockQueue;
vector<int> vecIndex;

bool isInitiated = false;
bool currentlyBlocked = false;
int isInDaemon = -1;
int g_sizeCounter = 0;

unsigned long int daemonThread, closeThread;

pthread_mutex_t s_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mapMutex, queueMutex;


// ==========================================================================================


/**
 * Frees the memory of the mutexes
 */
void free_mutex()
{
	pthread_mutex_destroy(&mapMutex);
	pthread_mutex_destroy(&queueMutex);
}

/*
 * return the index in the vector if exists in the queue, or -1 in case it is not in the queue
 */
int existBlock(int blockIdx)
{
	int i = 0;
	for (vector<Block*>::iterator iter = blockQueue.begin(); iter != blockQueue.end(); ++iter)
	{
		if ((*iter)->getIndex() == blockIdx)
		{
			return i;
		}
		++i;
	}
	return FAIL;
}

bool compLength(Block* i, Block* j)
{
	return i->getDepth() < j->getDepth();
}

/*
 * Returns the father's index.
 * If the map is empty, -1 is returned.
 */
int findFather()
{
	if(blockMap.empty())
	{
		return FAIL;
	}
	int maxLength = max_element(blockMap.begin(), blockMap.end(),
			[](const pair<int, Block*>& p1, const pair<int, Block*>& p2){
		return p1.second->getDepth() < p2.second->getDepth();
	})->second->getDepth();

	//find all the indexes with same length
	vector<int> indexes;
	for(auto block : blockMap)
	{
		if(block.second->getDepth() == maxLength)
		{
			indexes.push_back(block.second->getIndex());
		}
	}
	// shuffle things a bit..
	random_shuffle(indexes.begin(), indexes.end());
	return indexes.at(0);
}

int findMinIdx()
{
	unsigned int idToReturn = 0;
	sort(vecIndex.begin(), vecIndex.end());
	if(!vecIndex.empty())
	{
		for(; idToReturn < vecIndex.size(); ++idToReturn)
		{
			if((int)idToReturn != vecIndex.at(idToReturn))
			{
				break;
			}
		}
	}
	return idToReturn;
}

int addAndHash(Block* block)
{
	isInDaemon = block->getIndex();
	int fatherIndex = block->getFatherIndex();
	if(block->isAttachToLongest())
	{
		fatherIndex = findFather();
		if (fatherIndex == -1)
		{
			// cerr << "not found" << endl;
			return FAIL;
		}
		block->setDepth(blockMap.at(fatherIndex)->getDepth() + 1);
	}
	block->setHashedData(generate_hash(
			block->getData(),
			block->getLenData(),
			generate_nonce(block->getIndex(), fatherIndex)));
	block->setFatherIndex(fatherIndex);
	if (!blockMap.count(fatherIndex))
	{ // The father have been delete due to a prune call
		delete(block);
	}
	else
	{
		block->setNext(blockMap.at(fatherIndex));
		pthread_mutex_lock(&mapMutex);
		blockMap.insert(pair<int, Block*>(block->getIndex(), block));
		pthread_mutex_unlock(&mapMutex);
	}
	return SUCCESS;
}

/*
 * returns the block with the blockIdx index, or NULL in case it is not in the queue
 */
Block* popBlock(int blockIdx)
{
	for (vector<Block*>::iterator iter = blockQueue.begin(); iter != blockQueue.end(); ++iter)
	{
		if ((*iter)->getIndex() == blockIdx)
		{
			Block* tmp = *iter;
			blockQueue.erase(iter);
			return tmp;
		}
	}
	return NULL;
}

void* daemonFunction(void* ptr)
{
	while (!currentlyBlocked)
	{
		if (!blockQueue.empty())
		{
		pthread_mutex_lock(&queueMutex);
			Block* bToHash = blockQueue.front();
			addAndHash(popBlock(bToHash->getIndex()));
			g_sizeCounter++;
		pthread_mutex_unlock(&queueMutex);
		}
	}
	return NULL;
}

void* closeChainThread(void* ptr)
{
	pthread_mutex_lock(&queueMutex);
	while(!blockQueue.empty())
	{
		Block* bToHash = blockQueue.front();
		addAndHash(popBlock(bToHash->getIndex()));
		cout << bToHash->getHashedData() << endl;
		cout<< "this thread is :" << bToHash->getIndex() << endl;
	}
	pthread_mutex_unlock(&queueMutex);
	for (auto block : blockMap)
	{
		delete(block.second);
	}
	blockMap.clear();
	vecIndex.clear();
	g_sizeCounter = 0;
	currentlyBlocked = false;
	isInitiated = false;
	free_mutex();
	close_hash_generator();
	pthread_mutex_unlock(&s_mutex);
	return NULL;
}

/*=================================================================================================
 * ======================================Library Functions=========================================
 * ================================================================================================
 */



/*
 * DESCRIPTION: This function initiates the Block chain, and creates the genesis Block.  The genesis Block does not hold any transaction data
 *      or hash.
 *      This function should be called prior to any other functions as a necessary precondition for their success (all other functions should
 *      return with an error otherwise).
 * RETURN VALUE: On success 0, otherwise -1.
 */
int init_blockchain()
{
	if (currentlyBlocked || isInitiated)
	{
		return FAIL;
	}
	//mutex lock
	pthread_mutex_lock(&s_mutex);
	if(pthread_create(&daemonThread, nullptr, &daemonFunction, nullptr))
	{
		//		cerr << "can't create thread" << endl;
		return FAIL;
	}
	Block* gensis = new Block(nullptr, 0, -1, 0, 0);
	blockMap.insert(pair<int, Block*>(gensis->getIndex(), gensis));
	vecIndex.push_back(gensis->getIndex());
	pthread_mutex_init(&mapMutex, NULL);
	pthread_mutex_init(&queueMutex, NULL);
	init_hash_generator();

	isInitiated = true;
	return SUCCESS;
}

/*
 * DESCRIPTION: Ultimately, the function adds the hash of the data to the Block chain.
 *      Since this is a non-blocking package, your implemented method should return as soon as possible, even before the Block was actually
 *      attached to the chain.
 *      Furthermore, the father Block should be determined before this function returns. The father Block should be the last Block of the
 *      current longest chain (arbitrary longest chain if there is more than one).
 *      Notice that once this call returns, the original data may be freed by the caller.
 * RETURN VALUE: On success, the function returns the lowest available block_num (> 0),
 *      which is assigned from now on to this individual piece of data.
 *      On failure, -1 will be returned.
 */
int add_block(char *data, int length)
{

	if(!isInitiated || currentlyBlocked)
	{
		return FAIL;
	}
	int idx = findMinIdx();
	pthread_mutex_lock(&mapMutex);
	int fatherIdx = findFather();
	pthread_mutex_unlock(&mapMutex);
	Block* newBlock = new Block(data, idx, fatherIdx, blockMap.at(fatherIdx)->getDepth() + 1, length);
	vecIndex.push_back(newBlock->getIndex());
	blockQueue.push_back(newBlock);
	return idx;
}

/*
 * DESCRIPTION: Without blocking, enforce the policy that this block_num should be attached to the longest chain at the time of attachment of
 *      the Block. For clearance, this is opposed to the original add_block that adds the Block to the longest chain during the time that
 *      add_block was called.
 *      The block_num is the assigned value that was previously returned by add_block.
 * RETURN VALUE: If block_num doesn't exist, return -2; In case of other errors, return -1; In case of success return 0; In case block_num is
 *      already attached return 1.
 */
int to_longest(int block_num)
{
	if(!isInitiated)
	{
		return FAIL;
	}
	if(blockMap.count(block_num))
	{
		return FOUND;
	}
	int idx = existBlock(block_num);
	if (idx == FAIL)
	{
		return UNSUCCESSFUL;
	}
	blockQueue.at(idx)->setAttachToLongest(true);
	return SUCCESS;
}

/*
 * DESCRIPTION: This function blocks all other Block attachments, until block_num is added to the chain. The block_num is the assigned value
 *      that was previously returned by add_block.
 * RETURN VALUE: If block_num doesn't exist, return -2;
 *      In case of other errors, return -1; In case of success or if it is already attached return 0.
 */
int attach_now(int block_num)
{
	if(!isInitiated || currentlyBlocked)
	{
		return FAIL;
	}
	if(blockMap.count(block_num))
	{
		return SUCCESS;
	}
	if(isInDaemon == block_num)
	{
		while(!blockMap.count(block_num));
		return SUCCESS;
	}
	int idx = existBlock(block_num);
	if(idx == FAIL)
	{
		return UNSUCCESSFUL;
	}
	blockQueue.insert(blockQueue.begin(), popBlock(block_num));
	//TODO: lior, i added this. check it out! it's suppose to wait until this block is added (like the description says!
	while(!blockMap.count(block_num));
	return SUCCESS;
}

/*
 * DESCRIPTION: Without blocking, check whether block_num was added to the chain.
 *      The block_num is the assigned value that was previously returned by add_block.
 * RETURN VALUE: 1 if true and 0 if false. If the block_num doesn't exist, return -2; In case of other errors, return -1.
 */
int was_added(int block_num)
{
	if(!isInitiated)
	{
		return FAIL;
	}
	if(blockMap.count(block_num))
	{
		// cerr << "not found" << endl;
		return FOUND;
	}
	if (existBlock(block_num) != FAIL || block_num == isInDaemon)
	{
		return NOT_FOUND;
	}
	else
	{
		return UNSUCCESSFUL;
	}
	return 1;
}

/*
 * DESCRIPTION: Return how many Blocks were attached to the chain since init_blockchain.
 *      If the chain was closed (by using close_chain) and then initialized (init_blockchain) again this function should return
 *      the new chain size.
 * RETURN VALUE: On success, the number of Blocks, otherwise -1.
 */
int chain_size()
{
	if(!isInitiated)
	{
		return FAIL;
	}
	return g_sizeCounter;
}

/*
 * DESCRIPTION: Search throughout the tree for sub-chains that are not the longest chain,
 *      detach them from the tree, free the blocks, and reuse the block_nums.
 * RETURN VALUE: On success 0, otherwise -1.
 */
int prune_chain()
{
	if(!isInitiated || currentlyBlocked)
	{
		return FAIL;
	}

	//	pthread_mutex_lock(&queueMutex);
	int longestIdx = findFather();
	while(NULL != blockMap.at(longestIdx)->getNext() )
	{
		blockMap.at(longestIdx)->setIsInLongest(true);
		longestIdx = blockMap.at(longestIdx)->getNext()->getIndex();
	}
	// also, don't forget our genesis, which its next is NULL, but we don't want to delete it!
	blockMap.at(0)->setIsInLongest(true);

	for(vector<int>::iterator iter = vecIndex.begin(); *(iter) != *(vecIndex.end());)
	{
		if(!blockMap.at(*iter)->getIsInLongest())
		{
			vecIndex.erase(iter);
		}
		else
		{
			++iter;
		}
	}
	pthread_mutex_lock(&mapMutex);
	for(map<int, Block*>::iterator iter = blockMap.begin(); *(iter) != *(blockMap.end());)
	{
		if(!(*iter).second->getIsInLongest())
		{
			blockMap.erase((*iter++).first);
		}
		else
		{
			++iter;
		}
	}
	pthread_mutex_unlock(&mapMutex);
	for(auto block : blockMap)
	{
		block.second->setIsInLongest(false);
	}
	//	pthread_mutex_unlock(&queueMutex);

	return SUCCESS;
}

/*
 * DESCRIPTION: Close the recent blockchain and reset the system, so that it is possible to call init_blockchain again. Non-blocking.
 *      All pending Blocks should be hashed and printed to terminal (stdout).
 *      Calls to library methods which try to alter the state of the Blockchain are prohibited while closing the Blockchain. e.g.: Calling
 *      chain_size() is ok, a call to prune_chain() should fail.
 *      In case of a system error, the function should cause the process to exit.
 */
void close_chain()
{
	if(isInitiated)
	{
		currentlyBlocked = true;
		int check = pthread_create(&closeThread, NULL, &closeChainThread, NULL);
		if(check != SUCCESS)
		{
//			cerr << "error" << endl;
		}
		//		//				cout << "queue size is         " << blockQueue.size() << endl;
		//		pthread_mutex_lock(&queueMutex);
		////		cout << "locking in closing\n";
		//		//		cout << "HERE QUEUE !!!!!\n";
		//		currentlyBlocked = true;
		//		while(!blockQueue.empty())
		//		{
		//			Block* bToHash = blockQueue.front();
		////			cout << "index in closing chain   " << bToHash->getIndex() << endl;
		//			addAndHash(popBlock(bToHash->getIndex()));
		//			cout << bToHash->getHashedData() << endl;
		//		}
		//		blockMap.clear();
	}
}

/*
 * DESCRIPTION: The function blocks and waits for close_chain to finish.
 * RETURN VALUE: If closing was successful, it returns 0.
 *      If close_chain was not called it should return -2. In case of other error, it should return -1.
 */
int return_on_close()
{
	if (!currentlyBlocked || !isInitiated)
	{
		return UNSUCCESSFUL;
	}

	pthread_join(closeThread, NULL);


//	currentlyBlocked = false;
//	isInitiated = false;
//	//	cout << "unlocking in close\n";
//	pthread_mutex_unlock(&queueMutex);
//	pthread_mutex_unlock(&s_mutex);
//	free_mutex();


	return SUCCESS;
}

/*=================================================================================================
 * ======================================Testing===================================================
 * ================================================================================================
 */

void printChain()
{
	for(map<int, Block*>::iterator block = blockMap.begin(); block != blockMap.end(); ++block)
	{
		if(!block->first)
		{
			continue;
		}
		cout << "the block is " << block->second->getIndex() << " and the father is " << block->second->getFatherIndex() << endl;
	}
}

