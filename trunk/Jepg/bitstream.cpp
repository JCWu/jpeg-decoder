#include "bitstream.h"
#include <cstdlib>
#include <cstring>

struct bitstream 
{
	io_oper* io;
	int rem;
	unsigned char word;
	unsigned char flag;
};

bitstream* create_bit_stream(io_oper* io) {
	bitstream * st = (bitstream*) malloc(sizeof(bitstream));
	memset(st, 0, sizeof(bitstream));
	st->io = io;
	st->flag = 0xd0;
	return st;
}

void release_bis_stream(bitstream* bit) {
	free(bit);
}

int getbit(bitstream * stream) {
	if (! stream->rem ) {
		stream->rem = stream->io->Read(&stream->word, 1) * 8 ;
		if (stream->word == 0xff) {
			stream->io->Seek(stream->io->Tell() + 1);
		}
	}
	--stream->rem;
	int res = (stream->word >> stream->rem) & 1;
	return res;
}

int check_rst(bitstream *stream) {
	stream->io->Seek(stream->io->Tell() + 2);
	stream->rem = stream->io->Read(&stream->word, 1) * 8 ;
	return 1;
}
