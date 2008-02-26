/* -*- mode:C++; c-basic-offset:4 -*- */

/** @file shore_tpcc_env.cpp
 *
 *  @brief Declaration of the Shore TPC-C environment (database)
 *
 *  @author Ippokratis Pandis (ipandis)
 */

#include "stages/tpcc/shore/shore_tpcc_env.h"
#include "stages/tpcc/common/tpcc_trx_input.h"
#include "sm/shore/shore_sort_buf.h"
#include "sm/shore/shore_helper_loader.h"


using namespace shore;

ENTER_NAMESPACE(tpcc);


/** Exported variables */

ShoreTPCCEnv* shore_env;
tpcc_random_gen_t ShoreTPCCEnv::_atpccrndgen(NULL);

/** Exported functions */


/******************************************************************** 
 *
 *  @fn:    print_trx_stats
 *
 *  @brief: Prints trx statistics
 *
 ********************************************************************/

void tpcc_stats_t::print_trx_stats() 
{
    TRACE( TRACE_STATISTICS, "=====================================\n");
    TRACE( TRACE_STATISTICS, "TPC-C Database transaction statistics\n");
    TRACE( TRACE_STATISTICS, "NEW-ORDER\n");
    CRITICAL_SECTION(no_cs,  _no_lock);
    TRACE( TRACE_STATISTICS, "Attempted: %d\n", _no_att);
    TRACE( TRACE_STATISTICS, "Committed: %d\n", _no_com);
    TRACE( TRACE_STATISTICS, "Aborted  : %d\n", (_no_att-_no_com));
    TRACE( TRACE_STATISTICS, "PAYMENT\n");
    CRITICAL_SECTION(pay_cs, _pay_lock);
    TRACE( TRACE_STATISTICS, "Attempted: %d\n", _pay_att);
    TRACE( TRACE_STATISTICS, "Committed: %d\n", _pay_com);
    TRACE( TRACE_STATISTICS, "Aborted  : %d\n", (_pay_att-_pay_com));
    TRACE( TRACE_STATISTICS, "ORDER-STATUS\n");
    CRITICAL_SECTION(ord_cs, _ord_lock);
    TRACE( TRACE_STATISTICS, "Attempted: %d\n", _ord_att);
    TRACE( TRACE_STATISTICS, "Committed: %d\n", _ord_com);
    TRACE( TRACE_STATISTICS, "Aborted  : %d\n", (_ord_att-_ord_com));
    TRACE( TRACE_STATISTICS, "DELIVERY\n");
    CRITICAL_SECTION(del_cs, _del_lock);
    TRACE( TRACE_STATISTICS, "Attempted: %d\n", _del_att);
    TRACE( TRACE_STATISTICS, "Committed: %d\n", _del_com);
    TRACE( TRACE_STATISTICS, "Aborted  : %d\n", (_del_att-_del_com));
    TRACE( TRACE_STATISTICS, "STOCK-LEVEL\n");
    CRITICAL_SECTION(sto_cs, _sto_lock);
    TRACE( TRACE_STATISTICS, "Attempted: %d\n", _sto_att);
    TRACE( TRACE_STATISTICS, "Committed: %d\n", _sto_com);
    TRACE( TRACE_STATISTICS, "Aborted  : %d\n", (_sto_att-_sto_com));
    TRACE( TRACE_STATISTICS, "=====================================\n");
}



/********
 ******** Caution: The functions below should be invoked inside
 ********          the context of a smthread
 ********/


/****************************************************************** 
 *
 * @fn:    loaddata()
 *
 * @brief: Loads the data for all the TPCC tables, given the current
 *         scaling factor value. During the loading the SF cannot be
 *         changed.
 *
 ******************************************************************/

w_rc_t ShoreTPCCEnv::loaddata() 
{
    /* 0. lock the loading status and the scaling factor */
    critical_section_t load_cs(_load_mutex);
    if (_loaded) {
        TRACE( TRACE_ALWAYS, 
               "Env already loaded. Doing nothing...\n");
        return (RCOK);
    }        
    critical_section_t scale_cs(_scaling_mutex);


    /* 1. create the loader threads */
    tpcc_table_t* ptable = NULL;

    // (ip) for debug loading only the first 2 tables (WH, DISTR)
    //    int num_tbl = 4; // if 4 == PAYMENT TABLES
    int num_tbl = _table_list.size();

    const char* loaddatadir = _dev_opts[SHORE_DEF_DEV_OPTIONS[3][0]].c_str();
    int cnt = 0;
    table_loading_smt_t* loaders[SHORE_TPCC_TABLES];
    time_t tstart = time(NULL);

    TRACE( TRACE_DEBUG, "Loaddir (%s)\n", loaddatadir);

    for(tpcc_table_list_iter table_iter = _table_list.begin(); 
        table_iter != _table_list.end(); table_iter++)
        {
            ptable = *table_iter;
            loaders[cnt] = new table_loading_smt_t(c_str("ld%d", cnt), 
                                                   _pssm, ptable, 
                                                   _scaling_factor,
                                                   loaddatadir);
            cnt++;
        }


#if 1
    /* 3. fork the loading threads (PARALLEL) */
    for(int i=0; i<num_tbl; i++) {
	loaders[i]->fork();
    }

    /* 4. join the loading threads */
    for(int i=0; i<num_tbl; i++) {
	loaders[i]->join();        
        if (loaders[i]->rv()) {
            TRACE( TRACE_ALWAYS, "Error while loading (%s) *****\n",
                   loaders[i]->table()->name());
            delete loaders[i];
            return RC(se_ERROR_IN_LOAD);
        }
        TRACE( TRACE_TRX_FLOW, "Loader (%d) [%s] joined...\n", 
               i, loaders[i]->table()->name());
        delete loaders[i];
    }    
#else 
    /* 3. fork & join the loading threads SERIALLY */
    for(int i=0; i<num_tbl; i++) {
	loaders[i]->fork();
	loaders[i]->join();        
        if (loaders[i]->rv()) {
            TRACE( TRACE_ALWAYS, "Error while loading (%s) *****\n",
                   loaders[i]->table()->name());
            delete loaders[i];
            return RC(se_ERROR_IN_LOAD);
        }        
        delete loaders[i];
    }
#endif
    time_t tstop = time(NULL);

    /* 5. print stats */
    TRACE( TRACE_STATISTICS, "Loading finished. %d table loaded in (%d) secs...\n",
           num_tbl, (tstop - tstart));

    /* 6. notify that the env is loaded */
    _loaded = true;

    return (RCOK);
}



/****************************************************************** 
 *
 * @fn:    check_consistency()
 *
 * @brief: Iterates over all tables and checks consistency between
 *         the values stored in the base table (file) and the 
 *         corresponding indexes.
 *
 ******************************************************************/

w_rc_t ShoreTPCCEnv::check_consistency()
{
    /* 1. create the checker threads */
    tpcc_table_t* ptable = NULL;
    int num_tbl = _table_list.size();
    int cnt = 0;
    guard<table_checking_smt_t> checkers[SHORE_TPCC_TABLES];

    for(tpcc_table_list_iter table_iter = _table_list.begin(); 
        table_iter != _table_list.end(); table_iter++)
        {
            ptable = *table_iter;
            checkers[cnt] = new table_checking_smt_t(c_str("chk%d", cnt), 
                                                     _pssm, ptable);
            cnt++;
        }

#if 1
    /* 2. fork the threads */
    cnt = 0;
    time_t tstart = time(NULL);
    for(int i=0; i<num_tbl; i++) {
	checkers[i]->fork();
    }

    /* 3. join the threads */
    cnt = 0;
    for(int i=0; i < num_tbl; i++) {
	checkers[i]->join();
    }    
    time_t tstop = time(NULL);
#else
    /* 2. fork & join the threads SERIALLY */
    cnt = 0;
    time_t tstart = time(NULL);
    for(int i=0; i<num_tbl; i++) {
	checkers[i]->fork();
	checkers[i]->join();
    }
    time_t tstop = time(NULL);
#endif
    /* 4. print stats */
    TRACE( TRACE_DEBUG, "Checking finished in (%d) secs...\n",
           (tstop - tstart));
    TRACE( TRACE_DEBUG, "%d tables checked...\n", num_tbl);
    return (RCOK);
}



void ShoreTPCCEnv::set_qf(const int aQF)
{
    if ((aQF >= 0) && (aQF <= _scaling_factor)) {
        critical_section_t cs(_queried_mutex);
        TRACE( TRACE_ALWAYS, "New Queried factor: %d\n", aQF);
        _queried_factor = aQF;
    }
    else {
        TRACE( TRACE_ALWAYS, "Invalid queried factor input: %d\n", aQF);
    }
}



void ShoreTPCCEnv::dump()
{
    tpcc_table_t* ptable = NULL;
    int cnt = 0;
    for(tpcc_table_list_iter table_iter = _table_list.begin(); 
        table_iter != _table_list.end(); table_iter++)
        {
            ptable = *table_iter;
            ptable->print_table(this->_pssm);
            cnt++;

            // (ip) print only the first 4 tables
            if (cnt == 4)
                break;
        }
    
}



/******************************************************************** 
 *
 * TPC-C TRXS
 *
 * (1) The run_XXX functions are wrappers to the real transactions
 * (2) The xct_XXX functions are the implementation of the transactions
 *
 ********************************************************************/


/******************************************************************** 
 *
 * TPC-C TRXs Wrappers
 *
 * @brief: They are wrappers to the functions that execute the transaction
 *         body. Their responsibility is to:
 *
 *         1. Prepare the corresponding input
 *         2. Check the return of the trx function and abort the trx,
 *            if something went wrong
 *         3. Update the tpcc db environment statistics
 *
 ********************************************************************/


/* --- with input specified --- */

w_rc_t ShoreTPCCEnv::run_new_order(const int xct_id, 
                                   new_order_input_t& anoin,
                                   trx_result_tuple_t& atrt)
{
    TRACE( TRACE_TRX_FLOW, "%d. NEW-ORDER...\n", xct_id);     
    
    w_rc_t e = xct_new_order(&anoin, xct_id, atrt);
    if (e) {
        TRACE( TRACE_ALWAYS, "Xct (%d) NewOrder aborted [0x%x]\n", 
               xct_id, e.err_num());
        _tpcc_stats.inc_no_att();
        _env_stats.inc_trx_att();
        W_DO(_pssm->abort_xct());

        // (ip) could retry
        return (e);
    }

    TRACE( TRACE_TRX_FLOW, "Xct (%d) NewOrder completed\n", xct_id);
    _tpcc_stats.inc_no_com();
    _env_stats.inc_trx_com();
    return (RCOK); 
}

w_rc_t ShoreTPCCEnv::run_payment(const int xct_id, 
                                 payment_input_t& apin,
                                 trx_result_tuple_t& atrt)
{
    TRACE( TRACE_TRX_FLOW, "%d. PAYMENT...\n", xct_id);     
    
    w_rc_t e = xct_payment(&apin, xct_id, atrt);
    if (e) {
        TRACE( TRACE_ALWAYS, "Xct (%d) Payment aborted [0x%x]\n", 
               xct_id, e.err_num());
        _tpcc_stats.inc_pay_att();
        _env_stats.inc_trx_att();
        W_DO(_pssm->abort_xct());

        // (ip) could retry
        return (e);
    }

    TRACE( TRACE_TRX_FLOW, "Xct (%d) Payment completed\n", xct_id);
    _tpcc_stats.inc_pay_com();
    _env_stats.inc_trx_com();
    return (RCOK); 
}

w_rc_t ShoreTPCCEnv::run_order_status(const int xct_id, 
                                      order_status_input_t& aordstin,
                                      trx_result_tuple_t& atrt)
{
    TRACE( TRACE_TRX_FLOW, "%d. ORDER-STATUS...\n", xct_id);     
    
    w_rc_t e = xct_order_status(&aordstin, xct_id, atrt);
    if (e) {
        TRACE( TRACE_ALWAYS, "Xct (%d) OrderStatus aborted [0x%x]\n", 
               xct_id, e.err_num());
        _tpcc_stats.inc_ord_att();
        _env_stats.inc_trx_att();
        W_DO(_pssm->abort_xct());

        // (ip) could retry
        return (e);
    }

    TRACE( TRACE_TRX_FLOW, "Xct (%d) OrderStatus completed\n", xct_id);
    _tpcc_stats.inc_ord_com();
    _env_stats.inc_trx_com();
    return (RCOK); 
}

w_rc_t ShoreTPCCEnv::run_delivery(const int xct_id, 
                                  delivery_input_t& adelin,
                                  trx_result_tuple_t& atrt)
{
    TRACE( TRACE_TRX_FLOW, "%d. DELIVERY...\n", xct_id);     
    
    w_rc_t e = xct_delivery(&adelin, xct_id, atrt);
    if (e) {
        TRACE( TRACE_ALWAYS, "Xct (%d) Delivery aborted [0x%x]\n", 
               xct_id, e.err_num());
        _tpcc_stats.inc_del_att();
        _env_stats.inc_trx_att();
        W_DO(_pssm->abort_xct());

        // (ip) could retry
        return (e);
    }

    TRACE( TRACE_TRX_FLOW, "Xct (%d) Delivery completed\n", xct_id);
    _tpcc_stats.inc_del_com();
    _env_stats.inc_trx_com();
    return (RCOK); 
}

w_rc_t ShoreTPCCEnv::run_stock_level(const int xct_id, 
                                     stock_level_input_t& astoin,
                                     trx_result_tuple_t& atrt)
{
    TRACE( TRACE_TRX_FLOW, "%d. STOCK-LEVEL...\n", xct_id);     
    
    w_rc_t e = xct_stock_level(&astoin, xct_id, atrt);
    if (e) {
        TRACE( TRACE_ALWAYS, "Xct (%d) StockLevel aborted [0x%x]\n", 
               xct_id, e.err_num());
        _tpcc_stats.inc_sto_att();
        _env_stats.inc_trx_att();
        W_DO(_pssm->abort_xct());

        // (ip) could retry
        return (e);
    }

    TRACE( TRACE_TRX_FLOW, "Xct (%d) StockLevel completed\n", xct_id);
    _tpcc_stats.inc_sto_com();
    _env_stats.inc_trx_com();
    return (RCOK); 
}



/* --- without input specified --- */

w_rc_t ShoreTPCCEnv::run_new_order(const int xct_id, trx_result_tuple_t& atrt)
{
    new_order_input_t noin = create_no_input();
    return (run_new_order(xct_id, noin, atrt));
}


w_rc_t ShoreTPCCEnv::run_payment(const int xct_id, trx_result_tuple_t& atrt)
{
    payment_input_t pin = create_payment_input();
    return (run_payment(xct_id, pin, atrt));
}


w_rc_t ShoreTPCCEnv::run_order_status(const int xct_id, trx_result_tuple_t& atrt)
{
    order_status_input_t ordin = create_order_status_input();
    return (run_order_status(xct_id, ordin, atrt));
}


w_rc_t ShoreTPCCEnv::run_delivery(const int xct_id, trx_result_tuple_t& atrt)
{
    delivery_input_t delin = create_delivery_input();
    return (run_delivery(xct_id, delin, atrt));
}


w_rc_t ShoreTPCCEnv::run_stock_level(const int xct_id, trx_result_tuple_t& atrt)
{
    stock_level_input_t slin = create_stock_level_input();
    return (run_stock_level(xct_id, slin, atrt));
}



/******************************************************************** 
 *
 * @note: The functions below are private, the corresponding run_XXX are
 *        their public wrappers. The run_XXX are required because they
 *        do the trx abort in case something goes wrong inside the body
 *        of each of the transactions.
 *
 ********************************************************************/


// uncomment the line below if want to dump (part of) the trx results
//#define PRINT_TRX_RESULTS


/******************************************************************** 
 *
 * TPC-C NEW_ORDER
 *
 ********************************************************************/

w_rc_t ShoreTPCCEnv::xct_new_order(new_order_input_t* pnoin, 
                                   const int xct_id, 
                                   trx_result_tuple_t& trt)
{
    // ensure a valid environment
    assert (pnoin);
    assert (_pssm);
    assert (_initialized);
    assert (_loaded);

    // get a timestamp
    time_t tstamp = time(NULL);

    // new_order trx touches 8 tables:
    // warehouse, district, customer, neworder, order, item, stock, orderline
    table_row_t rwh(&_warehouse);
    table_row_t rdist(&_district);
    table_row_t rcust(&_customer);
    table_row_t rno(&_new_order);
    table_row_t rord(&_order);
    table_row_t ritem(&_item);
    table_row_t rst(&_stock);
    table_row_t rol(&_order_line);
    trt.reset(UNSUBMITTED, xct_id);

    /* 0. initiate transaction */
    W_DO(_pssm->begin_xct());


    /* SELECT c_discount, c_last, c_credit, w_tax
     * FROM customer, warehouse
     * WHERE w_id = :w_id AND c_w_id = w_id AND c_d_id = :d_id AND c_id = :c_id
     *
     * plan: index probe on "W_INDEX", index probe on "C_INDEX"
     */

    /* 1. retrieve warehouse for update */
    TRACE( TRACE_TRX_FLOW, 
           "App: %d NO:warehouse-index-probe (%d)\n", 
           xct_id, pnoin->_wh_id);
    W_DO(_warehouse.index_probe(_pssm, &rwh, pnoin->_wh_id));

    tpcc_warehouse_tuple awh;
    rwh.get_value(7, awh.W_TAX);


    /* 2. retrieve district for update */
    TRACE( TRACE_TRX_FLOW, 
           "App: %d NO:district-index-probe (%d) (%d)\n", 
           xct_id, pnoin->_wh_id, pnoin->_d_id);
    W_DO(_district.index_probe_forupdate(_pssm, &rdist, 
                                         pnoin->_d_id, pnoin->_wh_id));

    /* SELECT d_tax, d_next_o_id
     * FROM district
     * WHERE d_id = :d_id AND d_w_id = :w_id
     *
     * plan: index probe on "D_INDEX"
     */

    tpcc_district_tuple adist;
    rdist.get_value(8, adist.D_TAX);
    rdist.get_value(10, adist.D_NEXT_O_ID);
    adist.D_NEXT_O_ID++;


    /* 3. retrieve customer */
    TRACE( TRACE_TRX_FLOW, 
           "App: %d NO:customer-index-probe (%d) (%d) (%d)\n", 
           xct_id, pnoin->_wh_id, pnoin->_d_id, pnoin->_c_id);
    W_DO(_customer.index_probe(_pssm, &rcust, pnoin->_c_id, 
                               pnoin->_wh_id, pnoin->_d_id));

    tpcc_customer_tuple  acust;
    rcust.get_value(15, acust.C_DISCOUNT);
    rcust.get_value(13, acust.C_CREDIT, 3);
    rcust.get_value(5, acust.C_LAST, 17);


    /* UPDATE district
     * SET d_next_o_id = :next_o_id+1
     * WHERE CURRENT OF dist_cur
     */

    TRACE( TRACE_TRX_FLOW, "App: %d NO:district-update-next-o-id\n", xct_id);
    W_DO(_district.update_next_o_id(_pssm, &rdist, adist.D_NEXT_O_ID));
    double total_amount = 0;
    int all_local = 0;

    for (int item_cnt = 0; item_cnt < pnoin->_ol_cnt; item_cnt++) {

        /* 4. for all items update item, stock, and order line */
	register int ol_i_id = pnoin->items[item_cnt]._ol_i_id;
	register int ol_supply_w_id = pnoin->items[item_cnt]._ol_supply_wh_id;


	/* SELECT i_price, i_name, i_data
	 * FROM item
	 * WHERE i_id = :ol_i_id
	 *
	 * plan: index probe on "I_INDEX"
	 */

        tpcc_item_tuple aitem;
        TRACE( TRACE_TRX_FLOW, "App: %d NO:item-index-probe (%d)\n", 
               xct_id, ol_i_id);
	W_DO(_item.index_probe(_pssm, &ritem, ol_i_id));

        ritem.get_value(4, aitem.I_DATA, 51);
	ritem.get_value(3, aitem.I_PRICE);
	ritem.get_value(2, aitem.I_NAME, 25);

        int item_amount = aitem.I_PRICE * pnoin->items[item_cnt]._ol_quantity; 
        total_amount += item_amount;
        //	info->items[item_cnt].ol_amount = amount;


	/* SELECT s_quantity, s_remote_cnt, s_data, s_dist0, s_dist1, s_dist2, ...
	 * FROM stock
	 * WHERE s_i_id = :ol_i_id AND s_w_id = :ol_supply_w_id
	 *
	 * plan: index probe on "S_INDEX"
	 */

        tpcc_stock_tuple astock;
        TRACE( TRACE_TRX_FLOW, "App: %d NO:stock-index-probe (%d) (%d)\n", 
               xct_id, ol_i_id, ol_supply_w_id);
	W_DO(_stock.index_probe_forupdate(_pssm, &rst, ol_i_id, ol_supply_w_id));

        rst.get_value(0, astock.S_I_ID);
        rst.get_value(1, astock.S_W_ID);
        rst.get_value(5, astock.S_YTD);
        astock.S_YTD += pnoin->items[item_cnt]._ol_quantity;
	rst.get_value(2, astock.S_REMOTE_CNT);        
        rst.get_value(3, astock.S_QUANTITY);
        astock.S_QUANTITY -= pnoin->items[item_cnt]._ol_quantity;
        if (astock.S_QUANTITY < 10) astock.S_QUANTITY += 91;
        rst.get_value(6+pnoin->_d_id, astock.S_DIST[6+pnoin->_d_id], 25);
	rst.get_value(16, astock.S_DATA, 51);

        char c_s_brand_generic;
	if (strstr(aitem.I_DATA, "ORIGINAL") != NULL && 
            strstr(astock.S_DATA, "ORIGINAL") != NULL)
	    c_s_brand_generic = 'B';
	else c_s_brand_generic = 'G';

	rst.get_value(4, astock.S_ORDER_CNT);
        astock.S_ORDER_CNT++;

	if (pnoin->_wh_id != ol_supply_w_id) {
            astock.S_REMOTE_CNT++;
	    all_local = 1;
	}


	/* UPDATE stock
	 * SET s_quantity = :s_quantity, s_order_cnt = :s_order_cnt
	 * WHERE s_w_id = :w_id AND s_i_id = :ol_i_id;
	 */

        TRACE( TRACE_TRX_FLOW, "App: %d NO:stock-update-tuple (%d) (%d) (%d)\n", 
               xct_id, astock.S_ORDER_CNT, astock.S_YTD, astock.S_REMOTE_CNT);
	W_DO(_stock.update_tuple(_pssm, &rst, &astock));


	/* INSERT INTO order_line
	 * VALUES (o_id, d_id, w_id, ol_ln, ol_i_id, supply_w_id,
	 *        '0001-01-01-00.00.01.000000', ol_quantity, iol_amount, dist)
	 */

	rol.set_value(0, adist.D_NEXT_O_ID);
	rol.set_value(1, pnoin->_d_id);
	rol.set_value(2, pnoin->_wh_id);
	rol.set_value(3, item_cnt+1);
	rol.set_value(4, ol_i_id);
	rol.set_value(5, ol_supply_w_id);
	rol.set_value(6, tstamp);
	rol.set_value(7, pnoin->items[item_cnt]._ol_quantity);
	rol.set_value(8, item_amount);
	rol.set_value(9, astock.S_DIST[6+pnoin->_d_id]);

        TRACE( TRACE_TRX_FLOW, "App: %d NO:add-tuple (%d)\n", 
               xct_id, adist.D_NEXT_O_ID);
	W_DO(_order_line.add_tuple(_pssm, &rol));

    } /* end for loop */


    /* 5. insert row to orders and new_order */

    /* INSERT INTO orders
     * VALUES (o_id, o_d_id, o_w_id, o_c_id, o_entry_d, o_ol_cnt, o_all_local)
     */
   
    rord.set_value(0, adist.D_NEXT_O_ID);
    rord.set_value(1, pnoin->_c_id);
    rord.set_value(2, pnoin->_d_id);
    rord.set_value(3, pnoin->_wh_id);
    rord.set_value(4, tstamp);
    rord.set_value(5, 0);
    rord.set_value(6, pnoin->_ol_cnt);
    rord.set_value(7, all_local);

    TRACE( TRACE_TRX_FLOW, "App: %d NO:add-tuple (%d)\n", 
           xct_id, adist.D_NEXT_O_ID);
    W_DO(_order.add_tuple(_pssm, &rord));


    /* INSERT INTO new_order VALUES (o_id, d_id, w_id)
     */

    rno.set_value(0, adist.D_NEXT_O_ID);
    rno.set_value(1, pnoin->_d_id);
    rno.set_value(2, pnoin->_wh_id);

    TRACE( TRACE_TRX_FLOW, "App: %d NO:add-tuple (%d) (%d) (%d)\n", 
           xct_id, adist.D_NEXT_O_ID, pnoin->_d_id, pnoin->_wh_id);
    W_DO(_new_order.add_tuple(_pssm, &rno));


#ifdef PRINT_TRX_RESULTS
    // at the end of the transaction 
    // dumps the status of all the table rows used
    rwh.print_tuple();
    rdist.print_tuple();
    rcust.print_tuple();
    rno.print_tuple();
    rord.print_tuple();
    ritem.print_tuple();
    rst.print_tuple();
    rol.print_tuple();
#endif


    /* 6. finalize trx */
    W_DO(_pssm->commit_xct());

    // if we reached this point everything went ok
    trt.set_state(COMMITTED);

    return (RCOK);

} // EOF: NEW_ORDER




/******************************************************************** 
 *
 * TPC-C PAYMENT
 *
 ********************************************************************/

w_rc_t ShoreTPCCEnv::xct_payment(payment_input_t* ppin, 
                                 const int xct_id, 
                                 trx_result_tuple_t& trt)
{
    // ensure a valid environment    
    assert (ppin);
    assert (_pssm);
    assert (_initialized);
    assert (_loaded);


    // payment trx touches 4 tables: 
    // warehouse, district, customer, and history
    table_row_t rwh(&_warehouse);
    table_row_t rdist(&_district);
    table_row_t rcust(&_customer);
    table_row_t rhist(&_history);
    trt.reset(UNSUBMITTED, xct_id);

    /* 0. initiate transaction */
    W_DO(_pssm->begin_xct());


    /* 1. retrieve warehouse for update */
    TRACE( TRACE_TRX_FLOW, 
           "App: %d PAY:warehouse-index-probe (%d)\n", 
           xct_id, ppin->_home_wh_id);

    W_DO(_warehouse.index_probe_forupdate(_pssm, &rwh, ppin->_home_wh_id));   


    /* 2. retrieve district for update */
    TRACE( TRACE_TRX_FLOW, 
           "App: %d PAY:district-index-probe (%d) (%d)\n", 
           xct_id, ppin->_home_wh_id, ppin->_home_d_id);

    W_DO(_district.index_probe_forupdate(_pssm, &rdist,
                                         ppin->_home_d_id, 
                                         ppin->_home_wh_id));

    // find the customer wh and d
    int c_w = ( ppin->_v_cust_wh_selection > 85 ? ppin->_home_wh_id : ppin->_remote_wh_id);
    int c_d = ( ppin->_v_cust_wh_selection > 85 ? ppin->_home_d_id : ppin->_remote_d_id);


    /* 3. retrieve customer for update */
    if (ppin->_c_id == 0) {

        /* 3a. if no customer selected already use the index on the customer name */

	/* SELECT  c_id, c_first
	 * FROM customer
	 * WHERE c_last = :c_last AND c_w_id = :c_w_id AND c_d_id = :c_d_id
	 * ORDER BY c_first
	 *
	 * plan: index only scan on "C_NAME_INDEX"
	 */

        assert (ppin->_v_cust_ident_selection <= 60);

        index_scan_iter_impl* c_iter;
        TRACE( TRACE_TRX_FLOW, "App: %d PAY:cust-get-iter-by-name-index (%s)\n", 
               xct_id, ppin->_c_last);
	W_DO(_customer.get_iter_by_index(_pssm, c_iter, &rcust, 
                                         c_w, c_d, ppin->_c_last));

	int c_id_list[17];
	int count = 0;
	bool eof;

	W_DO(c_iter->next(_pssm, eof, rcust));
	while (!eof) {
	    rcust.get_value(0, c_id_list[count++]);            
            TRACE( TRACE_TRX_FLOW, "App: %d PAY:cust-iter-next (%d)\n", xct_id, c_id_list[count]);
	    W_DO(c_iter->next(_pssm, eof, rcust));
	}
	delete c_iter;
        assert (count);

	/* find the customer id in the middle of the list */
	ppin->_c_id = c_id_list[(count+1)/2-1];
    }
    assert (ppin->_c_id>0);


    /* SELECT c_first, c_middle, c_last, c_street_1, c_street_2, c_city, 
     * c_state, c_zip, c_phone, c_since, c_credit, c_credit_lim, 
     * c_discount, c_balance, c_ytd_payment, c_payment_cnt 
     * FROM customer 
     * WHERE c_id = :c_id AND c_w_id = :c_w_id AND c_d_id = :c_d_id 
     * FOR UPDATE OF c_balance, c_ytd_payment, c_payment_cnt
     *
     * plan: index probe on "C_INDEX"
     */

    TRACE( TRACE_TRX_FLOW, 
           "App: %d PAY:cust-index-probe-forupdate (%d) (%d) (%d)\n", 
           xct_id, c_w, c_d, ppin->_c_id);

    W_DO(_customer.index_probe_forupdate(_pssm, &rcust, ppin->_c_id, c_w, c_d));

    double c_balance, c_ytd_payment;
    int    c_payment_cnt;
    tpcc_customer_tuple acust;

    // retrieve customer
    rcust.get_value(3,  acust.C_FIRST, 17);
    rcust.get_value(4,  acust.C_MIDDLE, 3);
    rcust.get_value(5,  acust.C_LAST, 17);
    rcust.get_value(6,  acust.C_STREET_1, 21);
    rcust.get_value(7,  acust.C_STREET_2, 21);
    rcust.get_value(8,  acust.C_CITY, 21);
    rcust.get_value(9,  acust.C_STATE, 3);
    rcust.get_value(10, acust.C_ZIP, 10);
    rcust.get_value(11, acust.C_PHONE, 17);
    rcust.get_value(12, acust.C_SINCE);
    rcust.get_value(13, acust.C_CREDIT, 3);
    rcust.get_value(14, acust.C_CREDIT_LIM);
    rcust.get_value(15, acust.C_DISCOUNT);
    rcust.get_value(16, acust.C_BALANCE);
    rcust.get_value(17, acust.C_YTD_PAYMENT);
    rcust.get_value(18, acust.C_LAST_PAYMENT);
    rcust.get_value(19, acust.C_PAYMENT_CNT);
    rcust.get_value(20, acust.C_DATA_1, 251);
    rcust.get_value(21, acust.C_DATA_2, 251);

    // update customer fields
    acust.C_BALANCE -= ppin->_h_amount;
    acust.C_YTD_PAYMENT += ppin->_h_amount;
    acust.C_PAYMENT_CNT++;

    // if bad customer
    if (acust.C_CREDIT[0] == 'B' && acust.C_CREDIT[1] == 'C') { 
        /* 10% of customers */

	/* SELECT c_data
	 * FROM customer 
	 * WHERE c_id = :c_id AND c_w_id = :c_w_id AND c_d_id = :c_d_id
	 * FOR UPDATE OF c_balance, c_ytd_payment, c_payment_cnt, c_data
	 *
	 * plan: index probe on "C_INDEX"
	 */

        TRACE( TRACE_TRX_FLOW, 
               "App: %d PAY:cust-index-probe-forupdate (%d) (%d) (%d)\n", 
               xct_id, ppin->_c_id, c_w, c_d);        
	W_DO(_customer.index_probe_forupdate(_pssm, &rcust, ppin->_c_id, c_w, c_d));

        // update the data
	char c_new_data_1[251];
        char c_new_data_2[251];
	sprintf(c_new_data_1, "%d,%d,%d,%d,%d,%1.2f",
		ppin->_c_id, c_d, c_w, ppin->_home_d_id, 
                ppin->_home_wh_id, ppin->_h_amount);

        int len = strlen(c_new_data_1);
	strncat(c_new_data_1, acust.C_DATA_1, 250-len);
        strncpy(c_new_data_2, &acust.C_DATA_1[250-len], len);
        strncpy(c_new_data_2, acust.C_DATA_2, 250-len);

        TRACE( TRACE_TRX_FLOW, "App: %d PAY:cust-update-tuple\n", xct_id);
	W_DO(_customer.update_tuple(_pssm, &rcust, acust, c_new_data_1, c_new_data_2));
    }
    else { /* good customer */
        TRACE( TRACE_TRX_FLOW, "App: %d PAY:cust-update-tuple\n", xct_id);
	W_DO(_customer.update_tuple(_pssm, &rcust, acust, NULL, NULL));
    }

    /* UPDATE district SET d_ytd = d_ytd + :h_amount
     * WHERE d_id = :d_id AND d_w_id = :w_id
     *
     * plan: index probe on "D_INDEX"
     */

    TRACE( TRACE_TRX_FLOW, "App: %d PAY:distr-update-ytd1 (%d) (%d)\n", 
           xct_id, ppin->_home_wh_id, ppin->_home_d_id);
    W_DO(_district.update_ytd(_pssm, &rdist, ppin->_home_d_id, ppin->_home_wh_id, ppin->_h_amount));

    /* SELECT d_street_1, d_street_2, d_city, d_state, d_zip, d_name
     * FROM district
     * WHERE d_id = :d_id AND d_w_id = :w_id
     *
     * plan: index probe on "D_INDEX"
     */

    TRACE( TRACE_TRX_FLOW, "App: %d PAY:distr-index-probe (%d) (%d) (%.2f)\n", 
           xct_id, ppin->_home_wh_id, ppin->_home_d_id, ppin->_h_amount);
    W_DO(_district.index_probe(_pssm, &rdist, ppin->_home_d_id, ppin->_home_wh_id));

    tpcc_district_tuple adistr;
    rdist.get_value(2, adistr.D_NAME, 11);
    rdist.get_value(3, adistr.D_STREET_1, 21);
    rdist.get_value(4, adistr.D_STREET_2, 21);
    rdist.get_value(5, adistr.D_CITY, 21);
    rdist.get_value(6, adistr.D_STATE, 3);
    rdist.get_value(7, adistr.D_ZIP, 10);


    /* UPDATE warehouse SET w_ytd = wytd + :h_amount
     * WHERE w_id = :w_id
     *
     * plan: index probe on "W_INDEX"
     */

    TRACE( TRACE_TRX_FLOW, "App: %d PAY:wh-update-ytd2 (%d) (%.2f)\n", 
           xct_id, ppin->_home_wh_id, ppin->_h_amount);
    W_DO(_warehouse.update_ytd(_pssm, &rwh, ppin->_home_wh_id, ppin->_h_amount));

    tpcc_warehouse_tuple awh;
    rwh.get_value(1, awh.W_NAME, 11);
    rwh.get_value(2, awh.W_STREET_1, 21);
    rwh.get_value(3, awh.W_STREET_2, 21);
    rwh.get_value(4, awh.W_CITY, 21);
    rwh.get_value(5, awh.W_STATE, 3);
    rwh.get_value(6, awh.W_ZIP, 10);
    

    /* INSERT INTO history
     * VALUES (:c_id, :c_d_id, :c_w_id, :d_id, :w_id, :curr_tmstmp, :ih_amount, :h_data)
     */

    tpcc_history_tuple ahist;
    sprintf(ahist.H_DATA, "%s   %s", awh.W_NAME, adistr.D_NAME);
    ahist.H_DATE = time(NULL);
    rhist.set_value(0, ppin->_c_id);
    rhist.set_value(1, c_d);
    rhist.set_value(2, c_w);
    rhist.set_value(3, ppin->_home_d_id);
    rhist.set_value(4, ppin->_home_wh_id);
    rhist.set_value(5, ahist.H_DATE);
    rhist.set_value(6, ppin->_h_amount * 100.0);
    rhist.set_value(7, ahist.H_DATA);

    TRACE( TRACE_TRX_FLOW, "App: %d PAY:hist-add-tuple\n", xct_id);
    W_DO(_history.add_tuple(_pssm, &rhist));


#ifdef PRINT_TRX_RESULTS
    // at the end of the transaction 
    // dumps the status of all the table rows used
    rwh.print_tuple();
    rdist.print_tuple();
    rcust.print_tuple();
    rhist.print_tuple();
#endif


    /* 4. commit */
    W_DO(_pssm->commit_xct());

    // if we reached this point everything went ok
    trt.set_state(COMMITTED);

    return (RCOK);

} // EOF: PAYMENT



/******************************************************************** 
 *
 * TPC-C ORDER STATUS
 *
 * Input: w_id, d_id, c_id (use c_last if set to null), c_last
 *
 * @note: Read-Only trx
 *
 ********************************************************************/


w_rc_t ShoreTPCCEnv::xct_order_status(order_status_input_t* pstin, 
                                      const int xct_id, 
                                      trx_result_tuple_t& trt)
{
    // ensure a valid environment    
    assert (pstin);
    assert (_pssm);
    assert (_initialized);
    assert (_loaded);

    register int w_id = pstin->_wh_id;
    register int d_id = pstin->_d_id;

    // order_status trx touches 3 tables: 
    // customer, order and orderline
    table_row_t rcust(&_customer);
    table_row_t rord(&_order);
    table_row_t rordline(&_order_line);
    trt.reset(UNSUBMITTED, xct_id);

    /* 0. initiate transaction */
    W_DO(_pssm->begin_xct());

    /* 1a. select customer based on name */
    if (pstin->_c_id == 0) {
	/* SELECT  c_id, c_first
	 * FROM customer
	 * WHERE c_last = :c_last AND c_w_id = :w_id AND c_d_id = :d_id
	 * ORDER BY c_first
	 *
	 * plan: index only scan on "C_NAME_INDEX"
	 */

        assert (pstin->_c_select <= 60);
        assert (pstin->_c_last);

        index_scan_iter_impl* c_iter;
        TRACE( TRACE_TRX_FLOW, "App: %d ORDST:get-iter-by-index\n", xct_id);
	W_DO(_customer.get_iter_by_index(_pssm, c_iter, &rcust, 
                                         w_id, d_id, pstin->_c_last));

	int  c_id_list[17];
	int  count = 0;
	bool eof;

	W_DO(c_iter->next(_pssm, eof, rcust));
	while (!eof) {
	    rcust.get_value(0, c_id_list[count++]);            
            TRACE( TRACE_TRX_FLOW, "App: %d ORDST:iter-next\n", xct_id);
	    W_DO(c_iter->next(_pssm, eof, rcust));
	}
	delete c_iter;

	/* find the customer id in the middle of the list */
	pstin->_c_id = c_id_list[(count+1)/2-1];
    }
    assert (pstin->_c_id>0);


    /* 1. probe the customer */

    /* SELECT c_first, c_middle, c_last, c_balance
     * FROM customer
     * WHERE c_id = :c_id AND c_w_id = :w_id AND c_d_id = :d_id
     *
     * plan: index probe on "C_INDEX"
     */

    TRACE( TRACE_TRX_FLOW, 
           "App: %d ORDST:index-probe-forupdate (%d) (%d) (%d)\n", 
           xct_id, pstin->_c_id, w_id, d_id);
    W_DO(_customer.index_probe(_pssm, &rcust, pstin->_c_id, w_id, d_id));

    tpcc_customer_tuple acust;
    rcust.get_value(3,  acust.C_FIRST, 17);
    rcust.get_value(4,  acust.C_MIDDLE, 3);
    rcust.get_value(5,  acust.C_LAST, 17);
    rcust.get_value(16, acust.C_BALANCE);


    /* 2. retrieve the last order of this customer */

    /* SELECT o_id, o_entry_d, o_carrier_id
     * FROM orders
     * WHERE o_w_id = :w_id AND o_d_id = :d_id AND o_c_id = :o_c_id
     * ORDER BY o_id DESC
     *
     * plan: index scan on "C_CUST_INDEX"
     */

    index_scan_iter_impl* o_iter;
    TRACE( TRACE_TRX_FLOW, "App: %d ORDST:get-order-iter-by-index\n", xct_id);
    W_DO(_order.get_iter_by_index(_pssm, o_iter, &rord,
				  w_id, d_id, pstin->_c_id));

    tpcc_order_tuple aorder;
    bool eof;
    W_DO(o_iter->next(_pssm, eof, rord));
    while (!eof) {
	rord.get_value(0, aorder.O_ID);
	rord.get_value(4, aorder.O_ENTRY_D);
	rord.get_value(5, aorder.O_CARRIER_ID);
        rord.get_value(6, aorder.O_OL_CNT);

	W_DO(o_iter->next(_pssm, eof, rord));
    }
    delete o_iter;

    // we should have retrieved a valid id and ol_cnt for the order               
    assert (aorder.O_ID);
    assert (aorder.O_OL_CNT);

    /* 3. retrieve all the orderlines that correspond to the last order */
    
    /* SELECT ol_i_id, ol_supply_w_id, ol_quantity, ol_amount, ol_delivery_d 
     * FROM order_line 
     * WHERE ol_w_id = :H00003 AND ol_d_id = :H00004 AND ol_o_id = :H00016 
     *
     * plan: index scan on "OL_INDEX"
     */

    index_scan_iter_impl* ol_iter;
    TRACE( TRACE_TRX_FLOW, "App: %d ORDST:get-iter-by-index\n", xct_id);
    W_DO(_order_line.get_iter_by_index(_pssm, ol_iter, &rordline,
				       w_id, d_id, aorder.O_ID));

    tpcc_orderline_tuple* porderlines = new tpcc_orderline_tuple[aorder.O_OL_CNT];
    int i=0;

    W_DO(ol_iter->next(_pssm, eof, rordline));
    while (!eof) {
	rordline.get_value(4, porderlines[i].OL_I_ID);
	rordline.get_value(5, porderlines[i].OL_SUPPLY_W_ID);
	rordline.get_value(6, porderlines[i].OL_DELIVERY_D);
	rordline.get_value(7, porderlines[i].OL_QUANTITY);
	rordline.get_value(8, porderlines[i].OL_AMOUNT);
	i++;

	W_DO(ol_iter->next(_pssm, eof, rordline));
    }
    delete ol_iter;
    delete [] porderlines;


#ifdef PRINT_TRX_RESULTS
    // at the end of the transaction 
    // dumps the status of all the table rows used
    rcust.print_tuple();
    rord.print_tuple();
    rordline.print_tuple();
#endif


    /* 4. commit */
    W_DO(_pssm->commit_xct());

    // if we reached this point everything went ok
    trt.set_state(COMMITTED);

    return (RCOK);

}; // EOF: ORDER-STATUS



/******************************************************************** 
 *
 * TPC-C DELIVERY
 *
 * Input data: w_id, carrier_id
 *
 * @note: Delivers one new_order (undelivered order) from each district
 *
 ********************************************************************/


w_rc_t ShoreTPCCEnv::xct_delivery(delivery_input_t* pdin, 
                                  const int xct_id, 
                                  trx_result_tuple_t& trt)
{
    // ensure a valid environment    
    assert (pdin);
    assert (_pssm);
    assert (_initialized);
    assert (_loaded);

    register int w_id       = pdin->_wh_id;
    register int carrier_id = pdin->_carrier_id; 
    time_t ts_start = time(NULL);

    // delivery trx touches 4 tables: 
    // new_order, order, orderline, and customer
    table_row_t rno(&_new_order);
    table_row_t rord(&_order);
    table_row_t rordline(&_order_line);
    table_row_t rcust(&_customer);
    trt.reset(UNSUBMITTED, xct_id);

    /* 0. initiate transaction */
    W_DO(_pssm->begin_xct());


    /* process each district separately */
    for (int d_id = 1; d_id < DISTRICTS_PER_WAREHOUSE; d_id ++) {

        /* 1. Get the new_order of the district, with the min value */

	/* SELECT MIN(no_o_id) INTO :no_o_id:no_o_id_i
	 * FROM new_order
	 * WHERE no_d_id = :d_id AND no_w_id = :w_id
	 *
	 * plan: index scan on "NO_INDEX"
	 */

        // setup a sort buffer of SMALLINTS
	sort_buffer_t o_id_list(1);
	o_id_list.setup(0, SQL_INT);
        table_row_t rsb(&o_id_list);


        TRACE( TRACE_TRX_FLOW, "App: %d DEL:get-new-order-iter-by-index (%d) (%d)\n", 
               xct_id, w_id, d_id);

        index_scan_iter_impl* no_iter;
	W_DO(_new_order.get_iter_by_index(_pssm, no_iter, &rno, w_id, d_id));
	bool  eof;

        // iterate over all new_orders and load their no_o_ids to the sort buffer
	W_DO(no_iter->next(_pssm, eof, rno));
	while (!eof) {
	    int anoid;
	    rno.get_value(0, anoid);
            rsb.set_value(0, anoid);
	    o_id_list.add_tuple(rsb);

	    W_DO(no_iter->next(_pssm, eof, rno));
	}
	delete no_iter;
        assert (o_id_list.count());
        
	int no_o_id = 0;
        sort_iter_impl o_id_list_iter(_pssm, &o_id_list);

        //	W_DO(o_id_list.get_iter_sort_buffer(o_id_list_iter));

        // get the first entry (min value)
	W_DO(o_id_list_iter.next(_pssm, eof, rsb));
	if (!eof)
	    rsb.get_value(0, no_o_id);
	else continue;
        assert (no_o_id);


        /* 2. Delete the retrieved new order from the new_orders */        

	/* DELETE FROM new_order
	 * WHERE no_w_id = :w_id AND no_d_id = :d_id AND no_o_id = :no_o_id
	 *
	 * plan: index scan on "NO_INDEX"
	 */

        TRACE( TRACE_TRX_FLOW, "App: %d DEL:delete-new-order-by-index (%d) (%d) (%d)\n", 
               xct_id, w_id, d_id, no_o_id);

	W_DO(_new_order.delete_by_index(_pssm, &rno, w_id, d_id, no_o_id));


        /* 3a. Update the carrier for the delivered order (in the orders table) */
        /* 3b. Get the customer id of the updated order */

	/* UPDATE orders SET o_carrier_id = :o_carrier_id
         * SELECT o_c_id INTO :o_c_id FROM orders
	 * WHERE o_id = :no_o_id AND o_w_id = :w_id AND o_d_id = :d_id;
	 *
	 * plan: index probe on "O_INDEX"
	 */

        TRACE( TRACE_TRX_FLOW, "App: %d DEL:index-probe (%d) (%d) (%d)\n", 
               xct_id, w_id, d_id, no_o_id);

	rord.set_value(0, no_o_id);
	rord.set_value(2, d_id);
	rord.set_value(3, w_id);
	W_DO(_order.update_carrier_by_index(_pssm, &rord, carrier_id));

	int  c_id;
	rord.get_value(1, c_id);

        
        /* 4a. Calculate the total amount of the orders from orderlines */
        /* 4b. Update all the orderlines with the current timestamp */
           
	/* SELECT SUM(ol_amount) INTO :total_amount FROM order_line
         * UPDATE ORDER_LINE SET ol_delivery_d = :curr_tmstmp
	 * WHERE ol_w_id = :w_id AND ol_d_id = :no_d_id AND ol_o_id = :no_o_id;
	 *
	 * plan: index scan on "OL_INDEX"
	 */


        TRACE( TRACE_TRX_FLOW, 
               "App: %d DEL:get-orderline-iter-by-index (%d) (%d) (%d)\n", 
               xct_id, w_id, d_id, no_o_id);

	int total_amount = 0;
        index_scan_iter_impl* ol_iter;
	W_DO(_order_line.get_iter_by_index(_pssm, ol_iter, &rordline, 
                                           w_id, d_id, no_o_id));

        // iterate over all the orderlines for the particular order
	W_DO(ol_iter->next(_pssm, eof, rordline));
	while (!eof) {
	    int current_amount;
	    rordline.get_value(8, current_amount);
	    total_amount += current_amount;
            rordline.set_value(6, ts_start);
            W_DO(_order_line.update_tuple(_pssm, &rordline));
	    W_DO(ol_iter->next(_pssm, eof, rordline));
	}
	delete ol_iter;


        /* 5. Update balance of the customer of the order */

	/* UPDATE customer
	 * SET c_balance = c_balance + :total_amount, c_delivery_cnt = c_delivery_cnt + 1
	 * WHERE c_id = :c_id AND c_w_id = :w_id AND c_d_id = :no_d_id;
	 *
	 * plan: index probe on "C_INDEX"
	 */

        TRACE( TRACE_TRX_FLOW, "App: %d DEL:index-probe (%d) (%d) (%d)\n", 
               xct_id, w_id, d_id, c_id);

	W_DO(_customer.index_probe(_pssm, &rcust, c_id, w_id, d_id));

	double   balance;
	rcust.get_value(16, balance);
	rcust.set_value(16, balance+total_amount);
    }

#ifdef PRINT_TRX_RESULTS
    // at the end of the transaction 
    // dumps the status of all the table rows used
    rno.print_tuple();
    rord.print_tuple();
    rordline.print_tuple();
    rcust.print_tuple();
#endif


    /* 4. commit */
    W_DO(_pssm->commit_xct());

    // if we reached this point everything went ok
    trt.set_state(COMMITTED);

    return (RCOK);
}




/******************************************************************** 
 *
 * TPC-C STOCK LEVEL
 *
 * Input data: w_id, d_id, threshold
 *
 * @note: Read-only transaction
 *
 ********************************************************************/

w_rc_t ShoreTPCCEnv::xct_stock_level(stock_level_input_t* pslin, 
                                     const int xct_id, 
                                     trx_result_tuple_t& trt)
{
    // ensure a valid environment    
    assert (pslin);
    assert (_pssm);
    assert (_initialized);
    assert (_loaded);

    // stock level trx touches 4 tables: 
    // district, orderline, and stock
    table_row_t rdist(&_district);
    table_row_t rordline(&_order_line);
    table_row_t rstock(&_stock);
    trt.reset(UNSUBMITTED, xct_id);


    /* 0. initiate transaction */
    W_DO(_pssm->begin_xct());
    
    /* 1. get next_o_id from the district */

    /* SELECT d_next_o_id INTO :o_id
     * FROM district
     * WHERE d_w_id = :w_id AND d_id = :d_id
     *
     * (index scan on D_INDEX)
     */

    TRACE( TRACE_TRX_FLOW, "App: %d STO:index-probe (%d) (%d)\n", 
           xct_id, pslin->_d_id, pslin->_wh_id);

    W_DO(_district.index_probe(_pssm, &rdist, pslin->_d_id, pslin->_wh_id));

    int next_o_id = 0;
    rdist.get_value(10, next_o_id);


    /*
     *   SELECT COUNT(DISTRICT(s_i_id)) INTO :stock_count
     *   FROM order_line, stock
     *   WHERE ol_w_id = :w_id AND ol_d_id = :d_id
     *       AND ol_o_id < :o_id AND ol_o_id >= :o_id-20
     *       AND s_w_id = :w_id AND s_i_id = ol_i_id
     *       AND s_quantity < :threshold;
     *
     *  Plan: 1. index scan on OL_INDEX 
     *        2. sort ol tuples in the order of i_id from 1
     *        3. index scan on S_INDEX
     *        4. fetch stock with sargable on quantity from 3
     *        5. nljoin on 2 and 4
     *        6. unique on 5
     *        7. group by on 6
     */

    /* 2a. Index scan on order_line table. */

    TRACE( TRACE_TRX_FLOW, "App: %d STO:get-iter-by-index (%d) (%d) (%d) (%d)\n", 
           xct_id, pslin->_wh_id, pslin->_d_id, next_o_id-20, next_o_id);

    index_scan_iter_impl* ol_iter;
    W_DO(_order_line.get_iter_by_index(_pssm, ol_iter, &rordline,
				       pslin->_wh_id, pslin->_d_id,
				       next_o_id-20, next_o_id));


    sort_buffer_t ol_list(4);
    ol_list.setup(0, SQL_INT);  /* OL_I_ID */
    ol_list.setup(1, SQL_INT);  /* OL_W_ID */
    ol_list.setup(2, SQL_INT);  /* OL_D_ID */
    ol_list.setup(3, SQL_INT);  /* OL_O_ID */
    table_row_t rsb(&ol_list);

    // iterate over all selected orderlines and add them to the sorted buffer
    bool eof;
    W_DO(ol_iter->next(_pssm, eof, rordline));
    while (!eof) {
	/* put the value into the sorted buffer */
	int temp_oid, temp_iid;
	int temp_wid, temp_did;        

	rordline.get_value(4, temp_iid);
	rordline.get_value(0, temp_oid);
	rordline.get_value(2, temp_wid);
	rordline.get_value(1, temp_did);

	rsb.set_value(0, temp_iid);
	rsb.set_value(1, temp_wid);
	rsb.set_value(2, temp_did);
	rsb.set_value(3, temp_oid);

	ol_list.add_tuple(rsb);
  
	W_DO(ol_iter->next(_pssm, eof, rordline));
    }
    delete ol_iter;
    assert (ol_list.count());

    /* 2b. Sort orderline tuples on i_id */
    sort_iter_impl ol_list_sort_iter(_pssm, &ol_list);
    int last_i_id = -1;
    int count = 0;

    /* 2c. Nested loop join order_line with stock */
    W_DO(ol_list_sort_iter.next(_pssm, eof, rsb));
    while (!eof) {

	/* use the index to find the corresponding stock tuple */
	int i_id;
	int w_id;

	rsb.get_value(0, i_id);
	rsb.get_value(1, w_id);

        TRACE( TRACE_TRX_FLOW, "App: %d STO:index-probe (%d) (%d)\n", 
               xct_id, i_id, w_id);

	W_DO(_stock.index_probe(_pssm, &rstock, i_id, w_id));

        // check if stock quantity below threshold 
	int quantity;
	rstock.get_value(3, quantity);

	if (quantity < pslin->_threshold) {
	    /* Do join on the two tuples */

	    /* the work is to count the number of unique item id. We keep
	     * two pieces of information here: the last item id and the
	     * current count.  This is enough because the item id is in
	     * increasing order.
	     */
	    if (last_i_id != i_id) {
		last_i_id = i_id;
		count++;
	    }
	}

	W_DO(ol_list_sort_iter.next(_pssm, eof, rsb));
    }

#ifdef PRINT_TRX_RESULTS
    // at the end of the transaction 
    // dumps the status of all the table rows used
    rdist.print_tuple();
    rordline.print_tuple();
    rstock.print_tuple();
#endif

  
    /* 3. commit */
    W_DO(_pssm->commit_xct());

    // if we reached this point everything went ok
    trt.set_state(COMMITTED);

    return (RCOK);
}





EXIT_NAMESPACE(tpcc);