/*
  Simple File System

  This code is derived from function prototypes found /usr/include/fuse/fuse.h
  Copyright (C) 2001-2007  Miklos Szeredi <miklos@szeredi.hu>
  His code is licensed under the LGPLv2.

*/

#include "params.h"
#include "block.h"

#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <fuse.h>
#include <libgen.h>
#include <limits.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>

// Extra system libraries
#include <time.h>

#ifdef HAVE_SYS_XATTR_H
#include <sys/xattr.h>
#endif

#include "log.h"

const char *diskfile_path;

// Structure to represent a file or directory
struct inode {
    char *path;
    struct stat *statbuf;
    struct inode *parent;
    struct inode *next;
	struct inode *child;
	struct block *blockk;
};

// Root directory
struct inode *inode_head;

// Create a new inode with the given parameters
struct inode *create_inode(const char *path, mode_t mode, int links, struct inode *parent) {
	struct inode *new_inode = (struct inode*) malloc(sizeof(struct inode));
	
	new_inode->path = malloc(strlen(path) + 1);
	strcpy(new_inode->path, path);
	
	new_inode->statbuf = (struct stat*) malloc(sizeof(struct stat));
	new_inode->statbuf->st_mode = mode | S_IRWXU;
	new_inode->statbuf->st_nlink = links;
	
	if (mode == S_IFREG) {
		new_inode->statbuf->st_size = 0;
	}

	new_inode->statbuf->st_atime = time(NULL);
	new_inode->statbuf->st_mtime = time(NULL);
	new_inode->statbuf->st_ctime = time(NULL);
	
	new_inode->parent = parent;
	new_inode->next = NULL;
	new_inode->child = NULL;
	new_inode->blockk = NULL;
	
	return new_inode;
}

// Search for an inode matching path
struct inode *find_inode(const char *path) {
	struct inode *inode_current = inode_head;
	
	// Search for the inode whose path matches the one requested
	while (inode_current != NULL) {
		
		// Found the node
		if (strcmp(path, inode_current->path) == 0) {
			return inode_current;
		}
		
		// Node is in directory
		if (S_ISDIR(inode_current->statbuf->st_mode)) {
			char *path_update = malloc(strlen(inode_current->path) + 1);
			strcpy(path_update, inode_current->path);
			path_update[strlen(inode_current->path)] = '/';
			path_update[strlen(inode_current->path + 1)] = '\0';
			log_msg("\n NEW PATH %s\n", path_update);
			
			if ((strncmp(inode_current->path, path, strlen(inode_current->path)) == 0)) {
				inode_current = inode_current->child;
			}
			
			else {
				inode_current = inode_current->next;
			}
		}
		
		else {
			inode_current = inode_current->next;
		}
	}
	
	return NULL;
}

// Finds the appropriate spot in the inode tree for the new inode and creates it
int insert_inode(const char *path, mode_t mode) {
	int retstat = -ENOENT;
	
	struct inode *inode_current = inode_head;
    
    while (1) {
		
		// Check child
		if ((S_ISDIR(inode_current->statbuf->st_mode)) &&
		    (strncmp(path, inode_current->path, strlen(inode_current->path)) == 0)) {
			
			// Create child
			if (inode_current->child == NULL) {
				inode_current->child = create_inode(path, mode, 1, inode_current);
				return 0;
			}
			
			// Move to child
			inode_current = inode_current->child;
			
		}
		
		// Create next
		if (inode_current->next == NULL) {
			inode_current->next = create_inode(path, mode, 1, inode_current);
			return 0;
			
		// Move to next
		} else {
			inode_current = inode_current->next;
		}
	}
	
	return retstat;
}

// Free inode's allocated memory
void free_inode(struct inode *inode_current) {
	if (inode_current->blockk != NULL) {
		inode_current->blockk->free = 1;
	}
	
	free(inode_current->path);
	free(inode_current->statbuf);
	free(inode_current);
}

// Searchs for an inode and deletes it
int delete_inode(const char *path) {	
	struct inode *inode_current = find_inode(path);

	if (inode_current == NULL) {
		return -1;
	}

	if (inode_current->parent->child != NULL && inode_current->parent->child - inode_current == 0) {
		inode_current->parent->child = inode_current->next;
	} else {
		inode_current->parent->next = inode_current->next;
	}

	// Update the parent
	if (inode_current->next != NULL) {
		inode_current->next->parent = inode_current->parent;
	}
    
    free_inode(inode_current);
    
    return 0;
}

// Copy the contents of source into dest
void swap(struct stat *source, struct stat *dest) {
	dest->st_mode = source->st_mode;
	dest->st_nlink = source->st_nlink;
	
	if (S_ISREG(source->st_mode)) {
		dest->st_size = source->st_size;
	}
	
	dest->st_atime = source->st_atime;
	dest->st_mtime = source->st_mtime;
	dest->st_ctime = source->st_ctime;
}

///////////////////////////////////////////////////////////
//
// Prototypes for all these functions, and the C-style comments,
// come indirectly from /usr/include/fuse.h
//

/**
 * Initialize filesystem
 *
 * The return value will passed in the private_data field of
 * fuse_context to all file operations and as a parameter to the
 * destroy() method.
 *
 * Introduced in version 2.3
 * Changed in version 2.6
 */
void *sfs_init(struct fuse_conn_info *conn)
{
    fprintf(stderr, "in bb-init\n");
    log_msg("\nsfs_init()\n");

    log_conn(conn);
    log_fuse_context(fuse_get_context());
    
    // Create the root directory
	inode_head = create_inode("/", S_IFDIR, 2, NULL);
	
    return SFS_DATA;
}

/**
 * Clean up filesystem
 *
 * Called on filesystem exit.
 *
 * Introduced in version 2.3
 */
void sfs_destroy(void *userdata)
{
    log_msg("\nsfs_destroy(userdata=0x%08x)\n", userdata);
}

/** Get file attributes.
 * Similar to stat().  The 'st_dev' and 'st_blksize' fields are
 * ignored.  The 'st_ino' field is ignored except if the 'use_ino'
 * mount option is given.
 */
int sfs_getattr(const char *path, struct stat *statbuf)
{
	// Error code for no such file or directory
    int retstat = -ENOENT;
 
    char fpath[PATH_MAX];
    
    log_msg("\nsfs_getattr(path=\"%s\", statbuf=0x%08x)\n",
	  path, statbuf);
    
    memset(statbuf, 0, sizeof(struct stat));

	struct inode *inode_current = find_inode(path);
	
	if (inode_current != NULL) {
		swap(inode_current->statbuf, statbuf);
		retstat = 0;
	}

    return retstat;
}

/**
 * Create and open a file
 *
 * If the file does not exist, first create it with the specified
 * mode, and then open it.
 *
 * If this method is not implemented or under Linux kernel
 * versions earlier than 2.6.15, the mknod() and open() methods
 * will be called instead.
 *
 * Introduced in version 2.5
 */
int sfs_create(const char *path, mode_t mode, struct fuse_file_info *fi)
{
    int retstat = 0;
    log_msg("\nsfs_create(path=\"%s\", mode=0%03o, fi=0x%08x)\n",
	    path, mode, fi);
   
    insert_inode(path, S_IFREG);
    
    return retstat;
}

/** Remove a file */
int sfs_unlink(const char *path)
{
    int retstat = 0;
    log_msg("sfs_unlink(path=\"%s\")\n", path);

	return delete_inode(path);
	
	/*
    struct inode *inode_current = find_inode(path);

	log_msg("\nComparing %p with %p\n", inode_current->parent->child, inode_current);
	if (inode_current->parent->child != NULL && inode_current->parent->child - inode_current == 0) {
		inode_current->parent->child = inode_current->next;
	} else {
		inode_current->parent->next = inode_current->next;
	}

	// Update the parent
	if (inode_current->next != NULL) {
		inode_current->next->parent = inode_current->parent;
	}
    
    free_inode(inode_current);
    return retstat;
    */
}

/** File open operation
 *
 * No creation, or truncation flags (O_CREAT, O_EXCL, O_TRUNC)
 * will be passed to open().  Open should check if the operation
 * is permitted for the given flags.  Optionally open may also
 * return an arbitrary filehandle in the fuse_file_info structure,
 * which will be passed to all file operations.
 *
 * Changed in version 2.2
 */
int sfs_open(const char *path, struct fuse_file_info *fi)
{
    int retstat = -EPERM;
    log_msg("\nsfs_open(path\"%s\", fi=0x%08x)\n",
	    path, fi);

	struct inode *inode_current = find_inode(path);
	mode_t mode = inode_current->statbuf->st_mode;
	
	if (mode == S_IFREG | S_IRWXU) {
		retstat = 0;
	}
	
    return retstat;
}

/** Release an open file
 *
 * Release is called when there are no more references to an open
 * file: all file descriptors are closed and all memory mappings
 * are unmapped.
 *
 * For every open() call there will be exactly one release() call
 * with the same flags and file descriptor.  It is possible to
 * have a file opened more than once, in which case only the last
 * release will mean, that no more reads/writes will happen on the
 * file.  The return value of release is ignored.
 *
 * Changed in version 2.2
 */
int sfs_release(const char *path, struct fuse_file_info *fi)
{
    int retstat = 0;
    log_msg("\nsfs_release(path=\"%s\", fi=0x%08x)\n",
	  path, fi);
    

    return retstat;
}

/** Read data from an open file
 *
 * Read should return exactly the number of bytes requested except
 * on EOF or error, otherwise the rest of the data will be
 * substituted with zeroes.  An exception to this is when the
 * 'direct_io' mount option is specified, in which case the return
 * value of the read system call will reflect the return value of
 * this operation.
 *
 * Changed in version 2.2
 */
int sfs_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi)
{
    int retstat = 0;
    log_msg("\nsfs_read(path=\"%s\", buf=0x%08x, size=%d, offset=%lld, fi=0x%08x)\n",
	    path, buf, size, offset, fi);

	struct inode *inode_current = find_inode(path);
	
	if (inode_current == NULL) {
		return -1;
	}
	
	if (inode_current->blockk == NULL) {
		return retstat;
	}

	log_msg("\nAttempting to read %d bytes to file %s at block num #%d\n", size, diskfile_path, inode_current->blockk->block_num);
   
   	int diskfile = open(diskfile_path, O_CREAT|O_RDWR, S_IRUSR|S_IWUSR);
    int block_num = inode_current->blockk->block_num;
    retstat = pread(diskfile, buf, size, block_num*BLOCK_SIZE);
	close(diskfile);
	
	log_msg("\nRead %d bytes\n", retstat);
   
    return retstat;
}

/** Write data to an open file
 *
 * Write should return exactly the number of bytes requested
 * except on error.  An exception to this is when the 'direct_io'
 * mount option is specified (see read operation).
 *
 * Changed in version 2.2
 */
int sfs_write(const char *path, const char *buf, size_t size, off_t offset,
	     struct fuse_file_info *fi)
{
    int retstat = 0;
    log_msg("\nsfs_write(path=\"%s\", buf=0x%08x, size=%d, offset=%lld, fi=0x%08x)\n",
	    path, buf, size, offset, fi);
        
    struct inode *inode_current = find_inode(path);
    
    if (inode_current == NULL) {
		return -1;
	}
    
    if (inode_current->blockk == NULL) {
		inode_current->blockk = get_free_block();
	}
    
    log_msg("\nAttempting to write %d bytes to file %s at block number #%d\n", size, diskfile_path, inode_current->blockk->block_num);
    
	int diskfile = open(diskfile_path, O_CREAT|O_RDWR, S_IRUSR|S_IWUSR);
    int block_num = inode_current->blockk->block_num;
    retstat = pwrite(diskfile, buf, size, block_num*BLOCK_SIZE);
	close(diskfile);

    inode_current->statbuf->st_size = size;
    
    log_msg("\nWrote %d bytes\n", retstat);    
	
    return retstat;
}


/** Create a directory */
int sfs_mkdir(const char *path, mode_t mode)
{
    int retstat = 0;
    log_msg("\nsfs_mkdir(path=\"%s\", mode=0%3o)\n",
	    path, mode);
     
	insert_inode(path, S_IFDIR);

    return retstat;
}


/** Remove a directory */
int sfs_rmdir(const char *path)
{
    int retstat = 0;
    log_msg("sfs_rmdir(path=\"%s\")\n",
	    path);
    
    delete_inode(path);
    
    return retstat;
}


/** Open directory
 *
 * This method should check if the open operation is permitted for
 * this  directory
 *
 * Introduced in version 2.3
 */
int sfs_opendir(const char *path, struct fuse_file_info *fi)
{
    int retstat = -EPERM;
    log_msg("\nsfs_opendir(path=\"%s\", fi=0x%08x)\n",
	  path, fi);
    
    struct inode *inode_current = find_inode(path);
	mode_t mode = inode_current->statbuf->st_mode;
	
	if (mode == S_IFDIR | S_IRWXU) {
		retstat = 0;
	}

    return retstat;
}

/** Read directory
 *
 * This supersedes the old getdir() interface.  New applications
 * should use this.
 *
 * The filesystem may choose between two modes of operation:
 *
 * 1) The readdir implementation ignores the offset paraeter, and
 * passes zero to the filler function's offset.  The filler
 * function will not return '1' (unless an error happens), so the
 * whole directory is read in a single readdir operation.  This
 * works just like the old getdir() method.
 *
 * 2) The readdir implementation keeps track of the offsets of the
 * directory entries.  It uses the offset parameter and always
 * passes non-zero offset to the filler function.  When the buffer
 * is full (or an error happens) the filler function will return
 * '1'.
 *
 * Introduced in version 2.3
 */
int sfs_readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset,
	       struct fuse_file_info *fi)
{
	log_msg("\nsfs_readdir(path=\"%s\", fi=0x%08x)\n",
	  path, fi);
	
    int retstat = 0;
     
    struct inode *inode_current = inode_head;
    
    struct inode *directory = NULL;

	// Search for the inode whose path matches the one requested
	while (inode_current != NULL) {
		
		// Found the node
		if (strcmp(path, inode_current->path) == 0) {
			directory = inode_current;
			break;
		}
		
		// Node is in directory
		else if ((S_ISDIR(inode_current->statbuf->st_mode)) &&
		         (strncmp(inode_current->path, path, strlen(inode_current->path))) == 0) {
			inode_current = inode_current->child;
		}
		
		else {
			inode_current = inode_current->next;
		}
	}
	
	// If directory was found, add all the children
	if (directory != NULL) {
		directory = directory->child;
		while (directory != NULL) {
			char *name = strrchr(directory->path, '/');
			filler(buf, name + 1, NULL, 0);
			directory = directory->next;
		}
	}
	
    log_msg("\nsfs_readdir returned\n");
    return retstat;
}

/** Release directory
 *
 * Introduced in version 2.3
 */
int sfs_releasedir(const char *path, struct fuse_file_info *fi)
{
    int retstat = 0;

    
    return retstat;
}

struct fuse_operations sfs_oper = {
  .init = sfs_init,
  .destroy = sfs_destroy,

  .getattr = sfs_getattr,
  .create = sfs_create,
  .unlink = sfs_unlink,
  .open = sfs_open,
  .release = sfs_release,
  .read = sfs_read,
  .write = sfs_write,

  .rmdir = sfs_rmdir,
  .mkdir = sfs_mkdir,

  .opendir = sfs_opendir,
  .readdir = sfs_readdir,
  .releasedir = sfs_releasedir
};

void sfs_usage()
{
    fprintf(stderr, "usage:  sfs [FUSE and mount options] diskFile mountPoint\n");
    abort();
}

int main(int argc, char *argv[])
{
    int fuse_stat;
    struct sfs_state *sfs_data;
    
    // sanity checking on the command line
    if ((argc < 3) || (argv[argc-2][0] == '-') || (argv[argc-1][0] == '-'))
	sfs_usage();

    sfs_data = malloc(sizeof(struct sfs_state));
    if (sfs_data == NULL) {
	perror("main calloc");
	abort();
    }

    // Pull the diskfile and save it in internal data
    sfs_data->diskfile = argv[argc-2];
    diskfile_path = sfs_data->diskfile;
    argv[argc-2] = argv[argc-1];
    argv[argc-1] = NULL;
    argc--;
    
    sfs_data->logfile = log_open();
    
    // turn over control to fuse
    fprintf(stderr, "about to call fuse_main, %s \n", sfs_data->diskfile);
    fuse_stat = fuse_main(argc, argv, &sfs_oper, sfs_data);
    fprintf(stderr, "fuse_main returned %d\n", fuse_stat);
    
    return fuse_stat;
}
