#include <catch.hpp>

#include "../coap-decoder.h"
#include "test-data.h"
#include "../mc/experimental.h"
//#include "../mc/pipeline.h"
#include "../mc/experimental-factory.h"
#include "../coap-uripath-dispatcher.h"

using namespace moducom::coap;
using namespace moducom::coap::experimental;
//using namespace moducom::pipeline;

int test(experimental::FnFactoryContext context)
{
    return 7;
}

int test_wilma(experimental::FnFactoryContext context)
{
    return 77;
}


class TestBarnyObsever : public DispatcherHandlerBase
{
public:
    TestBarnyObsever(IncomingContext& context)
    {
        set_context(context);
    }

    void on_option(number_t number,
                           const moducom::pipeline::MemoryChunk::readonly_t& option_value_part,
                           bool last_chunk) OVERRIDE
    {
        // FIX: For some reason this isn't called, but it should be
    }
};

IDispatcherHandler* test_barny(UriDispatcherHandler::Context& ctx)
{
    moducom::pipeline::MemoryChunk chunk =
    ctx.chunk.carve_experimental(0, sizeof(TestBarnyObsever));
    void* p = chunk.data();
    return new (p) TestBarnyObsever(ctx.context);
}


TEST_CASE("experimental tests", "[experimental]")
{
    typedef moducom::pipeline::MemoryChunk chunk_t;
    typedef chunk_t::readonly_t ro_chunk_t;
    SECTION("layer1")
    {
        experimental::layer1::ProcessedMemoryChunk<128> chunk;

        chunk[0] = 1;
        chunk.processed(1);
    }
    SECTION("layer2")
    {
        experimental::layer2::ProcessedMemoryChunk<128> chunk;

        chunk[0] = 1;
        chunk.processed(1);
    }
    SECTION("option traits")
    {
        // it appears pre-C++11 lets us use regular const for this.  That's handy
        CONSTEXPR Option::Numbers option = Option::UriPath;
        CONSTEXPR Option::Numbers option2 = Option::Size1;

        typedef experimental::OptionTrait<option> option_trait_t;
        typedef experimental::OptionTrait<option2> option2_trait_t; // this line compiles, but nothing can really use option2_trait_t

        REQUIRE(Option::String == option_trait_t::format());
        //REQUIRE(Option::UInt == option2_trait_t::format());
    }
    SECTION("ManagedBuffer")
    {
        experimental::v2::ManagedBuffer buffer;

        moducom::pipeline::MemoryChunk c = buffer.current();

        c[0] = 'a';
        c[1] = 'b';
        c[2] = 'c';

        buffer.boundary(1, 3);

        c = buffer.current();

        c[0] = 'A';
        c[1] = 'B';
        c[2] = 'C';
        c[2] = 'D';

        buffer.boundary(1, 4);
        buffer.boundary(2, 0);

        buffer.reset();

        moducom::pipeline::MemoryChunk::readonly_t c_ro = buffer.current_ro(1);

        REQUIRE(c_ro[0] == 'a');
        REQUIRE(c_ro.length() == 3);

        buffer.next();
        c_ro = buffer.current_ro(1);

        REQUIRE(c_ro[0] == 'A');
        REQUIRE(c_ro.length() == 4);

        buffer.reset();

        c_ro = buffer.current_ro(2);

        REQUIRE(c_ro.length() == 7);
    }
    SECTION("More ManagedBuffer")
    {
        experimental::v2::ManagedBuffer buffer;

        chunk_t w = buffer.current();

        w[0] = '1';
        w[1] = '2';

        buffer.boundary(2, 2);

        w = buffer.current();

        w[0] = 'a';
        w[1] = 'b';
        w[2] = 'c';

        buffer.boundary(2, 3);

        buffer.reset();

        ro_chunk_t r = buffer.read(2);

        REQUIRE(r[0] == '1');
        REQUIRE(r[1] == '2');
        REQUIRE(r.length() == 2);

        r = buffer.read(2);

        REQUIRE(r[0] == 'a');
        REQUIRE(r.length() == 3);

    }
    SECTION("FnFactory")
    {
        typedef experimental::FnFactoryTraits<const char*, int> traits_t;
        typedef experimental::FnFactoryHelper<traits_t> fn_t;

        fn_t::item_t items[] =
        {
            fn_t::item("wilma", test_wilma),
            experimental::factory_item_helper("fred", test)
        };

        fn_t::factory_t factory(items);

        fn_t::context_t context;

        int result = factory.create("test", context);

        REQUIRE(result == -1);

        result = factory.create("fred", context);

        REQUIRE(result == 7);

        result = fn_t::create(items, "wilma", context);

        REQUIRE(result == 77);

        moducom::pipeline::MemoryChunk::readonly_t chunk((uint8_t*)"fred", 4);

        result = factory.create(chunk, context);

        REQUIRE(result == 7);
    }
    SECTION("experimental new uri dispatcher")
    {
        typedef UriDispatcherHandler::fn_t fn_t;
        typedef UriDispatcherHandler::item_t item_t;
        moducom::pipeline::layer1::MemoryChunk<512> buffer;
        // FIX: Something is quite wrong with the constructor
        // for _buffer, data() pointer seems to get corrupted
        moducom::pipeline::MemoryChunk _buffer = buffer;
        const void* test = buffer.data();
        const void* test2 = _buffer.data();
        moducom::pipeline::MemoryChunk __buffer((uint8_t*)test, 512);
        const void* test3 = __buffer.data();
        moducom::pipeline::MemoryChunk fake_uri((uint8_t*)"barny", 5);
        IncomingContext incomingContext;

        item_t items[] =
        {
            fn_t::item("barny", test_barny)
        };

        UriDispatcherHandler dh(__buffer, incomingContext, items);

        dh.on_option(Option::UriPath, fake_uri, true);
    }
    SECTION("Map")
    {
        typedef Map<const char*, int, int> map_t;

        map_t::item_t items[] =
        {
            { "fred", 7 },
            { "barny", 77 }
        };
        map_t map(items);

        REQUIRE(map.find("fred") == 7);
        REQUIRE(map.find("barny") == 77);
        REQUIRE(map.find("wilma") == -1);
    }
}
