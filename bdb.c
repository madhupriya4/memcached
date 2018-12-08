#include "memcached.h"
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <signal.h>
#include <db.h>

void bdb_env_init(void){
    int ret;
    const char *db_name = "in_mem_db1";
    u_int32_t open_flags;

    /* Create the environment */
    ret = db_env_create(&env, 0);
    if (ret != 0) {
        fprintf(stderr, "Error creating environment handle: %s\n",
                db_strerror(ret));
    }

    open_flags =
            DB_CREATE     |  /* Create the environment if it does not exist */
            DB_INIT_MPOOL |  /* Initialize the memory pool (in-memory cache) */
            DB_PRIVATE;      /* Region files are not backed by the filesystem.
                        * Instead, they are backed by heap memory.  */

    /*
     * Specify the size of the in-memory cache.
     */
    ret = env->set_cachesize(env, 0, CACHE_SIZE, 1);
    if (ret != 0) {
        fprintf(stderr, "Error increasing the cache size: %s\n",
                db_strerror(ret));
    }

    /*
     * Now actually open the environment. Notice that the environment home
     * directory is NULL. This is required for an in-memory only
     * application.
     */
    ret = env->open(env, NULL, open_flags, 0);
    if (ret != 0) {
        fprintf(stderr, "Error opening environment: %s\n",
                db_strerror(ret));
    }


    /* Initialize the DB handle */
    ret = db_create(&dbp, env, 0);
    if (ret != 0) {
        env->err(env, ret,
                  "Attempt to create db handle failed.");
    }

    /*
     * Set the database open flags. Autocommit is used because we are
     * transactional.
     */
    open_flags = DB_CREATE;
    ret = dbp->open(dbp,         /* Pointer to the database */
                    NULL,        /* Txn pointer */
                    NULL,        /* File name -- Must be NULL for inmemory! */
                    db_name,     /* Logical db name */
                    DB_HASH,    /* Database type (using btree) */
                    open_flags,  /* Open flags */
                    0);          /* File mode. Using defaults */

    if (ret != 0) {
        env->err(env, ret,
                 "Attempt to open db failed.");
    }

    /* Configure the cache file */
    mpf = dbp->get_mpf(dbp);
    ret = mpf->set_flags(mpf, DB_MPOOL_NOFILE, 1);

    if (ret != 0) {
        env->err(env, ret,
                  "Attempt failed to configure for no backing of temp files.");
    }

    /* Final status message and return. */
    printf("I'm all done.\n");
}


/* for atexit cleanup */
void bdb_db_close(void){
    int ret_t = 0;
    /* Close our database handle, if it was opened. */
    if (dbp != NULL) {
        ret_t = dbp->close(dbp, 0);
        if (ret_t != 0) {
            fprintf(stderr, "%s database close failed.\n",
                    db_strerror(ret_t));
        }
    }
}

/* for atexit cleanup */
void bdb_env_close(void){
    int ret_t = 0;
    /* Close our environment, if it was opened. */
    if (env != NULL) {
        ret_t = env->close(env, 0);
        if (ret_t != 0) {
            fprintf(stderr, "environment close failed: %s\n",
                    db_strerror(ret_t));
        }
    }
}
