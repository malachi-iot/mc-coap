//
// Created by malachi on 2/14/18.
//

#include "coap-uripath-dispatcher.h"

namespace moducom { namespace coap {

namespace experimental {

void UriDispatcherHandler::on_option(number_t number,
                                     const pipeline::MemoryChunk::readonly_t& option_value_part,
                                     bool last_chunk)
{
    IDispatcherHandler* handler = factory.create(option_value_part, context);

    if(handler != NULLPTR)
    {
        handler->on_option(number, option_value_part, last_chunk);
        // NOTE: This currently assumes we're doing in-place memory management
        handler->~IDispatcherHandler();
    }
}

}

}}
