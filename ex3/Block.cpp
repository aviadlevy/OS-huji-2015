/*
 * Block.cpp
 */

#include "Block.h"
#include <cstring>

#include <iostream>

Block::Block(char* data, int index, int fatherIndex, int depth, int length)
{
	if(data != nullptr)
	{
		_data = new char[length];
		memcpy(_data, data, length);
	}
	else
	{
		_data = nullptr;
	}
	_index = index;
	_fatherIndex = fatherIndex;
	_depth = depth;
	_lenData = length;
	_hashedData = NULL;
	_next = NULL;
	_isInLongest = false;
	_attachToLongest = false;
}

Block::~Block()
{
	delete[] _data;
	delete[] _hashedData;
}

char* Block::getData() const {
	return _data;
}

void Block::setData(char* data) {
	_data = data;
}

int Block::getFatherIndex() const {
	return _fatherIndex;
}

void Block::setFatherIndex(int fatherIndex) {
	_fatherIndex = fatherIndex;
}

char* Block::getHashedData() const {
	return _hashedData;
}

void Block::setHashedData(char* hashedData) {
	_hashedData = hashedData;
}

int Block::getIndex() const {
	return _index;
}

void Block::setIndex(int index) {
	_index = index;
}

int Block::getDepth() const {
	return _depth;
}

void Block::setDepth(int length) {
	_depth = length;
}

Block* Block::getNext()
{
	return _next;
}

void Block::setNext(Block* next)
{
	_next = next;
}

bool Block::getIsInLongest() const {
	return _isInLongest;
}

void Block::setIsInLongest(bool isInLongest) {
	_isInLongest = isInLongest;
}

int Block::getLenData() const {
	return _lenData;
}

void Block::setLenData(int lenData) {
	_lenData = lenData;
}

bool Block::isAttachToLongest() const {
	return _attachToLongest;
}

void Block::setAttachToLongest(bool attachToLongest) {
	_attachToLongest = attachToLongest;
}
