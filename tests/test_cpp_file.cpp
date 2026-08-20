#include "test_require.h"
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
    REQUIRE(decoded_offer.size == offer.size);
    REQUIRE(decoded_offer.display_name == ".._photo.jpg");
    REQUIRE(decoded_offer.mime_type == offer.mime_type);

    Chunk chunk;
    chunk.transfer_id = offer.transfer_id;
    chunk.offset = 4096;
    chunk.data.assign(512, 0xAB);
    const auto decoded_chunk = decode_chunk(encode_chunk(chunk));
    REQUIRE(decoded_chunk.transfer_id == chunk.transfer_id);
    REQUIRE(decoded_chunk.offset == chunk.offset);
    REQUIRE(decoded_chunk.data == chunk.data);

    REQUIRE(safe_display_name("report.txt") == "report.txt");
    REQUIRE(safe_display_name("a:b\\c/d") == "a_b_c_d");

    auto malformed = encode_chunk(chunk);
    malformed[24] = 0xFF;
    bool rejected = false;
    try {
        (void)decode_chunk(malformed);
    } catch (const ProtocolError&) {
        rejected = true;
    }
    REQUIRE(rejected);

    chunk.data.assign(1024 * 1024 + 1, 0);
    rejected = false;
    try {
        (void)encode_chunk(chunk);
    } catch (const ProtocolError&) {
        rejected = true;
    }
    REQUIRE(rejected);

    std::cout << "File-transfer protocol tests passed\n";
    return 0;
}
