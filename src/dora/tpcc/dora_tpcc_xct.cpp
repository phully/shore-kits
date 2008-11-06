/* -*- mode:C++; c-basic-offset:4 -*- */

/** @file:   dora_tpcc_xct.cpp
 *
 *  @brief:  Declaration of the Shore DORA transactions as part of ShoreTPCCEnv
 *
 *  @author: Ippokratis Pandis, Sept 2008
 */

#include "stages/tpcc/shore/shore_tpcc_env.h"

#include "dora/tpcc/dora_payment.h"
#include "dora/tpcc/dora_mbench.h"
#include "dora/tpcc/dora_tpcc.h"


using namespace shore;
using namespace dora;
using namespace tpcc;


ENTER_NAMESPACE(tpcc);


/******** Exported functions  ********/


/********
 ******** Caution: The functions below should be invoked inside
 ********          the context of a smthread
 ********/


/******************************************************************** 
 *
 * TPC-C DORA TRXS
 *
 * (1) The dora_XXX functions are wrappers to the real transactions
 * (2) The xct_dora_XXX functions are the implementation of the transactions
 *
 ********************************************************************/


/******************************************************************** 
 *
 * TPC-C DORA TRXs Wrappers
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

w_rc_t ShoreTPCCEnv::dora_new_order(const int xct_id, 
                                    new_order_input_t& anoin,
                                    trx_result_tuple_t& atrt)
{
    TRACE( TRACE_TRX_FLOW, "%d. DORA NEW-ORDER...\n", xct_id);     

    // 1. enqueue all actions
    assert (0);

    return (RCOK); 
}


w_rc_t ShoreTPCCEnv::dora_payment(const int xct_id, 
                                  payment_input_t& apin,
                                  trx_result_tuple_t& atrt)
{
    assert (_g_dora);

    // 1. Initiate transaction
    tid_t atid;   
    W_DO(_pssm->begin_xct(atid));
    xct_t* pxct = smthread_t::me()->xct();
    assert (pxct);
    TRACE( TRACE_TRX_FLOW, "Begin (%d)\n", atid);

    // 2. Setup the next RVP
    // PH1 consists of 3 packets
    midway_pay_rvp* rvp = new midway_pay_rvp(atid, pxct, xct_id,
                                             atrt, this, apin); 
    assert (rvp);
    
    // 3. Generate the actions    
    upd_cust_pay_action_impl* pay_upd_cust = _g_dora->get_upd_cust_pay_action();
    assert (pay_upd_cust);
    pay_upd_cust->set_input(atid, pxct, rvp, this, apin);
    pay_upd_cust->_m_rvp=rvp;
    rvp->add_action(pay_upd_cust);

    upd_dist_pay_action_impl* pay_upd_dist = _g_dora->get_upd_dist_pay_action();
    assert (pay_upd_dist);
    pay_upd_dist->set_input(atid, pxct, rvp, this, apin);
    pay_upd_dist->_m_rvp=rvp;
    rvp->add_action(pay_upd_dist);

    upd_wh_pay_action_impl* pay_upd_wh = _g_dora->get_upd_wh_pay_action();
    assert (pay_upd_wh);
    pay_upd_wh->set_input(atid, pxct, rvp, this, apin);
    pay_upd_wh->_m_rvp=rvp;
    rvp->add_action(pay_upd_wh);


    // 4. Detatch self from xct
    me()->detach_xct(pxct);
    TRACE( TRACE_TRX_FLOW, "Deattached from (%d)\n", atid);


    // For each action
    // 5a. Decide about partition
    // 5b. Enqueue
    //
    // All the enqueues should appear atomic
    // That is, there should be a total order across trxs 
    // (it terms of the sequence actions are enqueued)

    {
        int mypartition = apin._home_wh_id-1;

        // WH_PART_CS
        CRITICAL_SECTION(wh_part_cs, _g_dora->whs(mypartition)->_enqueue_lock);
            
        // (SF) WAREHOUSE partitions
        if (_g_dora->whs()->enqueue(pay_upd_wh, mypartition)) {
            TRACE( TRACE_DEBUG, "Problem in enqueueing PAY_UPD_WH\n");
            assert (0); 
            return (RC(de_PROBLEM_ENQUEUE));
        }

        // DIS_PART_CS
        CRITICAL_SECTION(dis_part_cs, _g_dora->dis(mypartition)->_enqueue_lock);
        wh_part_cs.exit();

        // (SF) WAREHOUSE partitions
        if (_g_dora->dis()->enqueue(pay_upd_dist, mypartition)) {
            TRACE( TRACE_DEBUG, "Problem in enqueueing PAY_UPD_DIST\n");
            assert (0); 
            return (RC(de_PROBLEM_ENQUEUE));
        }

        // CUS_PART_CS
        CRITICAL_SECTION(cus_part_cs, _g_dora->cus(mypartition)->_enqueue_lock);
        dis_part_cs.exit();

        // (SF) WAREHOUSE partitions
        if (_g_dora->cus()->enqueue(pay_upd_cust, mypartition)) {
            TRACE( TRACE_DEBUG, "Problem in enqueueing PAY_UPD_CUST\n");
            assert (0); 
            return (RC(de_PROBLEM_ENQUEUE));
        }
    }

    return (RCOK); 
}


w_rc_t ShoreTPCCEnv::dora_order_status(const int xct_id, 
                                       order_status_input_t& aordstin,
                                       trx_result_tuple_t& atrt)
{
    TRACE( TRACE_TRX_FLOW, "%d. ORDER-STATUS...\n", xct_id);

    // 1. enqueue all actions
    assert (0);

    return (RCOK); 
}


w_rc_t ShoreTPCCEnv::dora_delivery(const int xct_id, 
                                   delivery_input_t& adelin,
                                   trx_result_tuple_t& atrt)
{
    TRACE( TRACE_TRX_FLOW, "%d. DELIVERY...\n", xct_id);     

    // 1. enqueue all actions
    assert (0);

    return (RCOK); 
}

w_rc_t ShoreTPCCEnv::dora_stock_level(const int xct_id, 
                                      stock_level_input_t& astoin,
                                      trx_result_tuple_t& atrt)
{
    TRACE( TRACE_TRX_FLOW, "%d. STOCK-LEVEL...\n", xct_id);     

    // 1. enqueue all actions
    assert (0);

    return (RCOK); 
}



/* --- without input specified --- */

w_rc_t ShoreTPCCEnv::dora_new_order(const int xct_id, 
                                    trx_result_tuple_t& atrt,
                                    int specificWH)
{
    new_order_input_t noin = create_no_input(_queried_factor, 
                                             specificWH);
    return (dora_new_order(xct_id, noin, atrt));
}


w_rc_t ShoreTPCCEnv::dora_payment(const int xct_id, 
                                  trx_result_tuple_t& atrt,
                                  int specificWH)
{
    payment_input_t pin = create_payment_input(_queried_factor, 
                                               specificWH);
    return (dora_payment(xct_id, pin, atrt));
}


w_rc_t ShoreTPCCEnv::dora_order_status(const int xct_id, 
                                       trx_result_tuple_t& atrt,
                                       int specificWH)
{
    order_status_input_t ordin = create_order_status_input(_queried_factor, 
                                                           specificWH);
    return (dora_order_status(xct_id, ordin, atrt));
}


w_rc_t ShoreTPCCEnv::dora_delivery(const int xct_id, 
                                   trx_result_tuple_t& atrt,
                                   int specificWH)
{
    delivery_input_t delin = create_delivery_input(_queried_factor, 
                                                   specificWH);
    return (dora_delivery(xct_id, delin, atrt));
}


w_rc_t ShoreTPCCEnv::dora_stock_level(const int xct_id, 
                                      trx_result_tuple_t& atrt,
                                      int specificWH)
{
    stock_level_input_t slin = create_stock_level_input(_queried_factor, 
                                                        specificWH);
    return (dora_stock_level(xct_id, slin, atrt));
}



/******************************************************************** 
 *
 * DORA MBENCHES
 *
 ********************************************************************/

w_rc_t ShoreTPCCEnv::dora_mbench_cust(const int xct_id, 
                                      trx_result_tuple_t& atrt, 
                                      const int whid)
{
    assert (_g_dora);

    // 1. Initiate transaction
    tid_t atid;   
    W_DO(_pssm->begin_xct(atid));
    xct_t* pxct = smthread_t::me()->xct();
    assert (pxct);
    TRACE( TRACE_TRX_FLOW, "Begin (%d)\n", atid);


    // 2. Setup the final RVP
    final_mb_rvp* frvp = new final_mb_rvp(atid, pxct, xct_id, atrt, 1, this);
    assert (frvp);

    
    // 3. Generate the actions
    upd_cust_mb_action_impl* upd_cust = _g_dora->get_upd_cust_mb_action();
    assert (upd_cust);
    upd_cust->set_input(atid, pxct, frvp, this, whid);
    frvp->add_action(upd_cust);


    // 4. Detatch self from xct
    me()->detach_xct(pxct);
    TRACE( TRACE_TRX_FLOW, "Deattached from (%d)\n", atid);


    // For each action
    // 5a. Decide about partition
    // 5b. Enqueue
    //
    // All the enqueues should appear atomic
    // That is, there should be a total order across trxs 
    // (it terms of the sequence actions are enqueued)

    {
        int mypartition = whid-1;
        // CUS_PART_CS
        CRITICAL_SECTION(cus_part_cs, _g_dora->cus(mypartition)->_enqueue_lock);
        // (SF) CUSTOMER partitions
        if (_g_dora->cus()->enqueue(upd_cust, mypartition)) { 
            TRACE( TRACE_DEBUG, "Problem in enqueueing UPD_CUST\n");
            assert (0); 
            return (RC(de_PROBLEM_ENQUEUE));
        }
    }

    return (RCOK); 
}

w_rc_t ShoreTPCCEnv::dora_mbench_wh(const int xct_id, 
                                    trx_result_tuple_t& atrt, 
                                    const int whid)
{
    assert (_g_dora);

    // 1. Initiate transaction
    tid_t atid;   
    W_DO(_pssm->begin_xct(atid));
    xct_t* pxct = smthread_t::me()->xct();
    assert (pxct);
    TRACE( TRACE_TRX_FLOW, "Begin (%d)\n", atid);


    // 2. Setup the final RVP
    final_mb_rvp* frvp = new final_mb_rvp(atid, pxct, xct_id, atrt, 1, this);
    assert (frvp);
    

    // 3. Generate the actions
    upd_wh_mb_action_impl* upd_wh = _g_dora->get_upd_wh_mb_action();
    assert (upd_wh);
    upd_wh->set_input(atid, pxct, frvp, this, whid);
    frvp->add_action(upd_wh);


    // 4. Detatch self from xct
    smthread_t::me()->detach_xct(pxct);
    TRACE( TRACE_TRX_FLOW, "Deattached from (%d)\n", atid);

    // For each action
    // 5a. Decide about partition
    // 5b. Enqueue
    //
    // All the enqueues should appear atomic
    // That is, there should be a total order across trxs 
    // (it terms of the sequence actions are enqueued)

    {
        int mypartition = whid-1;
        // WH_PART_CS
        CRITICAL_SECTION(wh_part_cs, _g_dora->whs(mypartition)->_enqueue_lock);
        // (SF) WAREHOUSE partitions
        if (_g_dora->whs()->enqueue(upd_wh, mypartition)) {
            TRACE( TRACE_DEBUG, "Problem in enqueueing UPD_WH_MB\n");
            assert (0); 
            return (RC(de_PROBLEM_ENQUEUE));
        }
    }

    return (RCOK); 
}


EXIT_NAMESPACE(tpcc);
