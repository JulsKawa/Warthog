#include "communication/create_payment.hpp"
#include "crypto/hasher_sha256.hpp"
#include "mempool/entry.hpp"
#include "general/writer.hpp"
#include "parse.hpp"


Writer& operator<<(Writer& w, TransferTxExchangeMessage m)
{
    return w << m.txid
             << m.reserved
             << m.compactFee
             << m.toAddr
             << m.amount
             << m.signature;
}

Address TransferTxExchangeMessage::from_address(HashView txHash) const
{
    return signature.recover_pubkey(txHash.data()).address();
}

TxHash TransferTxExchangeMessage::txhash(HashView pinHash) const
{
    return TxHash(
        HasherSHA256()
        << pinHash
        << pin_height()
        << txid.nonceId
        << reserved
        << compactFee.uncompact()
        << toAddr
        << amount);
}

TransferTxExchangeMessage::TransferTxExchangeMessage(TransferView t, PinHeight ph, AddressView toAddr)
    : txid(t.txid(ph))
    , reserved(t.pin_nonce().reserved)
    , compactFee(t.compact_fee_trow())
    , toAddr(toAddr)
    , amount(t.amount_throw())
    , signature(t.signature()) {}

TransferTxExchangeMessage::TransferTxExchangeMessage(AccountId fromId, const PaymentCreateMessage& pcm)
    : txid(fromId, pcm.pinHeight, pcm.nonceId)
    , reserved(pcm.reserved)
    , compactFee(pcm.compactFee)
    , toAddr(pcm.toAddr)
    , amount(pcm.amount)
    , signature(pcm.signature) {}

TransferTxExchangeMessage::TransferTxExchangeMessage(const TransactionId& txid, const mempool::EntryValue& v)
    : txid(txid)
    , reserved(v.noncep2)
    , compactFee(v.fee)
    , toAddr(v.toAddr)
    , amount(v.amount)
    , signature(v.signature)
{
}

TransferTxExchangeMessage::TransferTxExchangeMessage(ReaderCheck<bytesize> r)
    : txid(r.r)
    , reserved(r.r.view<3>())
    , compactFee(CompactUInt::from_value_throw(r.r.uint16()))
    , toAddr(r.r.view<AddressView>())
    , amount(Funds::from_value_throw(r.r.uint64()))
    , signature(r.r.view<65>())
{
    r.assert_read_bytes();
}

