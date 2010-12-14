#include "io_oper.h"

#include <windows.h>
#include <string>
#include <exception>

#include <cstring>

using namespace std;

io_oper::~io_oper() {

}

class BufferOper : public io_oper
{
public:
	BufferOper(wstring fname);
	BufferOper(unsigned char* buf, unsigned int size);
	~BufferOper();
	void Seek(unsigned int offset);
	unsigned int Read(void *buffer, unsigned int size);
	unsigned int GetSize() const;
	unsigned int Tell() const;
private:
	unsigned int pointer;
	unsigned char* buf;
	unsigned int size;
};

struct Exception : public std::exception {
public:
	Exception(const char* str) : msg(str) {
	}
	~Exception() throw(){
	}
	const char* what() {
		return msg.c_str();
	}
private:
	std::string msg;
};

BufferOper::BufferOper(wstring fname) {
	HANDLE hFile = CreateFileW(fname.c_str(), GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL,0);
	if (hFile == INVALID_HANDLE_VALUE) {
		throw Exception("failed open file.");
	}
	size = GetFileSize(hFile, NULL);
	buf = new unsigned char [ size ];
	unsigned int s = 0;
	while (s < size) {
		DWORD o;
		BOOL flag = ReadFile(hFile, buf + s, size - s, &o, NULL);
		if (!flag) {
			CloseHandle(hFile);
			throw Exception("failed reading file.");
		}
		s += o;
	}
	CloseHandle(hFile);
}

BufferOper::~BufferOper() {
	delete [] buf;
}

BufferOper::BufferOper(unsigned char* buf, unsigned int size) {
	this->buf = buf;
	this->size = size;
}

void BufferOper::Seek(unsigned int offset) {
	if (offset >= 0 && offset < size) {
		pointer = offset;
	}
}

unsigned int BufferOper::Read(void *buffer, unsigned int size) {
	if (size > this->size - pointer) {
		size = this->size - pointer;
	}
	memcpy(buffer, buf + pointer, size);
	pointer += size;
	return size;
}

unsigned int BufferOper::GetSize() const {
	return size;
}

unsigned int BufferOper::Tell() const {
	return pointer;
}

io_oper* IStreamFromFile(std::wstring fname) {
	BufferOper* ins = new BufferOper(fname);
	return static_cast<io_oper*>(ins);
}
