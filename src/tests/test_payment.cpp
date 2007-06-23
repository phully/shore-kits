/* -*- mode:C++; c-basic-offset:4 -*- */

/** @file test_tpcc_load.cpp
 *
 *  @brief Test running TPC-C PAYMENT_STAGED transaction. We need to register
 *  all the trxstages.
 *
 *  @author Ippokratis Pandis (ipandis)
 */

#include "tests/common/tester_query.h"

#include "workload/register_stage_containers.h"
#include "workload/tpcc/drivers/tpcc_payment.h"


using namespace qpipe;


int main(int argc, char* argv[]) {

    query_info_t info = query_init(argc, argv, TRX_ENV);
    
    register_stage_containers(TRX_ENV);


    workload::tpcc_payment_driver driver(c_str("PAYMENT"));

    trace_set(TRACE_QUERY_RESULTS);

    query_main(info, &driver, TRX_ENV);
    return 0;
}
