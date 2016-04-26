#include"LockfreeSPSCBuffer.h"
#include<iostream>
#include<string>
#include<thread>
#include<fstream>
#include<vector>

typedef LockfreeSPSCBuffer<char, 4096, BufferAllocUsingNew> LFBUFFER;

void producer(std::string infilestr, LFBUFFER& sb) {
	char* wptr;
	std::pair<char*, int>wrinfo;
	std::ifstream filein;
	filein.open(infilestr.c_str(), std::ios::binary);
	if (!filein.is_open()) {
		std::cout << "error in opening input file" << "\n";
		return;
	}

	while (!filein.eof()) {
		if (sb.AquireWritePtr(wrinfo)) {
			filein.read(wptr, 1024);
			sb.ReleaseWritePtr(filein.gcount());
		//	std::cout << "count = " << filein.gcount() << "\n";
		}
	}
	sb.SetEOS();
	filein.close();
}
void consumer(std::string ofilestr, LFBUFFER& sb) {
	std::pair<char*, int> rinfo;
	std::ofstream fout;
	fout.open(ofilestr.c_str(), std::ios::binary);
	if (!fout.is_open()) {
		std::cout << "error in opening output file" << "\n";
		return;
	}
	auto writesize = 0;
	while (!sb.GetEOS()) {
		if (sb.AquireReadPtr(rinfo)) {
			writesize = (rinfo.second>1024) ? 1024 : rinfo.second;
			fout.write(rinfo.first, writesize);
			sb.ReleaseReadPtr(writesize);
		}
	}
	fout.close();
}
int main(int argc, char* argv[]) {
	if (argc != 2) {
		std::cout << "Usage " << argv[0] << " <filename>" << "\n";
		return 0;
	}
	LFBUFFER sharedbuf(1024 * 1024);
	std::string outfile(argv[1]);
	outfile = outfile + "copy";
	
	std::thread t1(producer,argv[1], std::ref(sharedbuf));
	std::thread t2(consumer,outfile, std::ref(sharedbuf));
	t1.join();
	t2.join();
}
