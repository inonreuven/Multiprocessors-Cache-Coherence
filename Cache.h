#ifndef CACHE_H_
#define CACHE_H_

/* Constants
 *
 * Both CACHE_SIZE and BLOCK_SIZE are in bytes. We can calculate the number
 * of lines in the cache with CACHE_SIZE / BLOCK_SIZE.
 *
 */

 /* Print Debug Messages */
#define DEBUG 0

/* Cache Size (in 4 byte words) */
#define CACHE_SIZE 256
#define BLOCK_SIZE 1

#define TAG 12   
#define INDEX 8 
#define OFFSET 0 
#define ADDR_SIZE (TAG+INDEX+OFFSET)

typedef enum{ M_BIT = 0, S_BIT = 1, I_BIT = 2, NUM_MSI_BITS } MSIBit;

  /********************************
   *           Structs            *
   ********************************/

   /* Block
	*
	* Holds an integer that states the validity of the bit (0 = invalid,
	* 1 = valid), the tag being held, and another integer that states if
	* the bit is modified or not (0 = clean, 1 = modified).
	*/

struct Block_
{
	char invalid;
	char shared;
	char modified;
	char* tag;
	int data;
};
typedef struct Block_* Block;

/* Cache
 *
 * Cache object that holds all the data about cache access as well as
 * the write policy, sizes, and an array of blocks.
 *
 * param:    hits            # of cache accesses that hit valid data
 * param:    misses          # of cache accesses that missed valid data
 * param:    reads           # of reads from main memory
 * param:    writes          # of writes from main memory
 * param:    cache_size      Total size of the cache in bytes
 * param:    block_size      How big each block of data should be
 * param:    numLines        Total number of blocks
 * param:    blocks          The actual array of blocks
 */
struct Cache_
{
	int id;
	int hits;
	int misses;
	int reads;
	int writes;
	int cache_size;
	int block_size;
	int numLines;
	int write_policy;
	Block* blocks;
};
typedef struct Cache_* Cache;


/* Create a new cache and return it */
Cache getNewCache( int id );

/* createCache
 *
 * Function to create a new cache struct.  Returns the new struct on success
 * and NULL on failure.
 *
 * param     id              cache id
 * param:    cache_size      size of cache in bytes
 * param:    block_size      size of each block in bytes
 * param:    write_policy    0 = write through, 1 = write back
 *
 * return:   on success         new Cache
 * return:   on failure         NULL
 */

Cache createCache(int id, int cache_size, int block_size, int write_policy);

/* destroyCache
 *
 * Function that destroys a created cache. Frees all allocated memory. If
 * you pass in NULL, nothing happens. So make sure to set your cache = NULL
 * after you destroy it to prevent a double free.
 *
 * param:    cache           cache object to be destroyed
 *
 * return:   void
 */

void destroyCache(Cache cache);

/* readFromCache
 *
 * Function that reads data from a cache. Returns 0 on failure
 * or 1 on success.
 *
 * param:        cache       target cache struct
 * param:        address     hexidecimal address
 *
 * return:       on success     1
 * return:       on failure     0
 */

int readFromCache(Cache cache, int address, int* data);

/* writeToCache
 *
 * Function that writes data to the cache. Returns 0 on failure or
 * 1 on success. Frees any old tags that already existed in the
 * target slot.
 *
 * param:        cache       target cache struct
 * param:        address     hexidecimal address
 *
 * return:       on success     1
 * return:       on error       0
 */

int writeToCache(Cache cache, int address, int data);

int addBlockToCache(Cache cache, int address, int data, char readOrWrite);

/* printCache
 *
 * Prints out the values of each slot in the cache
 * as well as the hit, miss, read, write, and size
 * data.
 *
 * param:        cache       Cache struct
 *
 * return:       void
 */

void printCache(Cache cache);

/* Get MSI bits of a block */
void getMSIBits(Cache cache, int address, char *pBits );

/* Set MSI bits of a block */
void setMSIBits(Cache cache, int address, char* pBits);

/* Converts a binary string to an integer.Returns 0 on error. */
int btoi(char* bin);

#endif
