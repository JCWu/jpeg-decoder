#include "jpeg_decoder.h"

__inline void skip(io_oper* st, int count) {
	st->Seek(st->Tell() + count);
}

__inline unsigned char get8(io_oper* st) {
	unsigned char res = 0;
	st->Read(&res, 1);
	return res;
}

__inline unsigned short get16be(io_oper* st) {
	unsigned short res = 0;
	st->Read(&res, 2);
	res = (res >> 8) + ((res & 0xff) << 8);
	return res;
}

__inline unsigned short get16le(io_oper* st) {
	unsigned short res = 0;
	st->Read(&res, 2);
	return res;
}

__inline unsigned int get32be(io_oper* st) {
	unsigned int res;
	res = get16be(st) + (get16be(st) << 16);
	return res;
}

bool CheckJpeg(io_oper* st) {
	st->Seek(0);
	return get8(st) == 0xff && get8(st) == 0xd8;
}

const static int zig_order[8][8] = {
	{0, 1, 5, 6, 14, 15, 27, 28},
	{2, 4, 7, 13, 16, 26, 29, 42},
	{3, 8, 12, 17, 25, 30, 41, 43},
	{9, 11, 18, 24, 31, 40, 44, 53},
	{10, 19, 23, 32, 39, 45, 52, 54},
	{20, 22, 33, 38, 46, 51, 55, 60},
	{21, 34, 37, 47, 50, 56, 59, 61},
	{35, 36, 48, 49, 57, 58, 62, 63}
};

struct huffman_table
{
	unsigned char size[16];
	int count;
	unsigned code[256];
};

typedef unsigned short qtab[64];

struct comp_info
{
	int type;
	int qt_id;
	int ac_table;
	int dc_table;
};

struct jpeg
{
	int w, h;
	int type;

	int dri;

	comp_info comp[3];
	qtab qt[3];
	huffman_table ac_huff[4], dc_huff[4];
};



static unsigned char get_section(io_oper* st) {
	while (get8(st) != 0xff) ;
	//get8(st);
	return get8(st);
}

static void skip_section(io_oper* st) {
	skip(st, get16be(st) - 2);
}

static void read_header(jpeg* j, io_oper* st) {
	int end = st->Tell(); end += get16be(st);
	get8(st);
	j->h = get16be(st);
	j->w = get16be(st);
	int t = get8(st); // comp ÊýÁ¿
	for (int i = 0; i < 3; ++i) {
		int id = get8(st);
		j->comp[id-1].type = get8(st);
		j->comp[id-1].qt_id = get8(st);
	}
	st->Seek(end);
	printf("w=%d h=%d t=%d\n", j->w, j->h, t);
}

static void read_huffman(jpeg* j, io_oper* st) {
	printf("<<DHT>>\n");
	int end = st->Tell(); end += get16be(st);
	while (st->Tell() < end) {
		unsigned char state = get8(st);
		int id = state & 15;
		int ac = ((state >> 4) & 1);
		huffman_table * table_ptr;
		if (ac) table_ptr = &j->ac_huff[id]; else table_ptr = &j->dc_huff[id];
		huffman_table &ht = *table_ptr;
		
		ht.count = 0;
		for (int i = 0; i < 16; ++i) {
			ht.size[i] = get8(st);
			ht.count += ht.size[i];
		}
		for (int i = 0; i < ht.count; ++i)
			ht.code[i] = get8(st);

	}
	st->Seek(end);
}

static void read_sos(jpeg* j, io_oper* st) {
	printf("<<SOS>>\n");
	int end = st->Tell(); end += get16be(st);
	get8(st);
	for (int i = 0; i < 3; ++i) {
		int t = get8(st);
		unsigned char tb = get8(st);
		j->comp[t-1].ac_table = tb & 15;
		j->comp[t-1].dc_table = tb >> 4;
	}
	st->Seek(end);
}

static void read_dqt(jpeg* j, io_oper* st) {
	printf("<<DQT>>\n");
	int end = st->Tell(); end += get16be(st);
	while (st->Tell() < end) {
		unsigned char state = get8(st);
		int id = state & 15;
		int is16 = state >> 4;
		qtab &qt = j->qt[id];
		if (is16) {
			for (int i = 0; i < 64; ++i)
				qt[i] = get16le(st);
		} else {
			for (int i = 0; i < 64; ++i)
				qt[i] = get8(st);
		}

		printf("table id: %d\n", id);
		for (int i = 0; i < 8; ++i) {
			for (int j = 0; j < 8; ++j)
				printf("%4d ", qt[zig_order[i][j]]);
			printf("\n");
		}
	}
	st->Seek(end);
}

static void read_dri(jpeg* j, io_oper* st) {
	printf("<<DRI>>\n");
	int end = st->Tell(); end += get16be(st);
	j->dri = get16be(st);
	st->Seek(end);
}

void * DecodeJpeg(io_oper* st, int &w, int &h, int &dep, int req_comp) {
	dep = 3;
	if (!CheckJpeg(st)) return 0;
	st->Seek(2);
	int done = 0;
	jpeg * j = (jpeg*) malloc(sizeof(jpeg));
	memset(j, 0, sizeof(jpeg));
	while (! done ) {
		unsigned char sc = get_section(st);
		switch (sc) {
		case 0xc0: read_header(j, st); break; // baseline
		case 0xc1: return 0;
		case 0xc4: read_huffman(j, st); break; // huffman table
		case 0xc8: return 0;
		case 0xda: // SOS
			read_sos(j, st); 
			done = true;
			break; 
		case 0xdb: read_dqt(j, st); break; // DQT
		case 0xdd: read_dri(j, st); break; // DRI
		default: skip_section(st);
		}
	}

	for (int i = 0; i < 3; ++i) {
		printf("comp%d : qt = %d ac = %d dc = %d\n", i, j->comp[i].qt_id, j->comp[i].ac_table, j->comp[i].dc_table);
	}

	free(j);
	return 0;
}
