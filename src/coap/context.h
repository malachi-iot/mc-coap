//
// Created by malachi on 2/25/18.
//

#pragma once

#include "../coap-token.h"
#include "../coap-header.h"
#include "../coap-features.h"

namespace moducom { namespace coap {


class TokenContext
{
#ifdef FEATURE_MCCOAP_INLINE_TOKEN
protected:
    typedef layer1::Token token_t;

    token_t _token;
#else
    typedef layer2::Token token_t;

    const token_t* _token;
#endif

public:
    TokenContext()
#ifdef FEATURE_MCCOAP_INLINE_TOKEN
    {
    }
#else
        : _token(NULLPTR)
    {}
#endif

#ifdef FEATURE_MCCOAP_INLINE_TOKEN
protected:
public:
    void token(const pipeline::MemoryChunk::readonly_t* t)
    {
        _token.copy_from(*t);
    }
#else
    // if a) incoming message has a token and b) we've decoded the token and have it
    // available, then this will be non-null
    const token_t* token() const { return _token; }

    // Since tokens arrive piecemeal and oftentimes not at all, we have a non-constructor
    // based setter
    void token(const token_t* token) { _token = token; }
#endif
};

// New-generation request context, replacement for premature coap_transmission one
// Incoming request handlers/dispatchers may extend this context, but this is a good
// foundational base class
// Called Incoming context because remember some incoming messages are RESPONSES, even
// when we are the server (ACKs, etc)
class IncomingContext : public TokenContext
{
    Header _header;

public:
    IncomingContext()
    {
        // always zero out, signifying UNINITIALIZED so that querying
        // parties don't mistakenly think we have a header
        _header.raw = 0;
    }

    // if incoming message has had its header decoded, then this is valid
    Header& header() { return _header; }

    void header(Header& header) { _header.raw = header.raw; }

    bool have_header() const { return _header.version() > 0; }

#ifdef FEATURE_MCCOAP_INLINE_TOKEN
    // FIX: All broken because currently our native token layer1/layer2 stuff:
    // a) layer1 has no sensible length (typical for layer 1)
    // b) layer2 has inline buffer (not sensible for callers of this accessor)
    inline const layer2::Token* broken_token() const
    {
        if(_header.token_length() > 0)
        {
            layer2::Token t;
            //((uint8_t*)_token.data(), _header.token_length());
            return &t;
        }
        else
            return NULLPTR;
    }


    inline const pipeline::MemoryChunk::readonly_t* token() const
    {
        size_t tkl = _header.token_length();

        if(tkl > 0)
        {
            // FIX: fix casting here
            pipeline::MemoryChunk::readonly_t ro((uint8_t*)_token.data(), tkl);

            // FIX: dangerous cast here
            return &ro;
        }
        else
        {
            return NULLPTR;
        }
    }

    void token(const pipeline::MemoryChunk::readonly_t* t)
    {
        TokenContext::token(t);
    }

    void token(const layer2::Token* t)
    {
        // FIX: make ReadOnlyMemoryChunk first class citizens across namespaces
        pipeline::MemoryChunk::readonly_t temp((uint8_t*)t->data(), t->length());
        TokenContext::token(&temp);
    }

#endif
};

}}
