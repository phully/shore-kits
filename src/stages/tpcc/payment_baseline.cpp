/* -*- mode:C++; c-basic-offset:4 -*- */

/** @file payment_baseline.h
 *
 *  @brief Implementation for the Baseline TPC-C Payment transaction
 *
 *  @author Ippokratis Pandis (ipandis)
 */

#include "stages/tpcc/payment_baseline.h"

#include "workload/tpcc/tpcc_env.h"


const c_str payment_baseline_packet_t::PACKET_TYPE = "PAYMENT_BASELINE";

const c_str payment_baseline_stage_t::DEFAULT_STAGE_NAME = "PAYMENT_BASELINE_STAGE";


/**
 *  @brief payment_baseline_stage_t constructor
 */

payment_baseline_stage_t::payment_baseline_stage_t() {
    
    TRACE(TRACE_ALWAYS, "PAYMENT_BASELINE constructor\n");
}


/**
 *  @brief Execute TPC-C Payment transaction is a conventional way
 *
 *  @return void
 *
 *  @throw May throw exceptions on error.
 */

void payment_baseline_stage_t::process_packet() {

    adaptor_t* adaptor = _adaptor;

    payment_baseline_packet_t* packet = 
	(payment_baseline_packet_t*)adaptor->get_packet();

    // Prints out the packet info
    packet->describe_trx();

    trx_result_tuple_t aTrxResultTuple = executePaymentBaseline(packet);
    

    TRACE( TRACE_ALWAYS, "DONE. NOTIFYING CLIENT\n" );
    
    // writing output 

    // create output tuple
    // "I" own tup, so allocate space for it in the stack
    size_t dest_size = packet->output_buffer()->tuple_size();
    
    array_guard_t<char> dest_data = new char[dest_size];
    tuple_t dest(dest_data, dest_size);
    
    trx_result_tuple_t* dest_result_tuple;
    dest_result_tuple = aligned_cast<trx_result_tuple_t>(dest.data);
    
    *dest_result_tuple = aTrxResultTuple;
    
    adaptor->output(dest);


} // process_packet


/** @fn executePayment
 *
 *  @brief Executes the PAYMENT transactions. Wrapper function.
 *  
 *  @return A trx_result_tuple_t with the status of the transaction.
 */
    
trx_result_tuple_t payment_baseline_stage_t::executePaymentBaseline(payment_baseline_packet_t* p) {

    // initialize the result structure
    trx_result_tuple_t aTrxResultTuple(UNDEF, p->get_trx_id());
    
    try {
        
       TRACE( TRACE_ALWAYS, "*** EXECUTING TRX CONVENTIONALLY ***\n");
    
       /** @note PAYMENT TRX According to TPC-C benchmark, Revision 5.8.0 pp. 32-35 */
    
       /** Step 1: The database transaction is started */
       TRACE( TRACE_ALWAYS, "Step 1: The database transaction is started\n" );

       dbenv->txn_begin(NULL, &p->_trx_txn, 0);
    
    
       /** Step 2: Retrieve the row in the WAREHOUSE table with matching W_ID.
        *  W_NAME, W_STREET_1, W_STREET_2, W_CITY, W_STATE and W_ZIP are retrieved.
        *  W_YTD is increased by H_AMOYNT.
        */
       TRACE( TRACE_ALWAYS, 
              "Step 2: Updating the row in the WAREHOUSE table with matching W_ID=%d\n", 
              p->_home_wh_id );

       // WAREHOUSE key: W_ID 
       Dbt key_wh(&p->_home_wh_id, sizeof(int));
       
       Dbt data_wh;
       data_wh.set_flags(DB_DBT_MALLOC);

       if (tpcc_tables[TPCC_TABLE_WAREHOUSE].db->get(p->_trx_txn, &key_wh, &data_wh, DB_RMW) == 
           DB_NOTFOUND) 
           {
               THROW2(BdbException, 
                      "warehouse with id=%d not found in the database\n", 
                      p->_home_wh_id);
           }

       tpcc_warehouse_tuple* warehouse = (tpcc_warehouse_tuple*)data_wh.get_data();
       
       assert(warehouse != NULL); // make sure that we got a warehouse
       
       warehouse->W_YTD += p->_h_amount;
       tpcc_tables[TPCC_TABLE_WAREHOUSE].db->put(p->_trx_txn, &key_wh, &data_wh, 0);       

       /** Step 3: Retrieve the row in the DISTRICT table with matching D_W_ID and D_ID.
        *  D_NAME, D_STREET_1, D_STREET_2, D_CITY, D_STATE and D_ZIP are retrieved.
        *  D_YTD is increased by H_AMOYNT.
        */
       TRACE( TRACE_ALWAYS, 
              "Step 3: Updating the row in the DISTRICT table with matching " \
              "D_ID=%d and D_W_ID=%d\n", 
              p->_home_d_id, 
              p->_home_wh_id);

       // DISTRICT key: D_ID, D_W_ID 
       tpcc_district_tuple_key dk;
       dk.D_ID = p->_home_d_id;
       dk.D_W_ID = p->_home_wh_id;
       Dbt key_d(&dk, sizeof(dk));

       /*
       memcpy(&dk.D_ID, &p->_home_d_id, sizeof(int));
       memcpy(&dk.D_W_ID, &p->_home_wh_id, sizeof(int));
       Dbt key_d(&dk.D_ID, 2 * sizeof(int));
       */


       Dbt data_d;
       data_d.set_flags(DB_DBT_MALLOC);

       if (tpcc_tables[TPCC_TABLE_DISTRICT].db->get(p->_trx_txn, &key_d, &data_d, DB_RMW) ==
           DB_NOTFOUND)
           {
               THROW3(BdbException, 
                      "district with id=(%d,%d) not found in the database\n", 
                      dk.D_ID,
                      dk.D_W_ID);
           }

       tpcc_district_tuple* district = (tpcc_district_tuple*)data_d.get_data();

       assert(district); // make sure that we got a district
       
       district->D_YTD += p->_h_amount;
       tpcc_tables[TPCC_TABLE_DISTRICT].db->put(p->_trx_txn, &key_d, &data_d, 0);       



       /** Step 4: Retrieve and Update CUSTOMER */

       // FIXME (ip) : It always chooses the customer by id. It should be 40.
       if (p->_v_cust_ident_selection < 0) {
           //       if (p->_v_cust_ident_selection < 40) {
           /** Step 4b: Retrieve CUSTOMER based on C_LAST */
           TRACE( TRACE_ALWAYS, "Step 4b: Retrieving and Updating the row in the CUSTOMER table \
                  with matching C_W_ID=%d and C_D_ID=%d and C_LAST=%s\n",
                  p->_home_wh_id,
                  p->_home_d_id, 
                  p->_c_last);
        
           if (updateCustomerByLast(p->_trx_txn, 
                                    p->_home_d_id, 
                                    p->_home_d_id, 
                                    p->_c_last, 
                                    p->_h_amount)) {
               // Failed
                
               /** FIXME (ip): Should Abort/Rollback */       
               assert (1==0);
           }
       }
       else {
           /** Step 4a: Retrieve CUSTOMER based on C_ID */
           TRACE( TRACE_ALWAYS, "Step 4a: Retrieving and Updating the row in the CUSTOMER table \
                  with matching C_W_ID=%d and C_D_ID=%d and C_ID=%d\n",
                  p->_home_wh_id,
                  p->_home_d_id, 
                  p->_c_id);
        
           if (updateCustomerByID(p->_trx_txn, 
                                  p->_home_wh_id, 
                                  p->_home_d_id, 
                                  p->_c_id, 
                                  p->_h_amount)) {
               // Failed
                 
               /** FIXME (ip): Should Abort/Rollback */       
               assert (1==0);
           }
       }
    
    
       /** Step 5: Insert a new HISTORY row. A new row is inserted into the HISTORY
        *  table with H_C_ID = C_ID, H_C_D_ID = C_D_ID, H_C_W_ID = C_W_ID, H_D_ID = D_ID,
        *  H_W_ID = W_ID and H_DATA = concatenating W_NAME and D_NAME separated by
        *  4 spaces.
        */
       TRACE( TRACE_ALWAYS, "Step 5: Inserting a new row in HISTORY table\n");

    
       /** Step 6: The database transaction is committed */
       TRACE( TRACE_ALWAYS, "Step 6: The database transaction is committed\n" );

       p->_trx_txn->commit(0);

    }
    catch(DbException err) {
        dbenv->err(err.get_errno(), "%d: Caught exception\n");
        TRACE( TRACE_ALWAYS, "DbException - Aborting PAYMENT trx...\n");
        
        p->_trx_txn->abort();
	    
        aTrxResultTuple.set_state(ROLLBACKED);
        return (aTrxResultTuple);
        
        // FIXME (ip): We may want to have retries
        //if (++failed_tries > MAX_TRIES) {
        //   packet->_trx_txn->abort();
        //   TRACE( TRACE_ALWAYS, "MAX_FAILS - Aborting...\n");        
    }
    
    return (aTrxResultTuple);

} // EOF: executePayment



/** @fn updateCustomerByID
 *
 *  @brief Step 4, Case 1: The customer is selected based on customer number.
 *  That is, the row in the CUSTOMER table with matching C_W_ID, C_D_ID and C_ID
 *  is selected. C_FIRST, C_MIDDLE, C_LAST, C_STREET_1, C_STREET_2, C_CITY, C_STATE,
 *  C_ZIP, C_PHONE, C_SINCE, C_CREDIT, C_CREDIT_LIM, C_DISCOUNT, C_BALANCE, C_DATA_1,
 *  and C_DATA_2 are retrieved.
 *  C_BALANCE is decreased by H_AMOYNT. 
 *  C_YTD_PAYMENT is increased by H_AMOYNT. 
 *  C_PAYMENT_CNT is increased by 1.
 *  updateCustomerData is called.
 *  
 *  @return 0 on success, non-zero on failure
 */
    
int payment_baseline_stage_t::updateCustomerByID(DbTxn* a_txn, 
                                                 int wh_id, 
                                                 int d_id, 
                                                 int c_id, 
                                                 decimal h_amount) 
{
    assert (a_txn); // abort if no valid transaction handle

    tpcc_customer_tuple_key ck;
    ck.C_C_ID = c_id;
    ck.C_D_ID = d_id;
    ck.C_W_ID = wh_id;
    Dbt key_c(&ck, sizeof(ck));
        
    Dbt data_c;
    data_c.set_flags(DB_DBT_MALLOC);
    
    if (tpcc_tables[TPCC_TABLE_CUSTOMER].db->get(a_txn, &key_c, &data_c, DB_RMW) ==
        DB_NOTFOUND)
        {
            TRACE( TRACE_ALWAYS,
                   "customer with id=(%d,%d,%d) not found in the database\n", 
                   ck.C_C_ID,
                   ck.C_D_ID,
                   ck.C_W_ID);
            
            THROW1( BdbException,
                    "Customer not found\n");
        }
    
    tpcc_customer_tuple* customer = (tpcc_customer_tuple*)data_c.get_data();
    
    assert(customer); // make sure that we got a customer
    
    if (customer->C_BALANCE < h_amount) {
        TRACE( TRACE_ALWAYS, 
               "balance (%f) < (%f) h_amount. Aborting trx...\n",
               customer->C_BALANCE,
               h_amount.to_double());
        return (1);
    }

    // updating customer data
    customer->C_BALANCE -= h_amount;
    customer->C_YTD_PAYMENT += h_amount;
    customer->C_PAYMENT_CNT++;
    
    
    if (strncmp(customer->C_CREDIT, "BC", 2) == 0) {
        updateCustomerData(customer);
    }
    
    return (tpcc_tables[TPCC_TABLE_DISTRICT].db->put(a_txn, &key_c, &data_c, 0));
}




/** @fn updateCustomerByLast
 *
 *  @brief Step 4, Case 2: The customer is selected based on customer last name.
 *  That is, all rows in the CUSTOMER table with matching C_W_ID, C_D_ID and C_LAST
 *  are selected sorted by C_FIRST in ascending order. Let (n) be the number of rows
 *  selected. C_ID, C_FIRST, C_MIDDLE, C_LAST, C_STREET_1, C_STREET_2, C_CITY, C_STATE,
 *  C_ZIP, C_PHONE, C_SINCE, C_CREDIT, C_CREDIT_LIM, C_DISCOUNT, C_BALANCE, C_DATA_1,
 *  and C_DATA_2 are retrieved from the row at position (n/2 rounded up to the next
 *  integer) in the sorted set of selected rows.
 *  C_BALANCE is decreased by H_AMOYNT. 
 *  C_YTD_PAYMENT is increased by H_AMOYNT. 
 *  C_PAYMENT_CNT is increased by 1.
 *  updateCustomerData is called.
 *  
 *  @return 0 on success, non-zero on failure
 */
    
int payment_baseline_stage_t::updateCustomerByLast(DbTxn* a_txn, 
                                                   int wh_id, 
                                                   int d_id, 
                                                   char* c_last,
                                                   decimal h_amount) 
{
    assert(a_txn); // abort if no valid transaction handle 

    int iResult = 0;

    tpcc_customer_tuple a_customer;
    
    TRACE( TRACE_ALWAYS, "*** TO BE DONE! Updating Customer using C_LAST\n");
    
    /** Retrieving CUSTOMER rows using C_LAST (and a cursor) */
    
    if (strncmp(a_customer.C_CREDIT, "BC", 2) == 0) {
        updateCustomerData(&a_customer);
    }

    // FIXME (ip) Not implemented yet
    assert( 1 == 0);
 
   
    return (iResult);
}
    
    
/** @fn updateCustomerData
 *
 *  @brief If the value of C_CREDIT is equal to "BC", then the following history
 *  information: C_ID, C_D_ID, C_W_ID, D_ID, W_ID, and H_AMOYNT, are inserted at
 *  the left of C_DATA field by shifting the existing content of C_DATA to the right
 *  by an equal number of bytes and by discarding the bytes that are shifted out of
 *  the right side of the C_DATA field. The content of the C_DATA field never exceeds
 *  500 characters. The selected customer is updated with the new C_DATA field.
 */
    
void payment_baseline_stage_t::updateCustomerData(tpcc_customer_tuple* a_customer) 
{
    assert( a_customer ); // make sure correct parameters

    assert((strncmp(a_customer->C_CREDIT, "BC", 2) == 0) &&
           a_customer->C_DATA_1 &&
           a_customer->C_DATA_2);


    TRACE( TRACE_ALWAYS, "*** TO BE DONE! Updating Customer Data\n");
    assert ( 1 == 0); // FIXME (ip) : Remove
}


    
