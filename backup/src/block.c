/*
  Copyright (C) 2015 CS416/CS516

  This program can be distributed under the terms of the GNU GPLv3.
  See the file COPYING.
*/

#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "block.h"

int diskfile = -1;
struct block *block_head;
int current_block = 0;

struct block *create_block() {
	struct block *new_block = (struct block*) malloc(sizeof(struct block));
	new_block->block_num = current_block;
	new_block->free = 1;
	new_block->next = NULL;
	
	current_block++;
	
	return new_block;
}

struct block *get_free_block() {
	
	if (block_head == NULL) {
		block_head = create_block();
	}
	
	struct block *block_current = block_head;
	
	while (block_current->next != NULL) {
		if (block_current->free) {
			block_current->free = 0;
			return block_current;
		}
		block_current = block_current->next;
	}
	
	if (block_current->free) {
		block_current->free = 0;
		return block_current;
	}
	
	block_current->next = create_block();
	block_current->next->free = 0;
	return block_current->next;	
}

void disk_open(const char* diskfile_path)
{
    if(diskfile >= 0){
	return;
    }
    
    diskfile = open(diskfile_path, O_CREAT|O_RDWR, S_IRUSR|S_IWUSR);
    if (diskfile < 0) {
	perror("disk_open failed");
	exit(EXIT_FAILURE);
    }
}

void disk_close()
{
    if(diskfile >= 0){
	close(diskfile);
    }
}

/** Read a block from an open file
 *
 * Read should return (1) exactly @BLOCK_SIZE when succeeded, or (2) 0 when the requested block has never been touched before, or (3) a negtive value when failed. 
 * In cases of error or return value equals to 0, the content of the @buf is set to 0.
 */
int block_read(const int block_num, void *buf, size_t size)
{
    int retstat = 0;
    retstat = pread(diskfile, buf, size, block_num*BLOCK_SIZE);
    if (retstat <= 0){
	memset(buf, 0, BLOCK_SIZE);
	if(retstat<0)
	perror("block_read failed");
    }

    return retstat;
}

/** Write a block to an open file
 *
 * Write should return exactly @BLOCK_SIZE except on error. 
 */
int block_write(const int block_num, const void *buf, size_t size)
{
    int retstat = 0;
    retstat = pwrite(diskfile, buf, size, block_num*BLOCK_SIZE);
    if (retstat < 0)
	perror("block_write failed");
    
    return retstat;
}

