/* -*- mode:C++; c-basic-offset:4 -*-
     Shore-kits -- Benchmark implementations for Shore-MT
   
                       Copyright (c) 2007-2009
      Data Intensive Applications and Systems Labaratory (DIAS)
               Ecole Polytechnique Federale de Lausanne
   
                         All Rights Reserved.
   
   Permission to use, copy, modify and distribute this software and
   its documentation is hereby granted, provided that both the
   copyright notice and this permission notice appear in all copies of
   the software, derivative works or modified versions, and any
   portions thereof, and that both notices appear in supporting
   documentation.
   
   This code is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. THE AUTHORS
   DISCLAIM ANY LIABILITY OF ANY KIND FOR ANY DAMAGES WHATSOEVER
   RESULTING FROM THE USE OF THIS SOFTWARE.
*/

/** @file:   qpipe_3_2.cpp
 *
 *  @brief:  Implementation of QPIPE SSB Q3_2 over Shore-MT
 *
 *  @author: Manos Athanassoulis
 *  @date:   July 2010
 */

#include "workload/ssb/shore_ssb_env.h"
#include "qpipe.h"

using namespace shore;
using namespace qpipe;



ENTER_NAMESPACE(ssb);


/******************************************************************** 
 *
 * QPIPE Q3_2 - Structures needed by operators 
 *
 ********************************************************************/

// the tuples after tablescan projection
struct lo_tuple
{
  int LO_CUSTKEY;
  int LO_SUPPKEY;
  int LO_ORDERDATE;
  double LO_REVENUE;    
};

struct c_tuple
{
  int C_CUSTKEY;
  char C_CITY[11];
};

struct s_tuple
{
  int S_SUPPKEY;
  char S_CITY[11];
};

struct d_tuple
{ 
  int D_DATEKEY;
  int D_YEAR;
};

struct join_tuple
{
  char C_CITY[11];
  char S_CITY[11];
  int D_YEAR;
  double LO_REVENUE;
};

struct projected_tuple
{
  int KEY;
};



class lineorder_tscan_filter_t : public tuple_filter_t 
{
private:
    ShoreSSBEnv* _ssbdb;
    row_impl<lineorder_t>* _prline;
    rep_row_t _rr;

    ssb_lineorder_tuple _lineorder;

public:

    lineorder_tscan_filter_t(ShoreSSBEnv* ssbdb)//,q3_2_input_t &in) 
        : tuple_filter_t(ssbdb->lineorder_desc()->maxsize()), _ssbdb(ssbdb)
    {

    	// Get a lineorder tupple from the tuple cache and allocate space
        _prline = _ssbdb->lineorder_man()->get_tuple();
        _rr.set_ts(_ssbdb->lineorder_man()->ts(),
                   _ssbdb->lineorder_desc()->maxsize());
        _prline->_rep = &_rr;

    }

    ~lineorder_tscan_filter_t()
    {
        // Give back the lineorder tuple 
        _ssbdb->lineorder_man()->give_tuple(_prline);
    }


    // Predication
    bool select(const tuple_t &input) {

        // Get next lineorder and read its shipdate
        if (!_ssbdb->lineorder_man()->load(_prline, input.data)) {
            assert(false); // RC(se_WRONG_DISK_DATA)
        }

        return (true);
    }

    
    // Projection
    void project(tuple_t &d, const tuple_t &s) {        

        lo_tuple *dest;
        dest = aligned_cast<lo_tuple>(d.data);

        _prline->get_value(2, _lineorder.LO_CUSTKEY);
        _prline->get_value(4, _lineorder.LO_SUPPKEY);
        _prline->get_value(5, _lineorder.LO_ORDERDATE);
        _prline->get_value(12, _lineorder.LO_REVENUE);


        TRACE( TRACE_RECORD_FLOW, "%d|%d|%d|%lf --d\n",
               _lineorder.LO_CUSTKEY,
               _lineorder.LO_SUPPKEY,
               _lineorder.LO_ORDERDATE,
               _lineorder.LO_REVENUE);

        dest->LO_CUSTKEY = _lineorder.LO_CUSTKEY;
        dest->LO_SUPPKEY = _lineorder.LO_SUPPKEY;
        dest->LO_ORDERDATE = _lineorder.LO_ORDERDATE;
        dest->LO_REVENUE = _lineorder.LO_REVENUE;

    }

    lineorder_tscan_filter_t* clone() const {
        return new lineorder_tscan_filter_t(*this);
    }

    c_str to_string() const {
        return c_str("lineorder_tscan_filter_t()");
    }
};


class customer_tscan_filter_t : public tuple_filter_t 
{
private:
    ShoreSSBEnv* _ssbdb;
    row_impl<customer_t>* _prcust;
    rep_row_t _rr;

    ssb_customer_tuple _customer;

  /*VARIABLES TAKING VALUES FROM INPUT FOR SELECTION*/
    char NATION[16];
public:

    customer_tscan_filter_t(ShoreSSBEnv* ssbdb, q3_2_input_t &in) 
        : tuple_filter_t(ssbdb->customer_desc()->maxsize()), _ssbdb(ssbdb)
    {

    	// Get a customer tupple from the tuple cache and allocate space
        _prcust = _ssbdb->customer_man()->get_tuple();
        _rr.set_ts(_ssbdb->customer_man()->ts(),
                   _ssbdb->customer_desc()->maxsize());
        _prcust->_rep = &_rr;

	
	strcpy(NATION,"UNITED STATES");
    }

    ~customer_tscan_filter_t()
    {
        // Give back the customer tuple 
        _ssbdb->customer_man()->give_tuple(_prcust);
    }


    // Predication
    bool select(const tuple_t &input) {

        // Get next customer and read its shipdate
        if (!_ssbdb->customer_man()->load(_prcust, input.data)) {
            assert(false); // RC(se_WRONG_DISK_DATA)
        }

        _prcust->get_value(5, _customer.C_NATION, 15);

        TRACE( TRACE_RECORD_FLOW, "NATION |%s --d\n",
               _customer.C_NATION);
	
	if (strcmp(_customer.C_NATION,NATION)==0)
	  return (true);
	else
	  return (false);

    }

    
    // Projection
    void project(tuple_t &d, const tuple_t &s) {        

        c_tuple *dest;
        dest = aligned_cast<c_tuple>(d.data);

        _prcust->get_value(0, _customer.C_CUSTKEY);
        _prcust->get_value(3, _customer.C_CITY, 10);

        TRACE( TRACE_RECORD_FLOW, "%d|%s --d\n",
               _customer.C_CUSTKEY,
               _customer.C_CITY);


        dest->C_CUSTKEY = _customer.C_CUSTKEY;
        strcpy(dest->C_CITY,_customer.C_CITY);
    }

    customer_tscan_filter_t* clone() const {
        return new customer_tscan_filter_t(*this);
    }

    c_str to_string() const {
        return c_str("customer_tscan_filter_t()");
    }
};


class supplier_tscan_filter_t : public tuple_filter_t 
{
private:
    ShoreSSBEnv* _ssbdb;
    row_impl<supplier_t>* _prsupp;
    rep_row_t _rr;

    ssb_supplier_tuple _supplier;

  /*VARIABLES TAKING VALUES FROM INPUT FOR SELECTION*/
    char NATION[16];
public:

    supplier_tscan_filter_t(ShoreSSBEnv* ssbdb, q3_2_input_t &in) 
        : tuple_filter_t(ssbdb->supplier_desc()->maxsize()), _ssbdb(ssbdb)
    {

    	// Get a supplier tupple from the tuple cache and allocate space
        _prsupp = _ssbdb->supplier_man()->get_tuple();
        _rr.set_ts(_ssbdb->supplier_man()->ts(),
                   _ssbdb->supplier_desc()->maxsize());
        _prsupp->_rep = &_rr;

	
	strcpy(NATION,"UNITED STATES");
    }

    ~supplier_tscan_filter_t()
    {
        // Give back the supplier tuple 
        _ssbdb->supplier_man()->give_tuple(_prsupp);
    }


    // Predication
    bool select(const tuple_t &input) {

        // Get next supplier and read its shipdate
        if (!_ssbdb->supplier_man()->load(_prsupp, input.data)) {
            assert(false); // RC(se_WRONG_DISK_DATA)
        }

        _prsupp->get_value(5, _supplier.S_NATION, 15);

        TRACE( TRACE_RECORD_FLOW, "NATION |%s --d\n",
               _supplier.S_NATION);

	if (strcmp(_supplier.S_NATION,NATION)==0)
	  return (true);
	else
	  return (false);
    }

    
    // Projection
    void project(tuple_t &d, const tuple_t &s) {        

        s_tuple *dest;
        dest = aligned_cast<s_tuple>(d.data);

        _prsupp->get_value(0, _supplier.S_SUPPKEY);
        _prsupp->get_value(3, _supplier.S_CITY, 10);

        TRACE( TRACE_RECORD_FLOW, "%d|%s --d\n",
               _supplier.S_SUPPKEY,
               _supplier.S_CITY);


        dest->S_SUPPKEY = _supplier.S_SUPPKEY;
        strcpy(dest->S_CITY,_supplier.S_CITY);
    }

    supplier_tscan_filter_t* clone() const {
        return new supplier_tscan_filter_t(*this);
    }

    c_str to_string() const {
        return c_str("supplier_tscan_filter_t()");
    }
};



class date_tscan_filter_t : public tuple_filter_t 
{
private:
    ShoreSSBEnv* _ssbdb;
    row_impl<date_t>* _prdate;
    rep_row_t _rr;

    ssb_date_tuple _date;

  /*VARIABLES TAKING VALUES FROM INPUT FOR SELECTION*/
    int YEAR_LOW;
    int YEAR_HIGH;

public:

    date_tscan_filter_t(ShoreSSBEnv* ssbdb, q3_2_input_t &in) 
        : tuple_filter_t(ssbdb->date_desc()->maxsize()), _ssbdb(ssbdb)
    {

    	// Get a date tupple from the tuple cache and allocate space
        _prdate = _ssbdb->date_man()->get_tuple();
        _rr.set_ts(_ssbdb->date_man()->ts(),
                   _ssbdb->date_desc()->maxsize());
        _prdate->_rep = &_rr;

	YEAR_LOW=1992;
	YEAR_HIGH=1997;
    }

    ~date_tscan_filter_t()
    {
        // Give back the date tuple 
        _ssbdb->date_man()->give_tuple(_prdate);
    }


    // Predication
    bool select(const tuple_t &input) {

        // Get next date and read its shipdate
        if (!_ssbdb->date_man()->load(_prdate, input.data)) {
            assert(false); // RC(se_WRONG_DISK_DATA)
        }

        _prdate->get_value(4, _date.D_YEAR);

        TRACE( TRACE_RECORD_FLOW, "YEAR |%d --d\n",
               _date.D_YEAR);
	
	if (_date.D_YEAR>=YEAR_LOW && _date.D_YEAR<=YEAR_HIGH)
	  return (true);
	else
	  return (false);

    }

    
    // Projection
    void project(tuple_t &d, const tuple_t &s) {        

        d_tuple *dest;
        dest = aligned_cast<d_tuple>(d.data);

        _prdate->get_value(0, _date.D_DATEKEY);
        _prdate->get_value(4, _date.D_YEAR);

        TRACE( TRACE_RECORD_FLOW, "%d|%d --d\n",
               _date.D_DATEKEY,
               _date.D_YEAR);


        dest->D_DATEKEY = _date.D_DATEKEY;
        dest->D_YEAR=_date.D_YEAR;
    }

    date_tscan_filter_t* clone() const {
        return new date_tscan_filter_t(*this);
    }

    c_str to_string() const {
        return c_str("date_tscan_filter_t()");
    }
};





/*struct count_aggregate_t : public tuple_aggregate_t {
    default_key_extractor_t _extractor;
    
    count_aggregate_t()
        : tuple_aggregate_t(sizeof(projected_tuple))
    {
    }
    virtual key_extractor_t* key_extractor() { return &_extractor; }
    
    virtual void aggregate(char* agg_data, const tuple_t &) {
        count_tuple* agg = aligned_cast<count_tuple>(agg_data);
        agg->COUNT++;
    }

    virtual void finish(tuple_t &d, const char* agg_data) {
        memcpy(d.data, agg_data, tuple_size());
    }
    virtual count_aggregate_t* clone() const {
        return new count_aggregate_t(*this);
    }
    virtual c_str to_string() const {
        return "count_aggregate_t";
    }
};
*/








class ssb_q3_2_process_tuple_t : public process_tuple_t 
{    
public:
        
    void begin() {
        TRACE(TRACE_QUERY_RESULTS, "*** q3_2 ANSWER ...\n");
        TRACE(TRACE_QUERY_RESULTS, "*** SUM_QTY\tSUM_BASE\tSUM_DISC...\n");
    }
    
    virtual void process(const tuple_t& output) {
        projected_tuple *tuple;
        tuple = aligned_cast<projected_tuple>(output.data);
        TRACE(TRACE_QUERY_RESULTS, "%d --\n",
	      tuple->KEY);
    }
};



/******************************************************************** 
 *
 * QPIPE q3_2 - Packet creation and submission
 *
 ********************************************************************/

w_rc_t ShoreSSBEnv::xct_qpipe_q3_2(const int xct_id, 
                                  q3_2_input_t& in)
{
    TRACE( TRACE_ALWAYS, "********** q3_2 *********\n");

   
    policy_t* dp = this->get_sched_policy();
    xct_t* pxct = smthread_t::me()->xct();
    

    // TSCAN PACKET
    tuple_fifo* lo_out_buffer = new tuple_fifo(sizeof(lo_tuple));
        tscan_packet_t* lo_tscan_packet =
        new tscan_packet_t("TSCAN LINEORDER",
                           lo_out_buffer,
                           new lineorder_tscan_filter_t(this),
                           this->db(),
                           _plineorder_desc.get(),
                           pxct
                           //, SH 
                           );


    /*    tuple_fifo* agg_output = new tuple_fifo(sizeof(count_tuple));
    aggregate_packet_t* agg_packet =
                new aggregate_packet_t(c_str("COUNT_AGGREGATE"),
                               agg_output,
                               new trivial_filter_t(agg_output->tuple_size()),
                               new count_aggregate_t(),
                               new int_key_extractor_t(),
                               date_tscan_packet);
    
        new partial_aggregate_packet_t(c_str("COUNT_AGGREGATE"),
                               agg_output,
                               new trivial_filter_t(agg_output->tuple_size()),
                               qlineorder_tscan_packet,
                               new count_aggregate_t(),
                               new default_key_extractor_t(),
                               new int_key_compare_t());
    */
    qpipe::query_state_t* qs = dp->query_state_create();
    lo_tscan_packet->assign_query_state(qs);
    //agg_packet->assign_query_state(qs);
        
    // Dispatch packet
    ssb_q3_2_process_tuple_t pt;
    process_query(lo_tscan_packet, pt);
    dp->query_state_destroy(qs);

    return (RCOK); 
}


EXIT_NAMESPACE(ssb);
