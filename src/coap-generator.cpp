//
// Created by malachi on 11/13/17.
//

#include "coap-generator.h"

namespace moducom { namespace coap {

typedef size_t (*pipeline_output_helper_fn_t)(moducom::pipeline::MemoryChunk& chunk, void* context);

// FIX:
// Experimental
// Not even close to ready for primetime.  Doesn't handle "in flight" position
// state, and no looping helpers
template <size_t buffer_length>
bool pipeline_output_helper(pipeline::experimental::IBufferProviderPipeline& output,
                            pipeline_output_helper_fn_t pipeline_output_helper_fn,
                            void* context = NULLPTR)
{
    size_t advanceable = output.advanceable();

    if(advanceable == 0) return false;

    pipeline::PipelineMessage message = output.get_buffer(advanceable);

    if(message.length > 0)
    {
        // FIX: this expects advanceable to always exceed needs of pipeline_output_helper_fn
        // but that very well might not be true
        pipeline_output_helper_fn(message, context);
    }
    else
    {
        uint8_t buffer[buffer_length];
        // TODO: optimize, we are wasting PipelineMessage stack at this point
        pipeline::MemoryChunk chunk(buffer, buffer_length);

        pipeline_output_helper_fn(chunk, context);
    }
}


bool CoAPGenerator::output_option_next(const experimental::layer2::OptionBase& option)
{
    _option_state_t& option_state = get_option_state();

    option_state.next(option);
}


bool CoAPGenerator::output_option_iterate(const experimental::layer2::OptionBase& option)
{
    typedef _option_state_t statemachine_t;
    typedef statemachine_t::output_t output_t;

    statemachine_t& option_state = get_option_state();

    switch(state)
    {
        case CoAP::Parser::HeaderDone:
            // signals initialization of options phase

            // placement new to get around union C++03 limitation
            new (get_option_state_ptr()) statemachine_t(option);

            break;
        case CoAP::Parser::Options:
        {
            if (&option != &option_state.get_option_base())
            {
                option_state.next(option);
                // reset/advance options parser
            }

            size_t advanceable = output.advanceable();

            if(advanceable == 0) return false;

            if(option_state.sub_state() == CoAP::Parser::OptionLengthDone)
            {
                if(option.length < advanceable)
                {
                    // TODO: must also check to see if option is a string or opaque type
                    // NOTE: this isn't handling zero copy as well as we'd like, this
                    // suggests a copy every time for at least the value portion of the
                    // option.  Instead, we need a specialized output_option_iterate
                    // which only takes length and number (type) and then we manually
                    // stuff value through similar to a payload
                    //output.write(option.value_opaque, option.length);
                }
            }

            output_t out = option_state.generate_iterate();
            if(out == statemachine_t::signal_continue) return false;
            uint8_t _out = out;

            // copy operation of one character is much preferred to buffer availability
            // juggling
            output.write(&_out, 1);
            return true;
        }
        case CoAP::Parser::OptionsDone:
            // probably we won't actually know this here
            // for completeness would like to do a placement delete, but I forgot
            // the syntax
            break;
    }
}

bool CoAPGenerator::output_payload_iterate(const pipeline::MemoryChunk& chunk)
{
    if(state != CoAP::Parser::Payload)
    {
        // FIX: cheap and nasty payload mode init, but it's a start
        payload_state.pos = 0;
        state = CoAP::Parser::Payload;
    }

    size_t advanceable = output.advanceable();

    if(advanceable >= chunk.length)
    {
        // FIX: really we need to pay attention to output.write result
        output.write(chunk.data, chunk.length);
        return true;
    }
    else
    {
        // FIX: really we need to pay attention to output.write result
        output.write(chunk.data, advanceable);
        payload_state.pos += advanceable;
        return false;
    }
    //pipeline::MemoryChunk
}

}}
