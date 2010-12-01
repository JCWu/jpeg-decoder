#ifndef __BIT_STREAM__
#define __BIT_STREAM__

#include "io_oper.h"

struct bitstream;

bitstream* create_bit_stream(io_oper* io);
void release_bis_stream(bitstream* bit);
int getbit(bitstream * stream);
int check_rst(bitstream *stream);

#endif
