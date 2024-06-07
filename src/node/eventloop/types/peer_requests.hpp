#pragma once
#include "block/chain/header_chain.hpp"
#include "block/chain/state.hpp"
#include "communication/messages.hpp"
#include "eventloop/types/conref_declaration.hpp"

#include <chrono>
#include <memory>
struct IsRequest {
    static constexpr auto expiry_time = std::chrono::seconds(30);
    bool isActiveRequest = true;
    void unref_active_requests(size_t& activeRequests)
    {
        if (isActiveRequest) {
            assert(activeRequests > 0);
            activeRequests -= 1;
            isActiveRequest = false;
        }
    }
};

struct Proberequest : public ProbereqMsg, public IsRequest {
    bool deprecated = false; // if deprecated expires close connection

    std::shared_ptr<Descripted> descripted;
    Proberequest(std::shared_ptr<Descripted> dsc, NonzeroHeight height)
        : ProbereqMsg(dsc->descriptor, height)
        , descripted(std::move(dsc))
    {
        assert(descripted->chain_length() >= height);
    }
};

struct AwaitInit {
};
struct Blockrequest : public BlockreqMsg, public IsRequest {
    Blockrequest(std::shared_ptr<Descripted> pdescripted,
        BlockRange range,
        Hash upperHash)
        : BlockreqMsg(DescriptedBlockRange { pdescripted->descriptor, range.lower, range.upper })
        , descripted(std::move(pdescripted))
        , upperHash(std::move(upperHash))
    {
    }
    std::shared_ptr<Descripted> descripted;
    Hash upperHash;
};

struct Batchrequest : public BatchreqMsg, public IsRequest {
    uint16_t minReturn = 0;
    uint16_t max_return() { return BatchreqMsg::selector().length; }
    using Pindata = Headerchain::pin_t;
    std::shared_ptr<Descripted> descripted;
    Batch prefix;
    using extra_t = std::variant<Header, Worksum>;
    extra_t extra;

    bool is_partial_request()
    {
        return std::holds_alternative<Worksum>(extra);
    }
    Batchrequest(std::shared_ptr<Descripted> pdescripted,
        const Pindata& pinnedChain,
        NonzeroHeight lower, NonzeroHeight upper, extra_t e)
        : BatchreqMsg { BatchSelector { pdescripted->descriptor, lower, uint16_t(upper - lower + 1) } }
        , minReturn(upper - lower + 1)

        , descripted(std::move(pdescripted))
        , extra(e)
    {
        static_assert(HEADERBATCHSIZE < std::numeric_limits<uint16_t>::max());
        assert(upper >= lower);
        if (is_partial_request())
            assert(upper - lower + 1 < HEADERBATCHSIZE);
        else
            assert(upper - lower + 1 <= HEADERBATCHSIZE);

        assert(pinnedChain.data);
        assert(pinnedChain->length() + 1 >= lower);

        // assign prefix
        Batchslot bs(lower);
        const Batch* b = (*pinnedChain)[bs];
        assert(b);
        auto begin = b->begin();
        auto end = begin + (lower - bs.lower());
        prefix.assign(begin, end);

        bool isUpper
            = Batchslot(upper).upper() == upper;
        assert(isUpper != is_partial_request());
    }

    Batchrequest(std::shared_ptr<Descripted> pdescripted,
        Batchslot slot, uint16_t minElements, Worksum ws)
        : BatchreqMsg { BatchSelector { pdescripted->descriptor, slot.lower(), HEADERBATCHSIZE } }
        , minReturn(minElements)
        , descripted(std::move(pdescripted))
        , extra(ws)
    {
    }
    Batchrequest(std::shared_ptr<Descripted> pdescripted, Batchslot slot, HeaderView h)
        : BatchreqMsg { BatchSelector { pdescripted->descriptor, slot.lower(), HEADERBATCHSIZE } }
        , minReturn(HEADERBATCHSIZE)
        , descripted(std::move(pdescripted))
        , extra(Header(h))
    {
    }
};
using Request = std::variant<Blockrequest, Batchrequest, Proberequest>;
