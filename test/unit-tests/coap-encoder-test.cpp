// 12/8/2019 - currently the explicitly named netbuf code is netbuf mk. 1
// we actually want to keep it around and eventually upgrade to netbuf mk. 2 for
// pre C++11 compatibility

#include <catch.hpp>

#include <stddef.h>

#include "coap-encoder.h"
#include "mc/memory.h"
#include "coap_transmission.h"
#include "coap/token.h"
#include "mc/experimental.h"
#include "coap/encoder.hpp"

#include "exp/netbuf.h"

#include "coap/encoder/streambuf.h"
#include <estd/string.h>

#include "test-data.h"

using namespace moducom::coap;
using namespace moducom::pipeline;

#include <stdio.h>

// Only for catch comparison
template <class TLeft, class TRight>
void compare_array(const TLeft& left, const TRight& right)
{
    for(int i = 0; i < sizeof(TLeft); i++)
    {
        const auto& l = left[i];
        const auto& r = right[i];

        INFO("Comparing at: " << i);
        REQUIRE(l == r);
    }
}


TEST_CASE("CoAP encoder tests", "[coap-encoder]")
{
    typedef Option number_t;

#ifdef UNUSED
    SECTION("1")
    {
        moducom::pipeline::layer3::MemoryChunk<128> chunk;
        moducom::pipeline::layer3::SimpleBufferedPipelineWriter writer(chunk);
        moducom::coap::experimental::BlockingEncoder encoder(writer);

        encoder.header(Header::Get);

        encoder.option(number_t::ETag, "etag");
        encoder.option(number_t::UriPath, "query");

        encoder.payload("A payload");

        const int option_pos = 4;

        REQUIRE(chunk[option_pos + 0] == 0x44);
        REQUIRE(chunk[option_pos + 5] == 0x75);
        REQUIRE(chunk[option_pos + 6] == 'q');
    }
    SECTION("Experimental encoder")
    {
        moducom::pipeline::layer3::MemoryChunk<128> chunk;
        moducom::pipeline::layer3::SimpleBufferedPipelineWriter writer(chunk);
        moducom::coap::experimental::BufferedEncoder encoder(writer);
        Header& header = *encoder.header();
        moducom::coap::layer1::Token& token = *encoder.token();

        header.token_length(2);
        header.type(Header::Confirmable);
        header.code(Header::Get);
        header.message_id(0xAA);

        token[0] = 1;
        token[1] = 2;

        encoder.option(number_t::UriPath,
                       moducom::pipeline::MemoryChunk((uint8_t*)"Test", 4));

        moducom::pipeline::MemoryChunk chunk2 = encoder.payload();

        int l = sprintf((char*)chunk2.data(), "Guess we'll do it directly %s.", "won't we");

        // remove null terminator
        encoder.advance(l - 1);
        //encoder.payload("Now for something completely different");

        encoder.complete();

        const int option_pos = 5;

//#if !defined (__APPLE__)
        // FIX: on MBP QTcreator this fails
        REQUIRE(chunk[4] == 1);
        REQUIRE(chunk[5] == 2);
        REQUIRE(chunk[option_pos + 6] == 0xFF);
//#endif
    }
#endif
    // TODO: Move this to more proper test file location
    SECTION("Read Only Memory Buffer")
    {
        // TODO: Make a layer2::string merely to be a wrapper around const char*
        MemoryChunk::readonly_t str = MemoryChunk::readonly_t::str_ptr("Test");

        REQUIRE(str.length() == 4);
    }
    SECTION("NetBuf encoder")
    {
        typedef moducom::io::experimental::layer2::NetBufMemory<256> netbuf_t;

        // also works, external buf
        //netbuf_t netbuf;
        //NetBufEncoder<netbuf_t&> encoder(netbuf);

        NetBufEncoder<netbuf_t> encoder;
        netbuf_t& netbuf = encoder.netbuf();
        const uint8_t* data = netbuf.chunk().data();

        moducom::coap::layer2::Token token;

        token.resize(3);
        token[0] = 1;
        token[1] = 2;
        token[2] = 3;

        Header header;

        header.token_length(token.size());

        Option::Numbers n = Option::UriPath;

        encoder.header(header);

        // Header is always 4
        size_t expected_msg_size = 4;

        REQUIRE(netbuf.length_processed() == expected_msg_size);

        encoder.token(token);

        REQUIRE(data[expected_msg_size] == 1);
        REQUIRE(data[expected_msg_size + 1] == 2);

        expected_msg_size += token.size();

        REQUIRE(netbuf.length_processed() == expected_msg_size);

        encoder.option(n, MemoryChunk((uint8_t*)"test", 4));

        REQUIRE(0xB4 == data[expected_msg_size]); // option 11, length 4

        netbuf_t& w = encoder.netbuf();

        // + option "header" of size 1 + option_value of 4
        expected_msg_size += 1 + 4;

        REQUIRE(netbuf.length_processed() == expected_msg_size);

        encoder.option(n, std::string("test2"));

        REQUIRE(data[expected_msg_size] == 0x05); // still option 11, length 5

        // option "header" of size 1 + option_value of size 5
        expected_msg_size += (1 + 5);

        REQUIRE(netbuf.length_processed() == expected_msg_size);

        encoder.option(Option::ContentFormat, 50);

        REQUIRE(data[expected_msg_size] == 0x11);
        REQUIRE(data[expected_msg_size + 1] == 50);

        expected_msg_size += 2; // option 'header' + one byte for '50'

        REQUIRE(netbuf.length_processed() == expected_msg_size);

        encoder.payload(std::string("payload"));

        REQUIRE(data[expected_msg_size] == COAP_PAYLOAD_MARKER); // payload marker
        REQUIRE(data[expected_msg_size + 1] == 'p');

        expected_msg_size += (1 + 7); // payload marker + "payload"

        REQUIRE(netbuf.length_processed() == expected_msg_size);

        encoder.complete();
    }
    SECTION("synthetic test 1")
    {
        typedef moducom::io::experimental::layer2::NetBufMemory<256> netbuf_t;

        NetBufEncoder<netbuf_t> encoder;
        uint8_t token[] = { 0, 1, 2, 3 };

        encoder.header(Header(Header::Confirmable, Header::Code::Content));
        encoder.token(token);

        encoder.payload_header();

        estd::layer2::const_string s = "hi2u";

        encoder << s;
        encoder.complete();

        netbuf_t& netbuf = encoder.netbuf();

        REQUIRE(netbuf.processed()[4] == token[0]);
        REQUIRE(netbuf.processed()[5] == token[1]);
        REQUIRE(netbuf.processed()[6] == token[2]);
        REQUIRE(netbuf.processed()[7] == token[3]);
        REQUIRE(netbuf.processed()[8] == 0xFF);
        REQUIRE(netbuf.processed()[9] == s[0]);
    }
    SECTION("streambuf encoder")
    {
        // for << operator, which I'm still undecided about
        using namespace moducom::coap::experimental;

        typedef char char_type;
        typedef estd::internal::streambuf<
                estd::internal::impl::out_span_streambuf<char_type> > streambuf_type;

        estd::layer2::const_string test_str = "hi2u";

        union
        {
            char_type buffer[128];
            unsigned char ubuffer[128];
        };

        // Match up header to what we do in buffer_16bit_delta
        Header header(Header::TypeEnum::Confirmable);

        header.code(Header::Code::Codes::Get);
        header.message_id(0x0123);

        StreambufEncoder<streambuf_type> encoder(buffer);

        SECTION("experimental finalize")
        {
            encoder.finalize();
        }
        SECTION("Test 1")
        {
            const char* hello_str = "hello";
            const int hello_str_len = 5;
            const estd::layer2::const_string payload_str = "PAYLOAD";
            uint8_t* b = ubuffer;

            encoder.header(header);
            encoder.option(Option::Numbers::UriPath, hello_str);
            encoder.option(Option::Numbers::UriPath, test_str);

            encoder.option(Option::Numbers::Size1); // synthetic size 0
            encoder.payload();

            auto out = encoder.ostream();
            // FIX: layer2::const_string not playing nice with << operator, so needing to do .data()
            out << payload_str.data();
            // FIX: Perhaps ostream should be char not unsigned char?
            //out.write((uint8_t *)payload_str.data(), payload_str.size());

            REQUIRE(buffer_16bit_delta[0] == buffer[0]);
            REQUIRE(buffer_16bit_delta[1] == buffer[1]);
            REQUIRE(buffer_16bit_delta[2] == buffer[2]);
            REQUIRE(buffer_16bit_delta[3] == buffer[3]);

            b += 4;
            // --- option #1
            REQUIRE(*b++ == ((Option::Numbers::UriPath << 4) | hello_str_len) );

            estd::layer3::const_string s((char*)b, hello_str_len);

            REQUIRE(s == hello_str);

            b += hello_str_len;
            // --- option #2
            //REQUIRE(*b++ == ((Option::Numbers::UriPath << 4) | test_str.size()) );
            REQUIRE(*b++ == test_str.size());

            new (&s) estd::layer3::const_string((char*)b, test_str.size());

            REQUIRE(s.size() == test_str.size());
            REQUIRE(s == test_str);
            b += test_str.size();

            // --- option #3
            REQUIRE(*b++ == (Option::ExtendedMode::Extended8Bit << 4));
            REQUIRE(*b++ == (Option::Numbers::Size1 - Option::Numbers::UriPath) - 13);

            // PAYLOAD
            REQUIRE(*b++ == 0xFF);

            new (&s) estd::layer3::const_string((char*)b, payload_str.size());

            REQUIRE(s == payload_str);
        }
        SECTION("Regenerate buffer_16bit_delta")
        {
            // TODO: make an estd::fill and use that instead
            // clear this out to avoid false successes
            memset(buffer, 0, sizeof(buffer));

            //estd::layer1::string<2, true> s = { 0, 1 };
            // NOTE: this doesn't work because non-null terminated tracks a distinct
            // size variable
            //estd::layer1::string<2, false> s;

            //s[0] = 4;
            //s[1] = 5;
            estd::array<char, 2> v = { 4, 5 };

            auto const rdbuf = encoder.rdbuf();

            encoder.header(header);
            encoder.option((Option::Numbers)270, 1);
            rdbuf->sputc(3);
            // Not quite as interesting because strings always maintain a runtime
            // size
            //encoder.option((Option::Numbers)271, s);
            // Doesn't work yet since array doesn't share allocated_array base (yet)
            //encoder.option((Option::Numbers)271, v);
            encoder.option((Option::Numbers)271, 2);
            rdbuf->sputc(4);
            rdbuf->sputc(5);
            encoder.payload();
            for(char_type i = 0x10; i <= 0x16; i++)
                rdbuf->sputc(i);

            compare_array(buffer_16bit_delta, ubuffer);
        }
        SECTION("stream operators")
        {
            // TODO: make an estd::fill and use that instead
            // clear this out to avoid false successes
            memset(buffer, 0, sizeof(buffer));

            estd::const_buffer empty(NULLPTR, 0);

            // Consider making encode_type *itself* an ostream rather than generating one
            // this would be particularly useful for the ostream-style of tracking eof, erroneous
            // conditions etc.  Maybe call it EncoderStream?
            auto out = encoder.ostream();

            // Be aware this technique may employ blocking, though of course it depends on the underlying
            // streambuf.  Because of this, it deviates from 'streambuf' things because in estd world
            // streambuf is, if you so choose, fully free from blocking (once we nail down the architure).
            // So this is further reason to morph StreambufEncoder into EncoderStream.
            // Be aware if we do go the EncoderStream route, we have to think about whether a DecoderStream
            // makes sense for 1:1 so that the APIs aren't so different
            encoder << header << token(empty);
            encoder << option((Option::Numbers)270, 1);
            out.put(3);
            encoder << option((Option::Numbers)271, 2);
            out.put(4);
            out.put(5);
            encoder << payload;
            for(char_type i = 0x10; i <= 0x16; i++) out.put(i);

            compare_array(buffer_16bit_delta, ubuffer);
        }
    }
}
