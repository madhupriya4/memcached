#include "memcached.h"
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <stdlib.h>

#define MAX_ITEM_FREELIST_LENGTH 4000
#define INIT_ITEM_FREELIST_LENGTH 500

static size_t item_make_header_bdb(const uint8_t nkey, const int flags, const int nbytes, char *suffix, uint8_t *nsuffix);

static item **freeitem;
static int freeitemtotal;
static int freeitemcurr;

void item_init_bdb(void) {
    freeitemtotal = INIT_ITEM_FREELIST_LENGTH;
    freeitemcurr  = 0;

    freeitem = (item **)malloc( sizeof(item *) * freeitemtotal );
    if (freeitem == NULL) {
        perror("malloc()");
    }
    return;
}

/*
 * Returns a item buffer from the freelist, if any. Sholud call
 * item_from_freelist for thread safty.
 * */
item *do_item_from_freelist_bdb(void) {
    item *s;

    if (freeitemcurr > 0) {
        s = freeitem[--freeitemcurr];
    } else {
        /* If malloc fails, let the logic fall through without spamming
         * STDERR on the server. */
        s = (item *)malloc( settings_bdb.item_buf_size );
        if (s != NULL){
            memset(s, 0, settings_bdb.item_buf_size);
        }
    }

    return s;
}

/*
 * Adds a item to the freelist. Should call
 * item_add_to_freelist for thread safty.
 */
int do_item_add_to_freelist_bdb(item *it) {
    if (freeitemcurr < freeitemtotal) {
        freeitem[freeitemcurr++] = it;
        return 0;
    } else {
        if (freeitemtotal >= MAX_ITEM_FREELIST_LENGTH){
            return 1;
        }
        /* try to enlarge free item buffer array */
        item **new_freeitem = (item **)realloc(freeitem, sizeof(item *) * freeitemtotal * 2);
        if (new_freeitem) {
            freeitemtotal *= 2;
            freeitem = new_freeitem;
            freeitem[freeitemcurr++] = it;
            return 0;
        }
    }
    return 1;
}

/**
 * Generates the variable-sized part of the header for an object.
 *
 * key     - The key
 * nkey    - The length of the key
 * flags   - key flags
 * nbytes  - Number of bytes to hold value and addition CRLF terminator
 * suffix  - Buffer for the "VALUE" line suffix (flags, size).
 * nsuffix - The length of the suffix is stored here.
 *
 * Returns the total size of the header.
 */
static size_t item_make_header_bdb(const uint8_t nkey, const int flags, const int nbytes,
                                   char *suffix, uint8_t *nsuffix) {
    /* suffix is defined at 40 chars elsewhere.. */
    *nsuffix = (uint8_t) snprintf(suffix, 40, " %d %d\r\n", flags, nbytes - 2);
    return sizeof(item) + nkey + *nsuffix + nbytes;
}

/*
 * alloc a item buffer, and init it.
 */
item *item_alloc1_bdb(char *key, const size_t nkey, const int flags, const int nbytes) {
    uint8_t nsuffix;
    item *it;
    char suffix[40];
    size_t ntotal = item_make_header_bdb(nkey + 1, flags, nbytes, suffix, &nsuffix);

    if (ntotal > settings_bdb.item_buf_size){
        it = (item *)malloc(ntotal);
        if (it == NULL){
            return NULL;
        }
        memset(it, 0, ntotal);
        if (settings_bdb.verbose > 1) {
            fprintf(stderr, "alloc a item buffer from malloc.\n");
        }
    }else{
        it = item_from_freelist_bdb();
        if (it == NULL){
            return NULL;
        }
        if (settings_bdb.verbose > 1) {
            fprintf(stderr, "alloc a item buffer from freelist.\n");
        }
    }

    it->nkey = nkey;
    it->nbytes = nbytes;
    strcpy(ITEM_key(it), key);
    memcpy(ITEM_suffix(it), suffix, (size_t)nsuffix);
    it->nsuffix = nsuffix;
    return it;
}

/*
 * alloc a item buffer only.
 */
item *item_alloc2_bdb(size_t ntotal) {
    item *it;
    if (ntotal > settings_bdb.item_buf_size){
        it = (item *)malloc(ntotal);
        if (it == NULL){
            return NULL;
        }
        memset(it, 0, ntotal);
        if (settings_bdb.verbose > 1) {
            fprintf(stderr, "alloc a item buffer from malloc.\n");
        }
    }else{
        it = item_from_freelist_bdb();
        if (it == NULL){
            return NULL;
        }
        if (settings_bdb.verbose > 1) {
            fprintf(stderr, "alloc a item buffer from freelist.\n");
        }
    }

    return it;
}

/*
 * free a item buffer. here 'it' must be a full item.
 */

int item_free_bdb(item *it) {
    size_t ntotal = 0;
    if (NULL == it)
        return 0;

    /* ntotal may be wrong, if 'it' is not a full item. */
    ntotal = ITEM_ntotal(it);
    if (ntotal > settings_bdb.item_buf_size){
        if (settings_bdb.verbose > 1) {
            //fprintf(stderr, "ntotal: %"PRIuS", use free() directly.\n", ntotal);
        }
        free(it);
    }else{
        if (0 != item_add_to_freelist_bdb(it)) {
            if (settings_bdb.verbose > 1) {
//                fprintf(stderr, "ntotal: %"PRIuS", add a item buffer to freelist fail, use free() directly.\n", ntotal);
            }
            free(it);
        }else{
            if (settings_bdb.verbose > 1) {
//                fprintf(stderr, "ntotal: %"PRIuS", add a item buffer to freelist.\n", ntotal);
            }
        }
    }
    return 0;
}

/* if return item is not NULL, free by caller */
item *item_get_bdb(char *key, size_t nkey){
    fprintf(stderr,"inside item_get_bdb\n");
    item *it = NULL;
    DBT dbkey, dbdata;
    bool stop;
    int ret;

    /* first, alloc a fixed size */

    it = item_alloc2_bdb(settings_bdb.item_buf_size);
    if (it == 0) {
        return NULL;
    }

    BDB_CLEANUP_DBT();
    dbkey.data = key;
    dbkey.size = nkey;
    dbdata.ulen = settings_bdb.item_buf_size;
    dbdata.data = it;
    dbdata.flags = DB_DBT_USERMEM;

    stop = false;
    fprintf(stderr,"try to get a item from bdb\n");
    /* try to get a item from bdb */
    while (!stop) {
        switch (ret = dbp->get(dbp, NULL, &dbkey, &dbdata, 0)) {
            case DB_BUFFER_SMALL:    /* user mem small */
                fprintf(stderr,"buffer too small, increasing it's size\n");
                /* free the original smaller buffer */
                item_free_bdb(it);
                /* alloc the correct size */
                it = item_alloc2_bdb(dbdata.size);
                if (it == NULL) {
                    return NULL;
                }

                dbdata.ulen = dbdata.size;
                dbdata.data = it;
                continue;
            case 0:                  /* Success. */
                stop = true;
                ReferencePage(lruQueue,key,nkey,it->nbytes);
                printQueue(lruQueue);
                break;
            case DB_NOTFOUND:
                stop = true;
                item_free_bdb(it);
                it = NULL;
                break;
            default:
                /* TODO: may cause bug here, if return DB_BUFFER_SMALL then retun non-zero again
                 * here 'it' may not a full one. a item buffer larger than item_buf_size may be added to freelist */
                stop = true;
                fprintf(stderr,"in the default condition of item_get_bdb\n");
                item_free_bdb(it);
                it = NULL;
                fprintf(stderr,"did not retrieve item");
                if (settings_bdb.verbose > 1) {
                    fprintf(stderr, "dbp->get: %s\n", db_strerror(ret));
                }
        }
    }
    if(it)
        fprintf(stderr,"returning item%s",ITEM_suffix(it));
    return it;
}

item *item_cget_bdb(DBC *cursorp, char *start, size_t nstart, u_int32_t flags){
    item *it = NULL;
    DBT dbkey, dbdata;
    bool stop;
    int ret;

    /* first, alloc a fixed size */
    it = item_alloc2_bdb(settings_bdb.item_buf_size);
    if (it == 0) {
        return NULL;
    }

    BDB_CLEANUP_DBT();
    dbkey.data = start;
    dbkey.size = nstart;
    dbkey.dlen = 0;
    dbkey.doff = 0;
    dbkey.flags = DB_DBT_PARTIAL;
    dbdata.ulen = settings_bdb.item_buf_size;
    dbdata.data = it;
    dbdata.flags = DB_DBT_USERMEM;

    stop = false;
    /* try to get a item from bdb */
    while (!stop) {
        switch (ret = cursorp->get(cursorp, &dbkey, &dbdata, flags)) {
            case DB_BUFFER_SMALL:    /* user mem small */
                if (settings_bdb.verbose > 1) {
                    fprintf(stderr, "cursorp->get: %s\n", db_strerror(ret));
                }
                /* free the original smaller buffer */
                item_free(it);
                /* alloc the correct size */
                it = item_alloc2_bdb(dbdata.size);
                if (it == NULL) {
                    return NULL;
                }
                dbkey.data = start;
                dbkey.size = nstart;
                dbdata.ulen = dbdata.size;
                dbdata.data = it;
                break;
            case 0:                  /* Success. */
                stop = true;
                break;
            case DB_NOTFOUND:
                stop = true;
                item_free(it);
                it = NULL;
                break;
            default:
                /* TODO: may cause bug here, if return DB_BUFFER_SMALL then retun non-zero again
                 * here 'it' may not a full one. a item buffer larger than item_buf_size may be added to freelist */
                stop = true;
                item_free(it);
                it = NULL;
                if (settings_bdb.verbose > 1) {
                    fprintf(stderr, "cursorp->get: %s\n", db_strerror(ret));
                }
        }
    }
    return it;
}

/* 0 for Success
   -1 for SERVER_ERROR
*/
int item_put_bdb(char *key, size_t nkey, item *it){
    int ret;
    DBT dbkey, dbdata;

    BDB_CLEANUP_DBT();
    dbkey.data = key;
    dbkey.size = nkey;
    dbdata.data = it;
    dbdata.size = ITEM_ntotal(it);

    insertCode:
    switch (ret = dbp->put(dbp, NULL, &dbkey, &dbdata, 0))
    {
        case 0:
        {
            fprintf(stderr,"ret = 0 ");
            ReferencePage(lruQueue,key,nkey,it->nbytes);
            printQueue(lruQueue);
            return 0;
        }
        case ENOMEM:
        {
            fprintf(stderr,"no memory");
            int reqSize=ITEM_ntotal(it)*2;
            int curSize=0;
            while(curSize<reqSize)
            {
                struct QNode * temp=deQueue(lruQueue);
                curSize+=temp->keyLength+temp->valueLength;

                DBT dbkeyTemp;

                BDB_CLEANUP_DBT();
                dbkeyTemp.data = temp->key;
                dbkeyTemp.size = temp->keyLength;
                ret = dbp->del(dbp, NULL, &dbkeyTemp, 0);
                if (ret == 0){
                    ;
                }else if(ret == DB_NOTFOUND){
                    return 1;
                }else{
                    if (settings_bdb.verbose > 1) {
                        fprintf(stderr, "dbp->del: %s\n", db_strerror(ret));
                    }
                    return -1;
                }
                printQueue(lruQueue);
            }
            goto insertCode;

            return  0;
        }
        default: {

            if (settings_bdb.verbose > 1) {
                fprintf(stderr, "dbp->put: %s\n", db_strerror(ret));
            }
            return -1;
        }
    }
}

/* 0 for Success
   1 for NOT_FOUND
   -1 for SERVER_ERROR
*/
int item_delete_bdb(char *key, size_t nkey){
    int ret;
    DBT dbkey;

    memset(&dbkey, 0, sizeof(dbkey));
    dbkey.data = key;
    dbkey.size = nkey;
    ret = dbp->del(dbp, NULL, &dbkey, 0);
    if (ret == 0){
        deleteKey(lruQueue,key);
        printQueue(lruQueue);
        fprintf(stderr,"printed lru queue\n");
        return 0;
    }else if(ret == DB_NOTFOUND){
        return 1;
    }else{
        if (settings_bdb.verbose > 1) {
            fprintf(stderr, "dbp->del: %s\n", db_strerror(ret));
        }
        return -1;
    }
}

/*
1 for exists
0 for non-exist
*/
int item_exists_bdb(char *key, size_t nkey){
    int ret;
    DBT dbkey;

    memset(&dbkey, 0, sizeof(dbkey));
    dbkey.data = key;
    dbkey.size = nkey;
    ret = dbp->exists(dbp, NULL, &dbkey, 0);
    if (ret == 0){
        return 1;
    }
    return 0;
}
