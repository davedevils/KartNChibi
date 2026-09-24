/// the one 0x00C0 body writer see docs packets opcodes 0x00C0 for the wire layout

#include "packets/KartDefinitionWire.h"

namespace knc {

void writeKartDefinitionBody(Packet& pkt, const KartDefinitionFields& f) {
    pkt.writeUInt32(f.visibleFlag);
    pkt.writeUInt32(f.badge);
    pkt.writeUInt32(f.kartKey);
    pkt.writeUInt8(f.unk0c);        // one byte on the wire not four
    pkt.writeInt32(f.vehicleKind);
    pkt.writeInt32(f.modelScheme);
    pkt.writeInt32(f.unk18);
    pkt.writeInt32(f.requiredLevel);

    pkt.writeString(f.modelName);
    pkt.writeString(f.displayNameKey);
    pkt.writeString(f.descriptionKey);

    pkt.writeBytes(f.defaultSkinKeys.data(), f.defaultSkinKeys.size());
    for (size_t i = 0; i < f.stats.size(); ++i) pkt.writeFloat(f.stats[i]);
    // the id is signed on the client -1 hides the pair 0 draws an icon with 0%
    pkt.writeInt32(f.abilityPair0.id);
    pkt.writeUInt32(f.abilityPair0.percent);
    pkt.writeInt32(f.abilityPair1.id);
    pkt.writeUInt32(f.abilityPair1.percent);
}

}  // namespace knc
