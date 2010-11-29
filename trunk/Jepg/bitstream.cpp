#include "bitstream.h"

struct bitstream 
{
	io_oper* io;
	int rem;
	unsigned char word;
};

bitstream* create_bit_stream(io_oper* io) {
	bitstream * st = (bitstream*) malloc(sizeof(bitstream));
	memset(st, 0, sizeof(bitstream));
	st->io = io;
	return st;
}

void release_bis_stream(bitstream* bit) {
	free(bit);
}

int getbit(bitstream * stream) {
	if (! stream->rem ) {
		stream->rem = stream->io->Read(&stream->word, 1) * 8 ;
	}
	--stream->rem;
	int res = (stream->word >> stream->rem) & 1;
	return res;
}
