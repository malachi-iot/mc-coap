#include <catch.hpp>

#include <string.h>
#include "cbor/encoder.h"
#include "cbor/decoder.h"
#include "platform/generic/malloc_netbuf.h"
#include "test-data.h"

using namespace moducom;


static const uint8_t* cbor_assert(CBOR::Decoder& decoder, const uint8_t* v, std::string expected)
{
    REQUIRE(decoder.type() == CBOR::String);
    REQUIRE(decoder.get_simple_value() == expected.length());
    std::string s((const char*)v, expected.length());

    REQUIRE(s == expected);

    v += expected.length();
    return v;
}


static std::string decoder_get_string(CBOR::Decoder& decoder, const uint8_t** v)
{
    pipeline::MemoryChunk::readonly_t chunk = decoder.get_string_experimental(v, 999);
    std::string s((const char*)chunk.data(), chunk.length());
    return s;
}


#ifdef __APPLE__
#define __glibcxx_assert(dummy)
#elif defined(_MSC_VER)
#define __glibcxx_assert(dummy)
#else
#endif

template <class TDecoder = CBOR::Decoder>
struct DecoderHelper
{
    TDecoder decoder;
    // TODO: Need to refactor whole thing to take chunks so that we can more sensibly
    // manage max_len and its diminishing size
    const uint8_t* value;
    //size_t max_len;
    CBOR::Decoder::ParseResult result;

    DecoderHelper(const uint8_t* value) : value(value)
    {}

    DecoderHelper(TDecoder& decoder, const uint8_t* value) :
            decoder(decoder),
            value(value)
    {}

    std::string string()
    {
        pipeline::MemoryChunk::readonly_t chunk = decoder.get_string_experimental(&value, 999, &result);
        __glibcxx_assert(result == CBOR::Decoder::OK);
        std::string s((const char*)chunk.data(), chunk.length());
        return s;
    }

    int integer()
    {
        int ret_val = decoder.get_int_experimental(&value, 999, &result);
        __glibcxx_assert(result == CBOR::Decoder::OK);
        return  ret_val;
    }

    int map()
    {
        int ret_val = decoder.get_map_experimental(&value, 999, &result);
        __glibcxx_assert(result == CBOR::Decoder::OK);
        return  ret_val;
    }
};



void require_map(cbor::experimental::StreamDecoder& decoder,
                 cbor::experimental::StreamDecoder::Context& context,
                 int len)
{
    // all maps are of long-type, this is just an optional check to make sure we're decoding that right
    REQUIRE(decoder.is_long_start());
    REQUIRE(decoder.type() == cbor::Root::Map);
    REQUIRE(decoder.integer<int>() == len);

    // since maps are long-type we fast forward.  For 'indeterminate' types (Map, ItemArray) this means move to
    // *contained* data item.
    decoder.fast_forward(context);

}

void require_string(cbor::experimental::StreamDecoder& decoder,
                    cbor::experimental::StreamDecoder::Context& context,
                    const char* str)
{
    // all strings are of long-type, this is just an optional check to make sure we're decoding that right
    REQUIRE(decoder.is_long_start());
    REQUIRE(decoder.type() == cbor::Root::String);

    // this call auto fast forwards.  For 'non-indeterminate' data types, which string is, fast-forward moves
    // completely past the data payload of the data item
    estd::layer3::const_string s = decoder.string_experimental(context);

    REQUIRE(s == str);
}

TEST_CASE("CBOR decoder tests", "[cbor-decoder]")
{
    SECTION("True/false test")
    {
        CBOR::Decoder decoder;
        uint8_t value = 0b11100000 | 21;

        do
        {
            decoder.process(value);
        }
        while(decoder.state() != CBOR::Decoder::Done);

        REQUIRE(decoder.is_simple_type_bool() == true);
    }
    SECTION("16-bit integer test")
    {
        CBOR::Decoder decoder;
        uint8_t value[] = { CBOR::Decoder::bits_16, 0xF0, 0x77 };
        uint8_t* v= value;

        do
        {
            decoder.process(*v++);
        }
        while(decoder.state() != CBOR::Decoder::Done);

        REQUIRE(decoder.is_simple_type_bool() == false);
        REQUIRE(decoder.type() == CBOR::PositiveInteger);
        REQUIRE(decoder.is_16bit());
        uint16_t retrieved = decoder.value<uint16_t>();
        REQUIRE(retrieved == 0xF077);
    }
    SECTION("Synthetic cred-set test: very manual")
    {
        CBOR::Decoder decoder;
        const uint8_t* v = cbor_cred;

        v = decoder.process(v);

        REQUIRE(decoder.type() == CBOR::Map);
        uint8_t len = decoder.get_simple_value();
        REQUIRE(len == 2);

        v = decoder.process(v);
        v = cbor_assert(decoder, v, "ssid");

        v = decoder.process(v);
        v = cbor_assert(decoder, v, "ssid_name");

        v = decoder.process(v);
        v = cbor_assert(decoder, v, "pass");

        v = decoder.process(v);
        v = cbor_assert(decoder, v, "secret");
    }
    SECTION("Synthetic cred-set test: with helpers")
    {
        CBOR::Decoder decoder;
        const uint8_t* v = cbor_cred;

        REQUIRE(decoder.get_map_experimental(&v, 999) == 2);
        REQUIRE(decoder_get_string(decoder, &v) == "ssid");
        REQUIRE(decoder_get_string(decoder, &v) == "ssid_name");
        REQUIRE(decoder_get_string(decoder, &v) == "pass");
        REQUIRE(decoder_get_string(decoder, &v) == "secret");

        v = cbor_cred;
        DecoderHelper<CBOR::Decoder&> dh(decoder, v);

        REQUIRE(dh.map() == 2);
        REQUIRE(dh.string() == "ssid");
        REQUIRE(dh.string() == "ssid_name");
        REQUIRE(dh.string() == "pass");
        REQUIRE(dh.string() == "secret");
    }
    SECTION("int decoder")
    {
        DecoderHelper<> dh(cbor_int);

        REQUIRE(dh.map() == 1);
        REQUIRE(dh.string() == "val");
        REQUIRE(dh.integer() == -123456789);
    }
    SECTION("basic encoder: layer1")
    {
        cbor::layer1::Encoder encoder;

        encoder.integer(10);

        REQUIRE(encoder.data()[0] == 10);

        encoder.integer(100);
        encoder.integer(1000);
    }
    SECTION("basic encoder: layer3")
    {
        uint8_t buffer[9];
        cbor::layer3::Encoder encoder(buffer);

        encoder.integer(10);

        REQUIRE(encoder.data()[0] == 10);

        encoder.integer(100);

        REQUIRE(encoder.data()[0] == CBOR::Decoder::bits_8);

        encoder.integer(1000);
    }
    SECTION("more advanced encoder: layer1")
    {
        cbor::layer1::Encoder encoder;

        encoder.integer(-123456789);

        REQUIRE(encoder.data()[0] == (CBOR::NegativeInteger << 5 | CBOR::Decoder::bits_32));

        int len = encoder.string(5);

        REQUIRE(len == 1);
        REQUIRE(encoder.data()[0] == (CBOR::String << 5 | 5));
    }
    SECTION("revamped cbor decoder")
    {
        cbor::Decoder decoder;
        const uint8_t* data = cbor_int;

        decoder.process_iterate(*data); // finish with Header
        decoder.process_iterate(*data++); // finish with HeaderDone
        decoder.process_iterate(*data); // finish with LongStart (because a nested map is present)
        REQUIRE(decoder.state() == cbor::Decoder::LongStart);
        REQUIRE(decoder.type() == cbor::Decoder::Map);

        decoder.fast_forward(); // nested types, fast forward by their state

        decoder.process_iterate(*data); // finished with HeaderStart
        decoder.process_iterate(*data); // finish with Header
        decoder.process_iterate(*data++); // finish with HeaderDone
        decoder.process_iterate(*data); // finish with LongStart
        REQUIRE(decoder.state() == cbor::Decoder::LongStart);
        REQUIRE(decoder.type() == cbor::Decoder::String);
        REQUIRE(decoder.integer<uint8_t>() == 3);

        // skip the data bytes
        data += 3;
        decoder.fast_forward();

        decoder.process_iterate(*data); // finish with Header
        decoder.process_iterate(*data++); // finish with HeaderDone
        decoder.process_iterate(*data); // finish with AdditionalStart
        REQUIRE(decoder.state() == cbor::Decoder::AdditionalStart);
        REQUIRE(decoder.type() == cbor::Decoder::NegativeInteger);

        decoder.process_iterate(*data); // finish with Additional
        decoder.process_iterate(*data++); // finish with Additional
        decoder.process_iterate(*data++); // finish with Additional
        decoder.process_iterate(*data++); // finish with Additional
        decoder.process_iterate(*data++); // finish with AdditionalDone
        REQUIRE(decoder.state() == cbor::Decoder::AdditionalDone);
        REQUIRE(decoder.integer<int32_t>() == -123456789);
    }
    SECTION("revamped netbuf encoder")
    {
        using namespace moducom::io::experimental;
        using namespace moducom::coap;
        using namespace moducom::cbor;

        NetBufEncoder<NetBufDynamicExperimental> encoder;

        const char* _s = "Hello world";
        estd::layer2::const_string s = _s;

        encoder.string(s);
        encoder.integer(-5);
        encoder.integer(0x1234);
        encoder.map(4);
        encoder.string("Hi");
#ifdef FEATURE_CPP_INITIALIZER_LIST
        encoder.items({ 1, 2, 3 });
#endif

        const uint8_t* d = encoder.netbuf().processed();
        int len = encoder.netbuf().length_processed();

        REQUIRE(len == (11 + 1 + 1 + 3 + 1 + 3
#ifdef FEATURE_CPP_INITIALIZER_LIST
                + 4
#endif
                ));
        REQUIRE(*d++ == 0x6B);

        estd::layer3::basic_string<char, false> s2(s.size(), (char*)d, s.size());

        REQUIRE(s2 == _s);
        d += s.size();

        REQUIRE(*d++ == 0x24); // -5
        REQUIRE(*d++ == cbor::Root::header(cbor::Root::UnsignedInteger, cbor::Root::bits_16));
        REQUIRE(*d++ == 0x12);
        REQUIRE(*d++ == 0x34);
        REQUIRE(*d++ == 0xA4); // map(4)

        REQUIRE(*d++ == 0x62); // string of 2
        REQUIRE(*d++ == 'H');
        REQUIRE(*d++ == 'i');

#ifdef FEATURE_CPP_INITIALIZER_LIST
        REQUIRE(*d++ == 0x83);
        REQUIRE(*d++ == 1);
        REQUIRE(*d++ == 2);
        REQUIRE(*d++ == 3);
#endif
    }
    SECTION("experimental cbor decoder")
    {
        using namespace cbor::experimental;

        pipeline::MemoryChunk::readonly_t chunk(cbor_int);
        StreamDecoder decoder;
        StreamDecoder::Context context(chunk, true);

        decoder.process(context);

        REQUIRE(decoder.is_long_start());
        REQUIRE(decoder.type() == cbor::Root::Map);
        REQUIRE(decoder.integer<int>() == 1);

        decoder.fast_forward(context);

        REQUIRE(decoder.is_long_start());
        REQUIRE(decoder.type() == cbor::Root::String);

        //estd::layer3::const_string s((const char*)context.unprocessed(), decoder.integer<int>());
        //decoder.fast_forward(context);

        estd::layer3::const_string s = decoder.string_experimental(context);

        REQUIRE(s == "val");

        REQUIRE(!decoder.is_long_start());
        REQUIRE(decoder.type() == cbor::Root::NegativeInteger);
        REQUIRE(decoder.integer<int>() == -123456789);
    }
    SECTION("experimental signature evaluation for cbor decoder")
    {
        using namespace cbor::experimental;

        pipeline::MemoryChunk::readonly_t chunk(cbor_int);
        StreamDecoder decoder;
        StreamDecoder::Context context(chunk, true);

        SimpleData::is_false(decoder);
    }
    SECTION("Next gen synthetic cred test")
    {
        using namespace cbor::experimental;

        pipeline::MemoryChunk::readonly_t chunk(cbor_cred);
        StreamDecoder decoder;
        StreamDecoder::Context context(chunk, true);

        decoder.process(context);

        REQUIRE(decoder.is_long_start());
        REQUIRE(decoder.type() == cbor::Root::Map);
        REQUIRE(decoder.integer<int>() == 2);

        decoder.fast_forward(context);

        require_string(decoder, context, "ssid");
        require_string(decoder, context, "ssid_name");
        // NOTE: technically this is now map item #2.  Would be nice to check for this somehow
        require_string(decoder, context, "pass");
        require_string(decoder, context, "secret");
    }
}
