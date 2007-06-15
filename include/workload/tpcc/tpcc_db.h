/* -*- mode:C++; c-basic-offset:4 -*- */

/** @file tpcc_db.h
 *
 *  @brief Interface for the functionality that creates and configures
 *  the transaction processing database. The current implemetnation uses 
 *  BerkeleyDB as the underlying storage manager, logging, and locking 
 *  engine.
 *
 *  @author Ippokratis Pandis (ipandis)
 */

#ifndef __TPCC_DB_H
#define __TPCC_DB_H

#include <inttypes.h>


//#include "workload/tpch/tpch_env.h"
//#include "workload/tpch/tpch_db_load.h"

ENTER_NAMESPACE(tpcc);


/** Transaction processing engine parameters */

/** @note Define the maximum number of lockers, locks and locked objects. 
 *  BDB's default value is 1000 for each of them. This value low and may 
 *  result to ENOMEM errors at run-time, especially when the number of 
 *  clients is high.
 */
#define BDB_MAX_LOCKERS 40000
#define BDB_MAX_LOCKS   40000
#define BDB_MAX_OBJECTS 40000


/** @note Define the maximum number of possible in-flight
 *  transactions. BDB's default value is 10.
 */
#define BDB_MAX_TRX     100


/** @note Define the timeout value for the transactions.
 *  BDB's default value is 0, which means that there is 
 *  no timeout.
 */
#define BDB_SEC         1000000
#define BDB_TRX_TIMEOUT 0 * BDB_SEC


/** @note Define the dbopen flags. This flag defines, among others,
 *  the possible isolation level. BDB's default isolation level is
 *  SERIALIZABLE (level 3). Other possible values are:
 *  READ UNCOMMITTED (level 1) - Allows reads of dirtied (uncommitted) data.
 *  READ COMMITTED (level 2)   - Releases read locks before trx ends.
 *
 *  @note In order to enable READ UNCOMMITTED isolation we must pass this
 *  parameter in the dbopen function.
 */
#define BDB_TPCC_DB_OPEN_FLAGS DB_CREATE
//#define BDB_TPCC_DB_OPEN_FLAGS DB_CREATE | DB_READ_UNCOMMITTED


/** Exported functions */

void db_open(uint32_t flags,
             uint32_t db_cache_size_gb=1,
             uint32_t db_cache_size_bytes=0);

void db_close();

void db_read();

EXIT_NAMESPACE(tpcc);

#endif