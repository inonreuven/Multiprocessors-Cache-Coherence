#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <ctype.h>
#include "Cache.h"

char* getBinary(unsigned int num);
char* formatBinary(char* bstring);

/********************************
 *        Cache Functions       *
 ********************************/

void getIndexTag(unsigned int address, char* tag, char* index)
{
	char* bstring, * bformatted;
	int i;

	if (address < 0)
	{
		fprintf(stderr, "Error: not a valid cache memory address.\n");
		return;
	}

	bstring = getBinary(address);
	bformatted = formatBinary(bstring);

	/* Fill tag */
	for (i = 0; i < TAG; i++)
		tag[i] = bformatted[i];
	tag[TAG] = '\0';

	/* Fill index */
	for (i = TAG + 1; i < INDEX + TAG + 1; i++)
		index[i - TAG - 1] = bformatted[i];
	index[INDEX] = '\0';

	free(bstring);
	free(bformatted);
}

 /* Get MSI bits of a block */
void getMSIBits(Cache cache, int address, char* pBits)
{
	char tag[TAG + 1];
	char index[INDEX + 1];
	int indexVal;

	/* Validate Inputs */
	if (cache == NULL)
	{
		fprintf(stderr, "Function getMSIBits: cache is null \n");
		return;
	}

	if (address <= 0)
	{
		fprintf(stderr, "Function getMSIBits: cache address is out of range \n");
		return;
	}

	getIndexTag(address, tag, index);
	indexVal = btoi(index);

	/* Get MSI Bits */
	if (strcmp(cache->blocks[indexVal]->tag, tag) == 0)  /* Hit on block */
	{
		pBits[M_BIT] = cache->blocks[indexVal]->modified;
		pBits[S_BIT] = cache->blocks[indexVal]->shared;
		pBits[I_BIT] = cache->blocks[indexVal]->invalid;
	}
	else /* Miss on block */
	{ 
		pBits[M_BIT] = -1;
		pBits[S_BIT] = -1;
		pBits[I_BIT] = -1;
	}
}

/* Set MSI bits of a block */
void setMSIBits(Cache cache, int address, char* pBits) 
{
	char tag[TAG + 1];
	char index[INDEX + 1];
	int indexVal;

	/* Validate Inputs */
	if (cache == NULL)
	{
		fprintf(stderr, "Function getMSIBits: cache is null \n");
		return;
	}

	if (address <= 0)
	{
		fprintf(stderr, "Function getMSIBits: cache address is out of range \n");
		return;
	}

	getIndexTag(address, tag, index);
	indexVal = btoi(index);

	/* Set MSI Bits */
	if (strcmp(cache->blocks[indexVal]->tag, tag) == 0)  /* Hit on block */
	{
		if (pBits[M_BIT] >= 0)
			cache->blocks[address]->modified = pBits[M_BIT];
		if (pBits[S_BIT] >= 0)
			cache->blocks[address]->shared = pBits[S_BIT];
		if (pBits[I_BIT] >= 0)
			cache->blocks[address]->invalid = pBits[I_BIT];
	}
}


/* Create new cache and return it
   param:      cache id
   return:     on success       pointer to new cache 
   return:     on failure       NULL */
Cache getNewCache( int id )
{	
	int write_policy = 1;	/* Write Policy: Write Back */

	/* Create the cache */
	return createCache(id, CACHE_SIZE, BLOCK_SIZE, write_policy);
}

  /* createCache
   *
   * Function to create a new cache struct.  Returns the new struct on success
   * and NULL on failure.
   *
   * param:    cache_size      size of cache in bytes
   * param:    block_size      size of each block in bytes
   * param:    write_policy    0 = write through, 1 = write back
   *
   * return:   on success         new Cache
   * return:   on failure         NULL
   */

Cache createCache(int id, int cache_size, int block_size, int write_policy)
{
	/* Local Variables */
	Cache cache;
	int i;

	/* Validate Inputs */
	if (cache_size <= 0)
	{
		fprintf(stderr, "Cache size must be greater than 0 bytes...\n");
		return NULL;
	}

	if (block_size <= 0)
	{
		fprintf(stderr, "Block size must be greater than 0 bytes...\n");
		return NULL;
	}

	if (write_policy != 0 && write_policy != 1)
	{
		fprintf(stderr, "Write policy must be either \"Write Through\" or \"Write Back\".\n");
		return NULL;
	}

	/* Lets make a cache! */
	cache = (Cache)malloc(sizeof(struct Cache_));
	if (cache == NULL)
	{
		fprintf(stderr, "Could not allocate memory for cache.\n");
		return NULL;
	}

	cache->id = id;
	cache->hits = 0;
	cache->misses = 0;
	cache->reads = 0;
	cache->writes = 0;

	cache->write_policy = write_policy;

	cache->cache_size = CACHE_SIZE;
	cache->block_size = BLOCK_SIZE;

	/* Calculate numLines */
	cache->numLines = (int)(CACHE_SIZE / BLOCK_SIZE);

	cache->blocks = (Block*)malloc(sizeof(Block) * cache->numLines);
	assert(cache->blocks != NULL);

	/* By default set all cache lines to invalid */
	for (i = 0; i < cache->numLines; i++)
	{
		cache->blocks[i] = (Block)malloc(sizeof(struct Block_));
		assert(cache->blocks[i] != NULL);
		cache->blocks[i]->invalid = 1;
		cache->blocks[i]->shared = 0;
		cache->blocks[i]->modified = 0;		
		cache->blocks[i]->tag = (char*) malloc ( (TAG+1) * sizeof(char) );
		if ( cache->blocks[i]->tag != NULL )
			cache->blocks[i]->tag[0] = '\0';
	}

	return cache;
}

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

void destroyCache(Cache cache)
{
	int i;

	if (cache != NULL)
	{
		for (i = 0; i < cache->numLines; i++)
		{
			if (cache->blocks[i]->tag != NULL)
				free(cache->blocks[i]->tag);

			free(cache->blocks[i]);
		}
		free(cache->blocks);
		free(cache);
	}
	return;
}

/* readFromCache
 *
 * Function that reads data from a cache. Returns 0 on failure
 * or 1 on success.
 *
 * param:        cache       target cache struct
 * param:        address     hexidecimal address
 *
 * return        hit     1
 * return:       miss    0
 * return:       error   -1
 */

int readFromCache(Cache cache, int address, int *data)
{
	char index[INDEX+1], tag[TAG+1];
	Block block;

	/* Validate inputs */
	if (cache == NULL)
	{
		fprintf(stderr, "Error: not a valid cache to write to.\n");
		return -1;
	}

	if (address < 0)
	{
		fprintf(stderr, "Error: not a valid cache memory address.\n");
		return -1;
	}

	cache->reads++;

	/* Convert and parse necessary values */
	getIndexTag(address, tag, index);
	
	if (DEBUG)
	{
		printf("Tag: %s (%i)\n", tag, btoi(tag));
		printf("Index: %s (%i)\n", index, btoi(index));		
	}

	/* Get the block (direct mapped cache) */
	block = cache->blocks[btoi(index)];

	if (DEBUG)
		printf("Attempting to read data from cache slot %i.\n", btoi(index));

	if ( block->invalid == 0 && strcmp(block->tag, tag) == 0 ) /* hit */
	{
		cache->hits++;		
		*data = block->data;
		return 1;
	}
	else /* miss */
	{
		cache->misses++;		

		/*
		if (cache->write_policy == 1 && block->modified == 1)
		{
			// Write block to memory before writing a new one in this cache line 
			block->modified = 0;
		}
		
		block->invalid = 0;
		strcpy(block->tag,tag);
		*/
		return 0;
	}
		
	return -1;
}

/* writeToCache
 *
 * Function that writes data to the cache. Returns 0 on failure or
 * 1 on success. Frees any old tags that already existed in the
 * target slot.
 *
 * param:        cache       target cache struct
 * param:        address     hexidecimal address
 *
 * return:       hit + modified     1
 * return:       hit + shared       2
 * return:       miss               0
 * return:       error             -1
 */

int writeToCache(Cache cache, int address, int data)
{	
	char index[INDEX + 1], tag[TAG + 1];
	Block block;


	/* Validate inputs */
	if (cache == NULL)
	{
		fprintf(stderr, "Error: Must supply a valid cache to write to.\n");
		return -1;
	}

	if (address < 0)
	{
		fprintf(stderr, "Error: Must supply a valid memory address.\n");
		return -1;
	}

	getIndexTag(address, tag, index);

	if (DEBUG)
	{
		printf("Tag: %s (%i)\n", tag, btoi(tag));
		printf("Index: %s (%i)\n", index, btoi(index));		
	}

	/* Get the block */
	block = cache->blocks[btoi(index)];

	if (DEBUG)
		printf("Attempting to write data to cache slot %i.\n", btoi(index));

	if ( strcmp(block->tag, tag) == 0 )
	{		
		if (block->invalid == 1)
			return 0; /* miss */

		if (block->modified == 1)
		{
			cache->writes++;
			block->shared = 0;
			block->invalid = 0;
			block->data = data;
			cache->hits++;
			return 1; /* hit */
		}
		
		if (block->shared == 1)
			return 2;		
	}
	
	return 0;
}

int addBlockToCache(Cache cache, int address, int data, char readOrWrite)
{
	char index[INDEX + 1], tag[TAG + 1];
	Block block;
	int indexVal;

	/* Validate inputs */
	if (cache == NULL)
	{
		fprintf(stderr, "Error: Must supply a valid cache to write to.\n");
		return 0;
	}

	if (address < 0)
	{
		fprintf(stderr, "Error: Must supply a valid memory address.\n");
		return 0;
	}

	getIndexTag(address, tag, index);
	indexVal = btoi(index);
	block = cache->blocks[indexVal];

	if (DEBUG)
	{
		printf("Tag: %s (%i)\n", tag, btoi(tag));
		printf("Index: %s (%i)\n", index, indexVal);
	}

	//cache->misses++;
	cache->writes++;
	block->data = data;

	if (readOrWrite == 0) /* Read */
	{
		block->invalid = 0;
		block->modified = 0;
		block->shared = 1;
	}
	else /* write */
	{
		block->invalid = 0;
		block->modified = 1;
		block->shared = 0;
	}

	strcpy(block->tag, tag);
	
	return 1;
}

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

void printCache(Cache cache)
{
	int i;
	char* tag;

	if (cache != NULL)
	{
		for (i = 0; i < cache->numLines; i++)
		{
			tag = "NULL";
			if (cache->blocks[i]->tag != NULL)
				tag = cache->blocks[i]->tag;

			printf("[%i]: { MSI: %i,%i,%i, tag: %s }\n", i, cache->blocks[i]->modified, cache->blocks[i]->shared, 
				                                            cache->blocks[i]->invalid, tag);
		}
		printf("Cache:\n\tCACHE HITS: %i\n\tCACHE MISSES: %i\n\tMEMORY READS: %i\n\tMEMORY WRITES: %i\n\n\tCACHE SIZE: %i Bytes\n\tBLOCK SIZE: %i Bytes\n\tNUM LINES: %i\n", cache->hits, cache->misses, cache->reads, cache->writes, cache->cache_size, cache->block_size, cache->numLines);
	}
}


/********************************
 *     Utility Functions     *
 ********************************/

 /* htoi
   *
   * Converts hexidecimal memory locations to unsigned integers.
   * No real error checking is performed. This function will skip
   * over any non recognized characters.
   */

unsigned int htoi(const char str[])
{
	/* Local Variables */
	unsigned int result;
	int i;

	i = 0;
	result = 0;

	if (str[i] == '0' && str[i + 1] == 'x')
	{
		i = i + 2;
	}

	while (str[i] != '\0')
	{
		result = result * 16;
		if (str[i] >= '0' && str[i] <= '9')
		{
			result = result + (str[i] - '0');
		}
		else if (tolower(str[i]) >= 'a' && tolower(str[i]) <= 'f')
		{
			result = result + (tolower(str[i]) - 'a') + 10;
		}
		i++;
	}

	return result;
}

/* getBinary
 *
 * Converts an unsigned integer into a string containing it's
 * 32 bit long binary representation.
 *
 *
 * param:    num         number to be converted
 *
 * result:   char*       binary string
 */

char* getBinary(unsigned int num)
{
	char* bstring;
	int i;

	/* Calculate the Binary String */

	bstring = (char*)malloc(sizeof(char) * 33);
	assert(bstring != NULL);

	bstring[32] = '\0';

	for (i = 0; i < 32; i++)
		bstring[32 - 1 - i] = (num == ((1 << i) | num)) ? '1' : '0';

	return bstring;
}

/* formatBinary
 *
 * Converts a 32 bit long binary string to a formatted version
 * for easier parsing. The format is determined by the TAG, INDEX,
 * and OFFSET variables.
 *
 * Ex. Format:
 *  -----------------------------------------------------
 * | Tag: 18 bits | Index: 12 bits | Byte Select: 4 bits |
 *  -----------------------------------------------------
 *
 * Ex. Result:
 * 000000000010001110 101111011111 00
 *
 * param:    bstring     binary string to be converted
 *
 * result:   char*       formated binary string
 */

char* formatBinary(char* bstring)
{
	char* formatted;
	int i;

	/* Format for Output */

	formatted = (char*)malloc(sizeof(char) * (ADDR_SIZE + 3));
	assert(formatted != NULL);

	formatted[ADDR_SIZE + 2] = '\0';

	for (i = 0; i < TAG; i++)
		formatted[i] = bstring[i];

	formatted[TAG] = ' ';

	for (i = TAG + 1; i < INDEX + TAG + 1; i++)
		formatted[i] = bstring[i - 1];

	formatted[INDEX + TAG + 1] = ' ';

	for (i = INDEX + TAG + 2; i < OFFSET + INDEX + TAG + 2; i++)
		formatted[i] = bstring[i - 2];

	return formatted;
}

/* btoi
 *
 * Converts a binary string to an integer. Returns 0 on error.
 *
 * param:    bin     binary string to convert
 *
 * result:   int     decimal representation of binary string
 */

int btoi(char* bin)
{
	int  b, k, m, n;
	int  len, sum;

	sum = 0;
	len = strlen(bin) - 1;

	for (k = 0; k <= len; k++)
	{
		n = (bin[k] - '0');
		if ((n > 1) || (n < 0))
			return 0;
	
		for (b = 1, m = len; m > k; m--)
			b *= 2;
		
		sum = sum + n * b;
	}
	return(sum);
}

/*
void test()
{
	while (fgets(buffer, LINELENGTH, file) != NULL)
	{
		if (buffer[0] != '#')
		{
			i = 0;
			while (buffer[i] != ' ')
			{
				i++;
			}

			mode = buffer[i + 1];

			i = i + 2;
			j = 0;

			while (buffer[i] != '\0')
			{
				address[j] = buffer[i];
				i++;
				j++;
			}

			address[j - 1] = '\0';

			if (DEBUG) printf("%i: %c %s\n", counter, mode, address);

			if (mode == 'R')
			{
				readFromCache(cache, address);
			}
			else if (mode == 'W')
			{
				writeToCache(cache, address);
			}
			else
			{
				printf("%i: ERROR!!!!\n", counter);
				fclose(file);
				destroyCache(cache);
				cache = NULL;

				return 0;
			}
			counter++;
		}
	}

	if (DEBUG) printf("Num Lines: %i\n", counter);

	printf("CACHE HITS: %i\nCACHE MISSES: %i\nMEMORY READS: %i\nMEMORY WRITES: %i\n", cache->hits, cache->misses, cache->reads, cache->writes);

	// Close the file, destroy the cache

	fclose(file);
	destroyCache(cache);
	cache = NULL;

	return 1;
}
*/