/*
  Copyright (C) 2015 CS416/CS516

  This program can be distributed under the terms of the GNU GPLv3.
  See the file COPYING.
*/

#ifndef _BLOCK_H_
#define _BLOCK_H_

#define BLOCK_SIZE 4096

struct block {
	int block_num;
	int free;
	struct block *next;
};

void disk_open(const char* diskfile_path);
void disk_close();
int block_read(const int block_num, void *buf, size_t size);
int block_write(const int block_num, const void *buf, size_t size);

struct block *create_block();
struct block *get_free_block();

#endif
