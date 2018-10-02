#include <iostream>

#include <estd/sstream.h>

#include <platform/posix/sockets-datapump.h>

#include <coap/encoder.h>
#include <coap/decoder.h>

#include <exp/uripath-repeater.h>

using namespace moducom::coap;

constexpr int id_test1 = 2;
constexpr int id_test2 = 3;
constexpr int id_wellknown_core = 5;

static experimental::UriPathMap paths[] =
{
    { "v1",     0,          -1 },
    { "api",    1,          0 },
    { "test1",  id_test1,   1 },
    { "test2",  id_test2,   1 },

    { ".well-known",    4,      -1 },
    { "core",           id_wellknown_core,      4 }
};

static experimental::CoREData coredata[] =
{
    { id_test1, "test1_resource", "test_interface" },
    { id_test2, "test2_resource", "test_interface" }
};


template <class TOStream>
static bool extra_link_attribute_handler(experimental::UriPathMap& path, TOStream& out)
{
    switch(path.second)
    {
        case id_test1:
            out << ";title=\"test title 1\"";
            return true;

        default:
            return false;
    }
}

int main()
{
    std::cout << "CoRE test" << std::endl;

    //const auto& _paths = estd::layer3::make_array(paths);

    SocketsDatapumpHelper sdh;

    sockets_datapump_t datapump;

    for(;;)
    {
        sdh.loop(datapump);

        if(!datapump.dequeue_empty())
        {
            typedef sockets_datapump_t datapump_t;
            typedef typename datapump_t::netbuf_t netbuf_t;
            typedef typename datapump_t::addr_t addr_t;
            typedef typename datapump_t::Item item_t;
            typedef NetBufDecoder<netbuf_t&> decoder_t;

            item_t& item = datapump.dequeue_front();

            // hold on to ipaddr for when we enqueue a response
            addr_t ipaddr = item.addr();

            decoder_t decoder(*item.netbuf());

            Header header_in = decoder.header();

            layer2::Token token;

            // populate token, if present.  Expects decoder to be at HeaderDone phase
            decoder.token(&token);

            // skip any options
            option_iterator<decoder_t> it(decoder, true);
            estd::layer1::optional<int, -1> found_id;

            for(;it.valid();++it)
            {
                // FIX: This feels a little clumsy, only because
                // option number itself is implicitly-available but value is explicit
                // (how would that be obvious to anyone?)
                Option::Numbers number = it;

                if(number == Option::UriPath)
                {
                    estd::layer3::const_string s = it.string();

                    const experimental::UriPathMap* path = experimental::match(
                                paths, sizeof(paths) / sizeof(paths[0]),
                                s, *found_id);

                    // One bad path and we're back to not found again
                    found_id = path != NULLPTR ? path->second : -1;
                }
                else if(number == Option::UriQuery)
                {
                    // Just for debug purposes to inspect incoming UriQuery - we
                    // don't do anything with it yet
                    estd::layer3::const_string s = it.string();
                }
            }

#ifdef FEATURE_EMBR_DATAPUMP_INLINE
            NetBufEncoder<netbuf_t> encoder;
#else
            // FIX: Need a much more cohesive way of doing this
            delete &decoder.netbuf();

            NetBufEncoder<netbuf_t&> encoder(* new netbuf_t);
#endif

            datapump.dequeue_pop();

            Header::Code::Codes code = found_id ? Header::Code::Content : Header::Code::NotFound;

            encoder.header(create_response(header_in, code));
            encoder.token(token);
            encoder.payload_header();

            // if in fact uripath == /.well-known/core
            if(found_id == id_wellknown_core)
            {
                // setup CoRE evaluator which only finds paths who actually
                // are associated with CoRE data
                experimental::core_evaluator ce(coredata);

                // sorta-wrap-up netbuf with a streambuf (span won't be able to advance or
                // next a netbuf though)
                estd::internal::streambuf<estd::internal::impl::out_span_streambuf<uint8_t> >
                        sb(encoder.netbuf().unprocessed(), encoder.netbuf().length_unprocessed());
                estd::internal::basic_ostream<decltype (sb)&> out(sb);

                // FIX: current architecture can't reasonably resolve precisely how to spit out commas
                // (because we don't know if another output is coming until after we run ce.evaluate
                //  at which point it's too late)
                for(auto& path : paths)
                {
                    bool is_core = ce.evaluate(experimental::known_uri_event(path), out);

                    // NOTE: 'paths' treated as canonical source for what CoRE paths are
                    // revealed, but doesn't have to be
                    if(is_core)
                        extra_link_attribute_handler(path, out);

                    if(is_core)
                        out << ',';
                }

                // sb.pos is not normally exposed.  artifact of the span streambuf
                // FIX: we yank off the last one since we always have one extra comma
                encoder.advance(sb.pos - 1);
            }

            encoder.complete();

#ifdef FEATURE_EMBR_DATAPUMP_INLINE
            datapump.enqueue_out(std::move(encoder.netbuf()), ipaddr);
#else
            datapump.enqueue_out(encoder.netbuf(), ipaddr);
#endif
        }
    }

    return 0;
}
