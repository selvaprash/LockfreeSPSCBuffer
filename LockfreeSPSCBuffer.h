#pragma once
#include <atomic>
#include <iostream>
#include <vector>
#include <fstream>
#include <new>
#include <utility>
/*
USAGE:
create object with same 1K MTU so buffersize=1 and speed say 10mbps so  dataspeed 10000000
GetCircBufferBasePtr()

from buffer writer thread
GetCircularBufferWriteOffset()
IncrementCircularBufferWriteOffset(no of bytes written in buf)
(write data)
once file is over
SetFileComplete()

from buffer reader thread
GetCircularBufferReadSize() equal to or lesser than MTU bytes
GetCircularBufferReadOffset()
read data  from buffer
SetCircularBufferReadOffset()

finally destroy object.
DestroyCB();
*/
//This can be instantiated like 
//circular_buffer_template<TSPKT_st, 4096, BufferAllocUsingNew> circular_buffer_instance;
template <class T, unsigned int align>class BufferAllocUsingNew;

template< class T, 
		  unsigned int align, 
		  template<class,unsigned int> class BufferAllocPolicy= BufferAllocUsingNew>
class LockfreeSPSCBuffer : public BufferAllocPolicy<T, align>
{
public:
	//		typedef boost::mutex::scoped_lock lock; 
	//		bounded_buffer(int n) : begin(0), end(0), buffered(0), circular_buf(n) { }
		LockfreeSPSCBuffer(int NoofTUnits) :begin(0), end(0), sizeinTUnits(NoofTUnits){
		unsigned int sizeinbytes;
		sizeinbytes = NoofTUnits*sizeof(T);

		try {
			circular_buffer = this->BufferAlloc(sizeinbytes);

		}
		catch (std::bad_alloc& ba) {

			std::cerr << "allocation failed for size " << sizeinbytes << "\n" << ba.what() <<std::endl;

		}

		buffered.store(0, std::memory_order_relaxed);
		baseptr = GetCircBufferBasePtr();
		eos_ = false;
		writebusy = false;
		readbusy = false;
	}
	//circular_buffer_template(const circular_buffer_template&) = delete;
	//circular_buffer_template& operator= (const circular_buffer_template&) = delete;
	bool AquireWritePtr(T*& writeptr) {
		auto index = GetCircularBufferWriteOffset();
		if (index == -1)return false;
		if (writebusy) {
			std::cout << "you have to release the write pointer" << "\n";
			return false;
		}
		writeptr = index + baseptr;
		writebusy = true;
		return true;
	}
	bool AquireWritePtr(std::pair<T*, int>&wrinfo) {
		int noofTunitstowr;	
		auto index = GetCircularBufferWriteOffset(noofTunitstowr);
		if(index == -1)
			return false;
		if(writebusy) {		
			std::cout << "you have to release the write pointer" << "\n";
			return false;
		}
		wrinfo.first=index + baseptr;
		wrinfo.second=noofTunitstowr;
		writebusy = true;
		return true;
	}
	void ReleaseWritePtr(int objswritten) {
		if (!writebusy) {
			std::cout << "aquire writeptr before releasing" << "\n";
			return;
		}
		IncrementCircularBufferWriteOffset(objswritten);
		writebusy = false;
	}
	bool AquireReadPtr(std::pair<T*, int>& rinfo) {
		auto rsize = GetCircularBufferReadSize();
		auto roffset = GetCircularBufferReadOffset();
		if (rsize == 0)return false;
		if (readbusy) {
			std::cout << "you have to release readptr" << "\n";
			return false;
		}
		rinfo.first = roffset + baseptr;
		rinfo.second = rsize;
		readbusy = true;
		return true;
	}
	void ReleaseReadPtr(int objsread) {
		if (!readbusy) {
			std::cout << "you have to first aquire read ptr" << "\n";
			return;
		}
		SetCircularBufferReadOffset(objsread);
		readbusy = false;
	}
	void ResetCircularBuffer(){
		buffered.store(0, std::memory_order_relaxed);
		begin = 0;
		end = 0;
	}
	void SetEOS() {
		eos_ = true;
	}
	bool GetEOS() {
		return eos_;
	}
	~LockfreeSPSCBuffer() {
		//		delete [] circular_buffer;
		//cout << "circ_buf template Object is destroyed" << endl;
		DestroyCB();
	}
private:
	T* GetCircBufferBasePtr(){
		return circular_buffer;
	}
	int GetCircularBufferWriteOffset(){//0=means buffer is full overflow condition
		int temp;
		temp = buffered.load(std::memory_order_acquire);
		if (temp == sizeinTUnits)
			return -1;
		else
			return end;
	}
	int GetCircularBufferWriteOffset(int& NoOfTUnitsEmpty){

		int temp;
		temp = buffered.load(std::memory_order_acquire);
		if (temp == sizeinTUnits)
			return -1;
		else{
			NoOfTUnitsEmpty = sizeinTUnits-temp; 
			if((NoOfTUnitsEmpty + end) > sizeinTUnits){ //readoffset < writeoffset return T units upto end of buffer
				NoOfTUnitsEmpty=sizeinTUnits-end;
			}	
			return end;
		}

	}		
	void IncrementCircularBufferWriteOffset(int temp){ //temp->no of T units to advance the write pointer with.
		end = (end + temp) % sizeinTUnits; //sizeinTUnits
		buffered.fetch_add(temp, std::memory_order_relaxed);
		//	cout<<"end::"<<end<<std::endl;
	}
	int GetCircularBufferReadSize(){ //if retval 0-: underflow condition
		int temp;
		//int sizetosend = 0;
		temp = buffered.load(std::memory_order_consume);
		if((temp + begin) > sizeinTUnits){

			temp = sizeinTUnits-begin;

		}	
		return temp;
	#if 0	
		if ((temp != 0) || (filereadover == 1)) {
			sizetosend = TS_DATA_PER_MTU;
			if (temp < TS_DATA_PER_MTU)
				sizetosend = temp;
		}

		return sizetosend;
	#endif	
	}
	int GetCircularBufferReadOffset(){
		return begin;
	}
	void SetCircularBufferReadOffset(int TunitsConsumed){
		begin = (begin + TunitsConsumed) % sizeinTUnits;
		buffered.fetch_sub(TunitsConsumed, std::memory_order_relaxed);
	}
	int getsize(){
		return sizeinTUnits;
	}
	void DestroyCB(){
		//std::cout << "Buffer Destroyed" << std::endl;
		this->BufferRelease(circular_buffer);
	}

	T* circular_buffer;
	int sizeinTUnits;
	std::atomic<int> buffered;
	std::atomic<bool> eos_;
	bool writebusy;
	bool readbusy;
	T* baseptr;
	int begin, end;
};

//allocation policy class 

template <class T, unsigned int align>
class BufferAllocUsingNew
{
public:

	T* BufferAlloc(unsigned int size)
	{
		unsigned int minallocvalue;
		unsigned int nooflcmchunks;
		unsigned int sizeofT = sizeof(T);
		minallocvalue = gcd(align, sizeofT); //
		minallocvalue = (align*sizeofT) / minallocvalue; //lcm

		if (size%minallocvalue) {

			nooflcmchunks = size / minallocvalue;

			size = minallocvalue*(nooflcmchunks + 1);

		}

		SizeAllocated = size;

		return new T[size / sizeofT];

	}

	int BufferRelease(T* ptr)
	{


		delete[] ptr;
		return 0;

	}
private:
	unsigned int gcd(unsigned int x, unsigned int y)
	{
		if (x == 0) {
			return y;
		}

		while (y != 0) {
			if (x > y) {
				x = x - y;
			}
			else {
				y = y - x;
			}
		}

		return x;
	}

	unsigned long SizeAllocated;
};

