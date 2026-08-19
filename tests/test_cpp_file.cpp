#include "wabridge_file.h"
#include "wabridge_envelope.h"

#include <cassert>
#include <iostream>

using wabridge::file::Chunk;
using wabridge::file::Offer;
using wabridge::file::decode_chunk;
using wabridge::file::decode_offer;
using wabridge::file::encode_chunk;
using wabridge::file::encode_offer;
using wabridge::file::safe_display_name;
using wabridge::protocol::ProtocolError;

int main() {
    Offer offer;
    offer.transfer_id.fill(0x11);
    offer.size = 12345;
    offer.sha256.fill(0x22);
    offer.display_name = "../photo.jpg";
    offer.mime_type = "image/jpeg";
    const auto decoded_offer = decode_offer(encode_offer(offer));
    assert(decoded_offer.size == offer.size);
    assert(decoded_offer.display_name == "_.._photo.jpg");
    assert(decoded_offer.mime_type == offer.mime_type);

    Chunk chunk;
    chunk.transfer_id = offer.transfer_id;
    chunk.offset = 4096;
    chunk.data.assign(512, 0xAB);
    const auto decoded_chunk = decode_chunk(encode_chunk(chunk));
    assert(decoded_chunk.transfer_id == chunk.transfer_id);
    assert(decoded_chunk.offset == chunk.offset);
    assert(decoded_chunk.data == chunk.data);

    assert(safe_display_name("report.txt") == "report.txt");
    assert(safe_display_name("a:b\\c/d") == "a_b_c_d");

    auto malformed = encode_chunk(chunk);
    malformed[24] = 0xFF;
    bool rejected = false;
    try {
        (void)decode_chunk(malformed);
    } catch (const ProtocolError&) {
        rejected = true;
    }
    assert(rejected);

    chunk.data.assign(1024 * 1024 + 1, 0);
    rejected = false;
    try {
        (void)encode_chunk(chunk);
    } catch (const ProtocolError&) {
        rejected = true;
    }
    assert(rejected);

    std::cout << "File-transfer protocol tests passed\n";
    return 0;
}
