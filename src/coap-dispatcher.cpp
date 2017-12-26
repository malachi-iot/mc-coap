//
// Created by Malachi Burke on 12/26/17.
//

#include "coap-dispatcher.h"

namespace moducom { namespace coap {

namespace experimental {

// TODO: Need a smooth way to ascertain end of CoAP message (remember multipart/streaming
// won't have a discrete message boundary)
// NOTE: This should be dispatch_process since it may well require multiple calls per chunk
//   and consider also returning a bool flag to assist callers in that
void Dispatcher::dispatch(const pipeline::MemoryChunk& chunk)
{
    size_t pos = 0;
    bool process_done = false;

    switch(state())
    {
        case Uninitialized:
            // If necessary, initialize header decoder
            state(Header);
            break;

        case Header:
            // NOTE: Untested code
            while(pos < chunk.length && !process_done)
            {
                process_done = headerDecoder.process_iterate(chunk[pos++]);
            }

            break;

        case HeaderDone:
            dispatch_header();
            if(headerDecoder.token_length() > 0)
                state(Token);
            else
                state(OptionsStart);
            break;

        case Token:
            state(TokenDone);
            break;

        case TokenDone:
            state(OptionsStart);
            break;

        case OptionsStart:
            optionHolder.number_delta = 0;
            optionHolder.length = 0;
            state(Options);
            break;

        case Options:
            dispatch_option(chunk);

            // FIX: We need to check for the 0xFF payload marker still

            if(optionDecoder.state() == OptionDecoder::OptionValueDone)
                state(OptionsDone);

            break;

        case OptionsDone:
            // TODO: Need to peer into incoming data stream to determine if a payload is on the way
            // or not
            state(Payload);
            break;

        case Payload:
        {
            // FIX: Need this clued in by caller due to streaming possibility
            bool last_payload_chunk = true;
            dispatch_payload(chunk, last_payload_chunk);
            state(PayloadDone);
            break;
        }

        case PayloadDone:
            //dispatch_payload(chunk, true);
            state(Done);
            break;

        case Done:
            state(Uninitialized);
            break;
    }
}

void Dispatcher::dispatch_header()
{
    handler_t* handler = head();

    while(handler != NULLPTR)
    {
        handler->on_header(headerDecoder);
        handler = handler->next();
    }
}

// also handles pre-dispatch processing
// 100% untested
void Dispatcher::dispatch_option(const pipeline::MemoryChunk& optionChunk)
{
    size_t processed_length = optionDecoder.process_iterate(optionChunk, &optionHolder);

    // FIX: Kludgey, can really be cleaned up and optimized
    // ensures that our linked list doesn't execute process_iterate multiple times to acquire
    // option-value.  Instead, our linked list looping might need to happen per state() switched on
    bool needs_value_processed = true;

    handler_t* handler = head();

    while(handler != NULLPTR)
    {
        if(handler->is_interested())
        {
            switch (optionDecoder.state())
            {
                case OptionDecoder::OptionLengthDone:
                case OptionDecoder::OptionDeltaAndLengthDone:
                {
                    handler->on_option((IOptionInput::number_t) optionHolder.number_delta, optionHolder.length);
                    // TODO: Need to demarkate boundary here too so that on_option knows where boundary starts
                    pipeline::MemoryChunk partialChunk(optionChunk.data + processed_length, 0);

                    if(needs_value_processed)
                    {
                        processed_length = optionDecoder.process_iterate(optionChunk, &optionHolder);
                        needs_value_processed = false;
                    }

                    partialChunk.length = processed_length;
                    bool full_option_value = optionDecoder.state() == OptionDecoder::OptionValueDone;
                    handler->on_option(partialChunk, full_option_value);
                    break;
                }

                case OptionDecoder::OptionValue:
                {
                    // FIX: Not yet tested
                    //processed_length = optionDecoder.process_iterate(optionChunk, NULLPTR);
                    //pipeline::MemoryChunk partialChunk(optionChunk.data, processed_length);
                    //bool full_option_value = optionDecoder.state() == OptionDecoder::OptionValueDone;
                    //handler->on_option(partialChunk, full_option_value);
                    handler->on_option(optionChunk, false);
                    break;
                }

                case OptionDecoder::OptionValueDone:
                {
                    // FIX: Not yet supported
                    // FIX: Not quite correct code.  We need to suss out how large
                    // the chunk we're passing through really is and of course
                    // at what boundary it starts
                    //processed_length = optionDecoder.process_iterate(optionChunk, NULLPTR);
                    handler->on_option(optionChunk, true);
                    break;
                }

                default:break;
            }
        }

        handler = handler->next();
    }
}


void Dispatcher::dispatch_payload(const pipeline::MemoryChunk& payloadChunk, bool last_chunk)
{
    //size_t processed_length = optionDecoder.process_iterate(payloadChunk, NULLPTR);

    handler_t* handler = head();

    while(handler != NULLPTR)
    {
        //handler->on_payload()
    }
}

}

}}