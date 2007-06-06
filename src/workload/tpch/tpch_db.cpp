/* -*- mode:C++; c-basic-offset:4 -*- */

#include "util.h"

#include "workload/common/bdb_env.h"
#include "workload/bdb_config.h"

#include "workload/common.h"

#include "workload/tpch/tpch_env.h"
#include "workload/tpch/tpch_filenames.h"
#include "workload/tpch/tpch_compare.h"


using namespace qpipe;
using namespace workload;
using namespace tpch;

/* definitions of exported functions */


/**
 *  @brief Open TPC-H tables.
 *
 *  @return void
 *
 *  @throw BdbException on error.
 */
void tpch::db_open(u_int32_t flags, u_int32_t db_cache_size_gb, 
                   u_int32_t db_cache_size_bytes) 
{

    TRACE(TRACE_ALWAYS,
          "TPC-H DB_OPEN called\n");


    // create environment
    try {
        dbenv = new DbEnv(0);
        dbenv->set_errpfx(BDB_ERROR_PREFIX);
    }
    catch (DbException &e) {
        TRACE(TRACE_ALWAYS, "Caught DbException creating new DbEnv object: %s\n", 
              e.what());
        THROW1(BdbException, "Could not create new DbEnv");
    }
  
    // specify buffer pool size
    try {
        if (dbenv->set_cachesize(db_cache_size_gb, db_cache_size_bytes, 0)) {
            TRACE(TRACE_ALWAYS, "dbenv->set_cachesize() failed!\n");
            THROW1(BdbException, "dbenv->set_cachesize() failed!\n");
        }
    }
    catch (DbException &e) {
        TRACE(TRACE_ALWAYS, 
              "Caught DbException setting buffer pool size to %d GB, %d B: %s\n",
              db_cache_size_gb,
              db_cache_size_bytes,
              e.what());
        THROW1(BdbException, "dbenv->set_cachesize() threw DbException");
    }
  
  
    // set data directory
    try {

        const char* desc = "BDB_TPCH_DIRECTORY (BDB data)";
        const char* dir  = BDB_TPCH_DIRECTORY;

        if (fileops_check_directory_accessible(dir))
            THROW3(BdbException, "%s %s not accessible.\n",
                   desc,
                   dir);

        // data directory stores table files
        dbenv->set_data_dir(BDB_TPCH_DIRECTORY);
    }
    catch (DbException &e) {
        TRACE(TRACE_ALWAYS,
              "Caught DbException setting data directory to \"%s\"."
              " Make sure directory exists\n",
              BDB_TPCH_DIRECTORY);
        TRACE(TRACE_ALWAYS, "DbException: %s\n", e.what());
        THROW1(BdbException, "dbenv->set_data_dir() threw DbException");
    }
  
  
    // set temp directory
    try {

        const char* desc = "BDB_TEMP_DIRECTORY (BDB temp)";
        const char* dir  = BDB_TEMP_DIRECTORY;

        if (fileops_check_directory_accessible(dir))
            THROW3(BdbException, "%s %s not accessible.\n",
                   desc,
                   dir);

        if (fileops_check_file_writeable(dir))
            THROW3(BdbException, "%s %s not writeable.\n",
                   desc,
                   dir);

        dbenv->set_tmp_dir(BDB_TEMP_DIRECTORY);
    }
    catch ( DbException &e) {
        TRACE(TRACE_ALWAYS,
              "Caught DbException setting temp directory to \"%s\"."
              " Make sure directory exists\n",
              BDB_TEMP_DIRECTORY);
        TRACE(TRACE_ALWAYS, "DbException: %s\n", e.what());
        THROW1(BdbException, "dbenv->set_tmp_dir() threw DbException");
    }

    dbenv->set_msgfile(stderr);
    dbenv->set_errfile(stderr);
    // TODO: DB_NOMMAP?
    // dbenv->set_flags(DB_NOMMAP, true);
    dbenv->set_verbose(DB_VERB_REGISTER, true);
    

    // open home directory
    try {
        
        const char* desc = "BDB_HOME_DIRECTORY (BDB home)";
        const char* dir  = BDB_HOME_DIRECTORY;

        if (fileops_check_directory_accessible(dir))
            THROW3(BdbException, "%s %s not accessible.\n",
                   desc,
                   dir);

        if (fileops_check_file_writeable(dir))
            THROW3(BdbException, "%s %s not writeable.\n",
                   desc,
                   dir);

        // open environment with no transactional support.
        dbenv->open(BDB_HOME_DIRECTORY,
                    DB_CREATE | DB_PRIVATE | DB_THREAD | DB_INIT_CDB | DB_INIT_MPOOL,
                    0);
    }
    catch ( DbException &e) {
        TRACE(TRACE_ALWAYS,
              "Caught DbException opening home directory \"%s\"."
              " Make sure directory exists\n",
              BDB_HOME_DIRECTORY);
        TRACE(TRACE_ALWAYS, "DbException: %s\n", e.what());
        THROW1(BdbException, "dbenv->open() threw DbException");
    }
  

    // open tables
    for (int i = 0; i < _TPCH_TABLE_COUNT_; i++)
        open_db_table(tpch_tables[i].db,
                                flags,
                                tpch_tables[i].bt_compare_fn,
                                tpch_tables[i].bdb_filename);
    

    // open indexes
    open_db_index(tpch_tables[TPCH_TABLE_LINEITEM].db,
                            tpch_lineitem_shipdate,
                            tpch_lineitem_shipdate_idx,
                            flags,
                            tpch_lineitem_shipdate_compare_fcn,
                            tpch_lineitem_shipdate_key_fcn,
                            INDEX_LINEITEM_SHIPDATE_NAME,
                            INDEX_LINEITEM_SHIPDATE_NAME "_IDX");
    
    TRACE(TRACE_ALWAYS, "BerkeleyDB buffer pool set to %d GB, %d B\n",
          db_cache_size_gb,
          db_cache_size_bytes);
    TRACE(TRACE_ALWAYS, "TPC-H database open\n");
}



/**
 *  @brief Close TPC-H tables.
 *
 *  @return void
 *
 *  @throw BdbException on error.
 */
void tpch::db_close() {

    // close indexes
    close_db_table(tpch_lineitem_shipdate_idx, INDEX_LINEITEM_SHIPDATE_NAME "_IDX");
    close_db_table(tpch_lineitem_shipdate, INDEX_LINEITEM_SHIPDATE_NAME);

    // close tables
    for (int i = 0; i < _TPCH_TABLE_COUNT_; i++)
        close_db_table(tpch_tables[i].db, tpch_tables[i].bdb_filename);
    
    // close environment
    try {    
 	dbenv->close(0);
    }
    catch ( DbException &e) {
	TRACE(TRACE_ALWAYS, "Caught DbException closing environment\n");
        TRACE(TRACE_ALWAYS, "DbException: %s\n", e.what());
        THROW1(BdbException, "dbenv->close() threw DbException");
    }


    TRACE(TRACE_ALWAYS, "TPC-H database closed\n");
}
