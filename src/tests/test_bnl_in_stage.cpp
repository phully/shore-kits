// -*- mode:C++; c-basic-offset:4 -*-

#include "engine/thread.h"
#include "engine/core/stage_container.h"
#include "engine/stages/func_call.h"
#include "engine/stages/bnl_in.h"
#include "trace.h"
#include "qpipe_panic.h"

#include "tests/common.h"

#include <vector>
#include <algorithm>

using std::vector;

using namespace qpipe;




int main(int argc, char* argv[]) {

    thread_init();


    // parse output filename
    if ( argc < 2 ) {
	TRACE(TRACE_ALWAYS, "Usage: %s <tuple count>\n", argv[0]);
	exit(-1);
    }
    int num_tuples = atoi(argv[1]);
    if ( num_tuples == 0 ) {
	TRACE(TRACE_ALWAYS, "Invalid tuple count %s\n", argv[1]);
	exit(-1);
    }


    register_stage<func_call_stage_t>(2);
    register_stage<bnl_in_stage_t>(1);


    
    tuple_buffer_t* left_int_buffer = new tuple_buffer_t(sizeof(int));
    char* left_packet_id = copy_string("LEFT_PACKET");
    struct int_tuple_writer_info_s left_writer_info(left_int_buffer, num_tuples, 0);

    func_call_packet_t* left_packet = 
	new func_call_packet_t(left_packet_id,
                               left_int_buffer, 
                               new trivial_filter_t(sizeof(int)), // unused, cannot be NULL
                               shuffled_triangle_int_tuple_writer_fc,
                               &left_writer_info);
    
    tuple_buffer_t* right_int_buffer = new tuple_buffer_t(sizeof(int));
    char* right_packet_id = copy_string("RIGHT_PACKET");
    struct int_tuple_writer_info_s right_writer_info(right_int_buffer, num_tuples, num_tuples/2);
    func_call_packet_t* right_packet = 
	new func_call_packet_t(right_packet_id,
                               right_int_buffer, 
                               new trivial_filter_t(sizeof(int)), // unused, cannot be NULL
                               shuffled_triangle_int_tuple_writer_fc,
                               &right_writer_info);
    
    
    tuple_buffer_t* output_buffer = new tuple_buffer_t(sizeof(int));

    char* in_packet_id = copy_string("BNL_IN_PACKET_1");
    bnl_in_packet_t* in_packet =
        new bnl_in_packet_t( in_packet_id,
                             output_buffer,
                             new trivial_filter_t(sizeof(int)),
                             left_packet,
                             new tuple_source_once_t(right_packet),
                             new int_key_extractor_t(),
                             new int_key_compare_t(),
                             true );
    dispatcher_t::dispatch_packet(in_packet);
    
    
    tuple_t output;
    while(!output_buffer->get_tuple(output))
        TRACE(TRACE_ALWAYS, "Value: %d\n", *(int*)output.data);
    TRACE(TRACE_ALWAYS, "TEST DONE\n");
    
    return 0;
}
