/* Linear probe hash table.
 *
 * Simple hashtable with open adressing linear probe.
 * Does not manage memory of key/value pointers.
 * Does not support deleting entries.
 */ 

#include "ghostxps.h"

static const unsigned primes[] =
{
    61, 127, 251, 509, 1021, 2039, 4093, 8191, 16381, 32749, 65521,
    131071, 262139, 524287, 1048573, 2097143, 4194301, 8388593, 0
};

typedef struct xps_hash_entry_s xps_hash_entry_t;

struct xps_hash_entry_s
{
    char *key;
    void *value;
};

struct xps_hash_table_s
{
    void *ctx;
    unsigned int size;
    unsigned int load;
    xps_hash_entry_t *entries;
};

static unsigned int hash(char *s)
{
    unsigned int h = 0;
    while (*s)
	h = *s++ + (h << 6) + (h << 16) - h;
    return h;
}

xps_hash_table_t *xps_hash_new(xps_context_t *ctx)
{
    xps_hash_table_t *table;

    table = xps_alloc(ctx, sizeof(xps_hash_table_t));
    if (!table)
    {
	gs_throw(-1, "out of memory: hash table struct");
	return NULL;
    }

    table->size = primes[0];
    table->load = 0;

    table->entries = xps_alloc(ctx, sizeof(xps_hash_entry_t) * table->size);
    if (!table->entries)
    {
	xps_free(ctx, table);
	gs_throw(-1, "out of memory: hash table entries array");
	return NULL;
    }

    memset(table->entries, 0, sizeof(xps_hash_entry_t) * table->size);

    return table;
}

static int xps_hash_double(xps_context_t *ctx, xps_hash_table_t *table)
{
    xps_hash_entry_t *old_entries;
    xps_hash_entry_t *new_entries;
    unsigned int old_size = table->size;
    unsigned int new_size = table->size * 2;
    int i;

    for (i = 0; primes[i] != 0; i++)
    {
	if (primes[i] > old_size)
	{
	    new_size = primes[i];
	    break;
	}
    }

    old_entries = table->entries;
    new_entries = xps_alloc(ctx, sizeof(xps_hash_entry_t) * new_size);
    if (!new_entries)
	return gs_throw(-1, "out of memory: hash table entries array");

    table->size = new_size;
    table->entries = new_entries;
    table->load = 0;

    memset(table->entries, 0, sizeof(xps_hash_entry_t) * table->size);

    for (i = 0; i < old_size; i++)
	if (old_entries[i].value)
	    xps_hash_insert(ctx, table, old_entries[i].key, old_entries[i].value);

    xps_free(ctx, old_entries);

    return 0;
}

void xps_hash_free(xps_context_t *ctx, xps_hash_table_t *table)
{
    xps_free(ctx, table->entries);
    xps_free(ctx, table);
}

void *xps_hash_lookup(xps_hash_table_t *table, char *key)
{
    xps_hash_entry_t *entries = table->entries;
    unsigned int size = table->size;
    unsigned int pos = hash(key) % size;

    while (1)
    {
	if (!entries[pos].value)
	    return NULL;

	if (strcmp(key, entries[pos].key) == 0)
	    return entries[pos].value;

	pos = (pos + 1) % size;
    }
}

int xps_hash_insert(xps_context_t *ctx, xps_hash_table_t *table, char *key, void *value)
{
    xps_hash_entry_t *entries;
    unsigned int size, pos;

    /* Grow the table at 80% load */
    if (table->load > table->size * 8 / 10)
    {
	if (xps_hash_double(ctx, table) < 0)
	    return gs_rethrow(-1, "cannot grow hash table");
    }

    entries = table->entries;
    size = table->size;
    pos = hash(key) % size;

    while (1)
    {
	if (!entries[pos].value)
	{
	    entries[pos].key = key;
	    entries[pos].value = value;
	    table->load ++;
	    return 0;
	}

	if (strcmp(key, entries[pos].key) == 0)
	{
	    return 0;
	}

	pos = (pos + 1) % size;
    }
}

void xps_hash_debug(xps_hash_table_t *table)
{
    int i;

    printf("hash table load %d / %d\n", table->load, table->size);

    for (i = 0; i < table->size; i++)
    {
	if (!table->entries[i].value)
	    printf("table % 4d: empty\n", i);
	else
	    printf("table % 4d: key=%s value=%p\n", i,
		    table->entries[i].key, table->entries[i].value);
    }
}

