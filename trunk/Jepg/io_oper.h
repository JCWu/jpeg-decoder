/** io_oper
*	 ‰»Î≥ÈœÛ≤„
*/

#ifndef IO_OPER_H_
#define IO_OPER_H_

#include <string>

class io_oper
{
public:
	virtual ~io_oper() = 0;
	virtual void Seek(unsigned int offset) = 0;
	virtual unsigned int Read(void *buffer, unsigned int size) = 0;
	virtual unsigned int Tell() const = 0;
	virtual unsigned int GetSize() const = 0;
};

io_oper* IStreamFromFile(std::wstring fname);

#endif
