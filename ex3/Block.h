/*
 * Block.h
 */

#ifndef BLOCK_H_
#define BLOCK_H_

#include "hash.h"
#include <stdlib.h>

class Block
{
public:
	/*
	 * Constructs a block.
	 */
	Block(char* data, int index, int fatherIndex, int depth, int length);

	/*
	 * Destructor.
	 */
	~Block();
	char* getData() const;
	void setData(char* data);
	int getFatherIndex() const;
	void setFatherIndex(int fatherIndex);
	char* getHashedData() const;
	void setHashedData(char* hashedData);
	int getIndex() const;
	void setIndex(int index);
	int getDepth() const;
	void setDepth(int length);
	Block* getNext();
	void setNext(Block* next);
	bool getIsInLongest() const;
	void setIsInLongest(bool isInLongest);
	int getLenData() const;
	void setLenData(int lenData);
	bool isAttachToLongest() const;
	void setAttachToLongest(bool attachToLongest);

private:
	char* _data;  // The data of this block.
	int _index;  // Contains the hash value of the father.
	int _fatherIndex;
	int _depth;
	int _lenData;
	char* _hashedData;  // The hashed data of this block.
	Block* _next;
	bool _isInLongest;  // using only in prune function
	bool _attachToLongest;
};



#endif /* BLOCK_H_ */
