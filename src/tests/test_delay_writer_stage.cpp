/* -*- mode:C++; c-basic-offset:4 -*- */

#include "stages.h"
#include "tests/common.h"
#include "workload/common.h"


class test_delay_writer_stage_process_tuple_t : public process_tuple_t {

    stopwatch_t _sw;

public:

    test_delay_writer_stage_process_tuple_t()
        : _sw()
    {
    }
    
    virtual void begin() {
        _sw.reset();
    }

    virtual void process(const tuple_t& /*output*/) {
	//int* d = aligned_cast<int>(output.data);
	TRACE(TRACE_ALWAYS, "Delay was %lf\n", _sw.time());
        _sw.reset();
    }
    
};


int main() {

    util_init();
    db_open_guard_t db_open;

    register_stage<delay_writer_stage_t>(1);
    

    // DELAY WRITER PACKET
    // the output consists of a single int
    tuple_fifo* out_buffer = new tuple_fifo(sizeof(int));
    tuple_filter_t* filter = new trivial_filter_t(out_buffer->tuple_size());
    

    delay_writer_packet_t* delay_writer_packet =
        new delay_writer_packet_t("DELAY_WRITER_PACKET",
                                  out_buffer,
                                  filter,
                                  10000,
                                  1000);


    test_delay_writer_stage_process_tuple_t pt;
    process_query(delay_writer_packet, pt);

    return 0;
}