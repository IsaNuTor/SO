#include "fuseLib.h"

#include <stdio.h>
#include <time.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <errno.h>
#include <inttypes.h>
#include <linux/kdev_t.h>

/**
 * @brief Modifies the data size originally reserved by an inode, reserving or removing space if needed.
 *
 * @param idxNode inode number
 * @param newSize new size for the inode
 * @return int
 **/
int resizeNode(uint64_t idxNode, size_t newSize)
{
    NodeStruct *node = myFileSystem.nodes[idxNode];
    char block[BLOCK_SIZE_BYTES];
    int i, diff = newSize - node->fileSize;

    if(!diff)
        return 0;

    memset(block, 0, sizeof(char)*BLOCK_SIZE_BYTES);

    /// File size increases
    if(diff > 0) {

        /// Delete the extra conent of the last block if it exists and is not full
        if(node->numBlocks && node->fileSize % BLOCK_SIZE_BYTES) {
            int currentBlock = node->blocks[node->numBlocks - 1];

            if( readBlock(&myFileSystem, currentBlock, &block)==-1 ) {
                fprintf(stderr,"Error reading block in resizeNode\n");
                return -EIO;
            }

            int offBlock = node->fileSize % BLOCK_SIZE_BYTES;
            int bytes2Write = (diff > (BLOCK_SIZE_BYTES - offBlock)) ? BLOCK_SIZE_BYTES - offBlock : diff;
            for(i = 0; i < bytes2Write; i++) {
                block[offBlock++] = 0;
            }

            if( writeBlock(&myFileSystem, currentBlock, &block)==-1 ) {
                fprintf(stderr,"Error writing block in resizeNode\n");
                return -EIO;
            }
        }

        /// File size in blocks after the increment
        int newBlocks = (newSize + BLOCK_SIZE_BYTES - 1) / BLOCK_SIZE_BYTES - node->numBlocks;
        if(newBlocks) {
            memset(block, 0, sizeof(char)*BLOCK_SIZE_BYTES);

            // We check that there is enough space
            if(newBlocks > myFileSystem.superBlock.numOfFreeBlocks)
                return -ENOSPC;

            myFileSystem.superBlock.numOfFreeBlocks -= newBlocks;
            int currentBlock = node->numBlocks;
            node->numBlocks += newBlocks;

            for(i = 0; currentBlock != node->numBlocks; i++) {
                if(myFileSystem.bitMap[i] == 0) {
                    myFileSystem.bitMap[i] = 1;
                    node->blocks[currentBlock] = i;
                    currentBlock++;
                    // Clean disk (necessary for truncate)
                    if( writeBlock(&myFileSystem, i, &block)==-1 ) {
                        fprintf(stderr,"Error writing block in resizeNode\n");
                        return -EIO;
                    }
                }
            }
        }
        node->fileSize += diff;

    }
    /// File decreases
    else {
        // File size in blocks after truncation
        int numBlocks = (newSize + BLOCK_SIZE_BYTES - 1) / BLOCK_SIZE_BYTES;
        myFileSystem.superBlock.numOfFreeBlocks += (node->numBlocks - numBlocks);

        for(i = node->numBlocks; i > numBlocks; i--) {
            int nBloque = node->blocks[i - 1];
            myFileSystem.bitMap[nBloque] = 0;
            // Clean disk (it is not really necessary)
            if( writeBlock(&myFileSystem, nBloque, &block)==-1 ) {
                fprintf(stderr,"Error writing block in resizeNode\n");
                return -EIO;
            }
        }
        node->numBlocks = numBlocks;
        node->fileSize += diff;
    }
    node->modificationTime = time(NULL);

    sync();

    /// Update all the information in the backup file
    updateSuperBlock(&myFileSystem);
    updateBitmap(&myFileSystem);
    updateNode(&myFileSystem, idxNode, node);

    return 0;
}

/**
 * @brief It formats the access mode of a file so it is compatible on screen, when it is printed
 *
 * @param mode mode
 * @param str output with the access mode in string format
 * @return void
 **/
void mode_string(mode_t mode, char *str)
{
    str[0] = mode & S_IRUSR ? 'r' : '-';
    str[1] = mode & S_IWUSR ? 'w' : '-';
    str[2] = mode & S_IXUSR ? 'x' : '-';
    str[3] = mode & S_IRGRP ? 'r' : '-';
    str[4] = mode & S_IWGRP ? 'w' : '-';
    str[5] = mode & S_IXGRP ? 'x' : '-';
    str[6] = mode & S_IROTH ? 'r' : '-';
    str[7] = mode & S_IWOTH ? 'w' : '-';
    str[8] = mode & S_IXOTH ? 'x' : '-';
    str[9] = '\0';
}

/**
 * @brief Obtains the file attributes of a file just with the filename
 *
 * Help from FUSE:
 *
 * The 'st_dev' and 'st_blksize' fields are ignored. The 'st_ino' field is ignored except if the 'use_ino' mount option is given.
 *
 *		struct stat {
 *			dev_t     st_dev;     // ID of device containing file
 *			ino_t     st_ino;     // inode number
 *			mode_t    st_mode;    // protection
 *			nlink_t   st_nlink;   // number of hard links
 *			uid_t     st_uid;     // user ID of owner
 *			gid_t     st_gid;     // group ID of owner
 *			dev_t     st_rdev;    // device ID (if special file)
 *			off_t     st_size;    // total size, in bytes
 *			blksize_t st_blksize; // blocksize for file system I/O
 *			blkcnt_t  st_blocks;  // number of 512B blocks allocated
 *			time_t    st_atime;   // time of last access
 *			time_t    st_mtime;   // time of last modification (file's content were changed)
 *			time_t    st_ctime;   // time of last status change (the file's inode was last changed)
 *		};
 *
 * @param path full file path
 * @param stbuf file attributes
 * @return 0 on success and <0 on error
 **/
static int my_getattr(const char *path, struct stat *stbuf)
{
    NodeStruct *node;
    int idxDir;

    fprintf(stderr, "--->>>my_getattr: path %s\n", path);

    memset(stbuf, 0, sizeof(struct stat));

    /// Directory attributes
    if(strcmp(path, "/") == 0) {
        stbuf->st_mode = S_IFDIR | 0755;
        stbuf->st_nlink = 2;
        stbuf->st_uid = getuid();
        stbuf->st_gid = getgid();
        stbuf->st_mtime = stbuf->st_ctime = myFileSystem.superBlock.creationTime;
        return 0;
    }

    /// Rest of the world
    if((idxDir = findFileByName(&myFileSystem, (char *)path + 1)) != -1) {
        node = myFileSystem.nodes[myFileSystem.directory.files[idxDir].nodeIdx];
        stbuf->st_size = node->fileSize;
        stbuf->st_mode = S_IFREG | 0644;
        stbuf->st_nlink = 1;
        stbuf->st_uid = getuid();
        stbuf->st_gid = getgid();
        stbuf->st_mtime = stbuf->st_ctime = node->modificationTime;
        return 0;
    }

    return -ENOENT;
}

/**
 * @brief Reads the content of the root directory
 *
 * Help from FUSE:
 *
 * The filesystem may choose between two modes of operation:
 *
 * 1) The readdir implementation ignores the offset parameter, and passes zero to the filler function's offset.
 *    The filler function will not return '1' (unless an error happens), so the whole directory is read in a single readdir operation.
 *
 * 2) The readdir implementation keeps track of the offsets of the directory entries.
 *    It uses the offset parameter and always passes non-zero offset to the filler function. When the buffer is full
 *    (or an error happens) the filler function will return '1'.
 *
 * Function to add an entry in a readdir() operation:
 *	typedef int(* fuse_fill_dir_t)(void *buf, const char *name, const struct stat *stbuf, off_t off)
 *
 *	*Parameters
 *		-buf: the buffer passed to the readdir() operation
 *		-name: the file name of the directory entry
 *		-stat: file attributes, can be NULL
 *		-off: offset of the next entry or zero
 *	*Returns 1 if buffer is full, zero otherwise
 *
 * @param path path to the root folder
 * @param buf buffer where all the forder entries will be stored
 * @param filler FUSE funtion used to fill the buffer
 * @param offset offset used by filler to fill the buffer
 * @param fi FUSE structure associated to the directory
 * @return 0 on success and <0 on error
 **/
static int my_readdir(const char *path, void *buf, fuse_fill_dir_t filler,  off_t offset, struct fuse_file_info *fi)
{
    int i;

    fprintf(stderr, "--->>>my_readdir: path %s, offset %jd\n", path, (intmax_t)offset);

    if(strcmp(path, "/") != 0)
        return -ENOENT;

    filler(buf, ".", NULL, 0);
    filler(buf, "..", NULL, 0);

    for(i = 0; i < MAX_FILES_PER_DIRECTORY; i++) {
        if(!(myFileSystem.directory.files[i].freeFile)) {
            if(filler(buf, myFileSystem.directory.files[i].fileName, NULL, 0) == 1)
                return -ENOMEM;
        }
    }

    return 0;
}

/**
 * @brief File opening
 *
 * Help from FUSE:
 *
 * No creation (O_CREAT, O_EXCL) and by default also no truncation (O_TRUNC) flags will be passed to open().
 * If an application specifies O_TRUNC, fuse first calls truncate() and then open().
 *
 * Unless the 'default_permissions' mount option is given (limits access to the mounting user and root),
 * open should check if the operation is permitted for the given flags.
 *
 * Optionally open may also return an arbitrary filehandle in the fuse_file_info structure, which will be passed to all file operations.
 *
 *	struct fuse_file_info{
 *		int				flags				Open flags. Available in open() and release()
 *		unsigned long 	fh_old				Old file handle, don't use
 *		int 			writepage			In case of a write operation indicates if this was caused by a writepage
 *		unsigned int 	direct_io: 1		Can be filled in by open, to use direct I/O on this file
 *		unsigned int 	keep_cache: 1		Can be filled in by open, to indicate, that cached file data need not be invalidated.
 *		unsigned int 	flush: 1			Indicates a flush operation.
 *		unsigned int 	nonseekable: 1		Can be filled in by open, to indicate that the file is not seekable.
 *		unsigned int 	padding: 27			Padding. Do not use
 *		uint64_t 		fh					File handle. May be filled in by filesystem in open(). Available in all other file operations
 *		uint64_t 		lock_owner			Lock owner id.
 *		uint32_t 		poll_events			Requested poll events.
 *	}
 *
 * @param path path to the root folder
 * @param fi FUSE structure associated to the file opened
 * @return 0 on success and <0 on error
 **/
static int my_open(const char *path, struct fuse_file_info *fi)
{
    int idxDir;

    fprintf(stderr, "--->>>my_open: path %s, flags %d, %"PRIu64"\n", path, fi->flags, fi->fh);

    //if(findFileByName(path, &idxNodoI)){
    if((idxDir = findFileByName(&myFileSystem, (char *)path + 1)) == -1) {
        return -ENOENT;
    }

    // Save the inode number in file handler to be used in the following calls
    fi->fh = myFileSystem.directory.files[idxDir].nodeIdx;

    return 0;
}


/**
 * @brief Write data on an opened file
 *
 * Help from FUSE
 *
 * Write should return exactly the number of bytes requested except on error.
 *
 * @param path file path
 * @param buf buffer where we have data to write
 * @param size quantity of bytes to write
 * @param offset offset over the writing
 * @param fi FUSE structure linked to the opened file
 * @return 0 on success and <0 on error
 **/
static int my_write(const char *path, const char *buf, size_t size, off_t offset, struct fuse_file_info *fi)
{
    char buffer[BLOCK_SIZE_BYTES];
    int bytes2Write = size, totalWrite = 0;
    NodeStruct *node = myFileSystem.nodes[fi->fh];

    fprintf(stderr, "--->>>my_write: path %s, size %zu, offset %jd, fh %"PRIu64"\n", path, size, (intmax_t)offset, fi->fh);

    // Increase the file size if it is needed
    if(resizeNode(fi->fh, size + offset) < 0)
        return -EIO;

    // Write data
    while(bytes2Write) {
        int i;
        int currentBlock, offBlock;
        currentBlock = node->blocks[offset / BLOCK_SIZE_BYTES];
        offBlock = offset % BLOCK_SIZE_BYTES;

        if( readBlock(&myFileSystem, currentBlock, &buffer)==-1 ) {
            fprintf(stderr,"Error reading blocks in my_write\n");
            return -EIO;
        }

        for(i = offBlock; (i < BLOCK_SIZE_BYTES) && (totalWrite < size); i++) {
            buffer[i] = buf[totalWrite++];
        }

        if( writeBlock(&myFileSystem, currentBlock, &buffer)==-1 ) {
            fprintf(stderr,"Error writing block in my_write\n");
            return -EIO;
        }

        // Discount the written stuff
        bytes2Write -= (i - offBlock);
        offset += (i - offBlock);
    }
    sync();

    node->modificationTime = time(NULL);
    updateSuperBlock(&myFileSystem);
    updateBitmap(&myFileSystem);
    updateNode(&myFileSystem, fi->fh, node);

    return size;
}

/**
 * @brief Read data on an opened file
 *
 * Help from FUSE
 *
 * Read should return exactly the number of bytes requested except on error.
 *
 * @param path file path
 * @param buf buffer where we have data to read
 * @param size quantity of bytes to read
 * @param offset offset over the writing
 * @param fi FUSE structure linked to the opened file
 * @return 0 on success and <0 on error
 **/
static int my_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi)
{ 
	int idFichero, idNodo;
	int bytes2Read, bloqueLeido,totalRead = 0, tamanoTotalLeido;

	// Buscamos el fichero, si no lo encuentra devolvemos error
	if((idFichero = findFileByName( &myFileSystem, (char*)path+1)) == -1 ){
        return -ENOENT;
    }
    
	// Obtener el nodo del fichero.
	idNodo = myFileSystem.directory.files[idFichero].nodeIdx;
	NodeStruct *nodo = (NodeStruct*) malloc(sizeof(NodeStruct));
	readNode(&myFileSystem, idNodo, nodo);

	// Comprobar que offset no sale fuera del fichero
	// bytes2Read = numero de bytes a leer (puede ser menor que size)
	bytes2Read = nodo->fileSize - offset;
	//Si fileSize es menor que el tamaño leido del fichero
	if(bytes2Read < size){
		// Si el offset es mayor que el size entonces bytes2Read es negativo y no puede.
		if(bytes2Read < 0){
			bytes2Read = 0;
		}
	}else{
		
		bytes2Read = size;
	}
	
	while (totalRead < bytes2Read) {
		int i, currentBlock, offBlock, leido;
		//currentBlock = bloque fisico en la posicion offset
		bloqueLeido= (offset + totalRead) / BLOCK_SIZE_BYTES;
		currentBlock = myFileSystem.nodes[idNodo]->blocks[bloqueLeido];
		//offBlock = desplazamiento en el bloque fisico correspondiente
		offBlock = (offset + totalRead) % BLOCK_SIZE_BYTES;
		//buffer <- bloque currentblock del disco virtual
		lseek(myFileSystem.fdVirtualDisk, (currentBlock * BLOCK_SIZE_BYTES) + offBlock, SEEK_SET);
		//copiar los bytes leidos al buffer de salida buf
		leido = size - totalRead;
		
		for(i = 0; i < leido; i++){
			*buf = '\0';
			buf++;
		}
		//actualizar offset y totalRead
		tamanoTotalLeido = read(myFileSystem.fdVirtualDisk, buf, 1);

		if (tamanoTotalLeido > 0) {

			buf += tamanoTotalLeido;
			totalRead += tamanoTotalLeido;

		} else if (tamanoTotalLeido == -1) {
			return -ENOENT;
		
		}

	}
	return totalRead;
}

static int my_unlink(const char *path){
    int idxNodoI, idxFile;
    fprintf(stderr, "--->>>my_unlink: path %s", path);

    //We look for the file
    if ( (idxFile = findFileByName( &myFileSystem, (char*)path+1)) == -1 ){
        return -ENOENT;
    }
    //We look for the corresponding Inode
    idxNodoI = myFileSystem.directory.files[idxFile].nodeIdx;
    NodeStruct *node = (NodeStruct*) malloc (sizeof (NodeStruct));
    readNode(&myFileSystem, idxNodoI, node);

    //We clean the bitmap
    BIT bit;
    int i;
    for (i =0; i<node->numBlocks; i++){
    	bit = node->blocks[i];
    	node->blocks[i]=0;
    	myFileSystem.bitMap[bit]=0;
    }

    //We resize the node to 0
    resizeNode(idxNodoI, 0);

    //We update directory and filesystem information
    myFileSystem.directory.files[idxFile].freeFile= true;
    myFileSystem.directory.numFiles--;
    myFileSystem.numFreeNodes++;
    node->freeNode = true;

    //We 'commit' the changes.
    updateBitmap(&myFileSystem);
    updateDirectory(&myFileSystem);
    updateNode(&myFileSystem, idxNodoI, node);
    free(node);
    sync();
    return 0;

}


/**
 * @brief Close the file
 *
 * Help from FUSE:
 *
 * Release is calle