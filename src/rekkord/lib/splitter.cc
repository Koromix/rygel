// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 Niels Martignène <niels.martignene@protonmail.com>

#include "src/core/base/base.hh"
#include "splitter.hh"

namespace K {

static const Size AverageMin = 256;
static const Size AverageMax = Mebibytes(256);
static const Size MinimumMin = 64;
static const Size MinimumMax = Mebibytes(64);
static const Size MaximumMin = 1024;
static const Size MaximumMax = Mebibytes(1024);

static const uint32_t GearTable[] = {
    0x5C95C078u, 0x22408989u, 0x2D48A214u, 0x12842087u, 0x530F8AFBu, 0x474536B9u,
    0x2963B4F1u, 0x44CB738Bu, 0x4EA7403Du, 0x4D606B6Eu, 0x074EC5D3u, 0x3AF39D18u,
    0x726003CAu, 0x37A62A74u, 0x51A2F58Eu, 0x7506358Eu, 0x5D4AB128u, 0x4D4AE17Bu,
    0x41E85924u, 0x470C36F7u, 0x4741CBE1u, 0x01BB7F30u, 0x617C1DE3u, 0x2B0C3A1Fu,
    0x50C48F73u, 0x21A82D37u, 0x6095ACE0u, 0x419167A0u, 0x3CAF49B0u, 0x40CEA62Du,
    0x66BC1C66u, 0x545E1DADu, 0x2BFA77CDu, 0x6E85DA24u, 0x5FB0BDC5u, 0x652CFC29u,
    0x3A0AE1ABu, 0x2837E0F3u, 0x6387B70Eu, 0x13176012u, 0x4362C2BBu, 0x66D8F4B1u,
    0x37FCE834u, 0x2C9CD386u, 0x21144296u, 0x627268A8u, 0x650DF537u, 0x2805D579u,
    0x3B21EBBDu, 0x7357ED34u, 0x3F58B583u, 0x7150DDCAu, 0x7362225Eu, 0x620A6070u,
    0x2C5EF529u, 0x7B522466u, 0x768B78C0u, 0x4B54E51Eu, 0x75FA07E5u, 0x06A35FC6u,
    0x30B71024u, 0x1C8626E1u, 0x296AD578u, 0x28D7BE2Eu, 0x1490A05Au, 0x7CEE43BDu,
    0x698B56E3u, 0x09DC0126u, 0x4ED6DF6Eu, 0x02C1BFC7u, 0x2A59AD53u, 0x29C0E434u,
    0x7D6C5278u, 0x507940A7u, 0x5EF6BA93u, 0x68B6AF1Eu, 0x46537276u, 0x611BC766u,
    0x155C587Du, 0x301BA847u, 0x2CC9DDA7u, 0x0A438E2Cu, 0x0A69D514u, 0x744C72D3u,
    0x4F326B9Bu, 0x7EF34286u, 0x4A0EF8A7u, 0x6AE06EBEu, 0x669C5372u, 0x12402DCBu,
    0x5FEAE99Du, 0x76C7F4A7u, 0x6ABDB79Cu, 0x0DFAA038u, 0x20E2282Cu, 0x730ED48Bu,
    0x069DAC2Fu, 0x168ECF3Eu, 0x2610E61Fu, 0x2C512C8Eu, 0x15FB8C06u, 0x5E62BC76u,
    0x69555135u, 0x0ADB864Cu, 0x4268F914u, 0x349AB3AAu, 0x20EDFDB2u, 0x51727981u,
    0x37B4B3D8u, 0x5DD17522u, 0x6B2CBFE4u, 0x5C47CF9Fu, 0x30FA1CCDu, 0x23DEDB56u,
    0x13D1F50Au, 0x64EDDEE7u, 0x0820B0F7u, 0x46E07308u, 0x1E2D1DFDu, 0x17B06C32u,
    0x250036D8u, 0x284DBF34u, 0x68292EE0u, 0x362EC87Cu, 0x087CB1EBu, 0x76B46720u,
    0x104130DBu, 0x71966387u, 0x482DC43Fu, 0x2388EF25u, 0x524144E1u, 0x44BD834Eu,
    0x448E7DA3u, 0x3FA6EAF9u, 0x3CDA215Cu, 0x3A500CF3u, 0x395CB432u, 0x5195129Fu,
    0x43945F87u, 0x51862CA4u, 0x56EA8FF1u, 0x201034DCu, 0x4D328FF5u, 0x7D73A909u,
    0x6234D379u, 0x64CFBF9Cu, 0x36F6589Au, 0x0A2CE98Au, 0x5FE4D971u, 0x03BC15C5u,
    0x44021D33u, 0x16C1932Bu, 0x37503614u, 0x1ACAF69Du, 0x3F03B779u, 0x49E61A03u,
    0x1F52D7EAu, 0x1C6DDD5Cu, 0x062218CEu, 0x07E7A11Au, 0x1905757Au, 0x7CE00A53u,
    0x49F44F29u, 0x4BCC70B5u, 0x39FEEA55u, 0x5242CEE8u, 0x3CE56B85u, 0x00B81672u,
    0x46BEECCCu, 0x3CA0AD56u, 0x2396CEE8u, 0x78547F40u, 0x6B08089Bu, 0x66A56751u,
    0x781E7E46u, 0x1E2CF856u, 0x3BC13591u, 0x494A4202u, 0x520494D7u, 0x2D87459Au,
    0x757555B6u, 0x42284CC1u, 0x1F478507u, 0x75C95DFFu, 0x35FF8DD7u, 0x4E4757EDu,
    0x2E11F88Cu, 0x5E1B5048u, 0x420E6699u, 0x226B0695u, 0x4D1679B4u, 0x5A22646Fu,
    0x161D1131u, 0x125C68D9u, 0x1313E32Eu, 0x4AA85724u, 0x21DC7EC1u, 0x4FFA29FEu,
    0x72968382u, 0x1CA8EEF3u, 0x3F3B1C28u, 0x39C2FB6Cu, 0x6D76493Fu, 0x7A22A62Eu,
    0x789B1C2Au, 0x16E0CB53u, 0x7DECEEEBu, 0x0DC7E1C6u, 0x5C75BF3Du, 0x52218333u,
    0x106DE4D6u, 0x7DC64422u, 0x65590FF4u, 0x2C02EC30u, 0x64A9AC67u, 0x59CAB2E9u,
    0x4A21D2F3u, 0x0F616E57u, 0x23B54EE8u, 0x02730AAAu, 0x2F3C634Du, 0x7117FC6Cu,
    0x01AC6F05u, 0x5A9ED20Cu, 0x158C4E2Au, 0x42B699F0u, 0x0C7C14B3u, 0x02BD9641u,
    0x15AD56FCu, 0x1C722F60u, 0x7DA1AF91u, 0x23E0DBCBu, 0x0E93E12Bu, 0x64B2791Du,
    0x440D2476u, 0x588EA8DDu, 0x4665A658u, 0x7446C418u, 0x1877A774u, 0x5626407Eu,
    0x7F63BD46u, 0x32D2DBD8u, 0x3C790F4Au, 0x772B7239u, 0x6F8B2826u, 0x677FF609u,
    0x0DC82C11u, 0x23FFE354u, 0x2EAC53A6u, 0x16139E09u, 0x0AFD0DBCu, 0x2A4D4237u,
    0x56A368C7u, 0x234325E4u, 0x2DCE9187u, 0x32E8EA7Eu
};
static_assert(K_LEN(GearTable) == 256);

template <typename T>
T CeilingDivide(T x, T y)
{
    T ret = (x % y) ? ((x / y) + 1) : (x / y);
    return ret;
}

static Size CenterSize(Size avg, Size min, Size max)
{
    Size offset = min + CeilingDivide(min, (Size)2);
    offset = std::min(offset, avg);

    Size size = avg - offset;
    size = std::min(size, max);

    return size;
}

FastSplitter::FastSplitter(Size avg, Size min, Size max, uint64_t salt8)
    : avg(avg), min(min), max(max)
{
    K_ASSERT(AverageMin <= avg && avg <= AverageMax);
    K_ASSERT(MinimumMin <= min && min <= MinimumMax);
    K_ASSERT(MaximumMin <= max && max <= MaximumMax);
    K_ASSERT(min < avg && avg < max);

    // Init masks
    {
        int bits = (int)round(log2((double)avg));

        mask1 = (1u << (bits + 1)) - 1;
        mask2 = (1u << (bits - 1)) - 1;
    }

    // Init salt
    {
        union {
            uint64_t salt8;
            uint8_t salt[8];
        } u;
        static_assert(K_SIZE(salt) == K_SIZE(u.salt));
        static_assert(K_SIZE(u.salt) == K_SIZE(salt8));

        u.salt8 = LittleEndian(salt8);
        MemCpy(salt, u.salt, K_SIZE(salt));
    }
}

Size FastSplitter::Process(Span<const uint8_t> buf, bool last,
                           FunctionRef<bool(Size idx, int64_t total, Span<const uint8_t> chunk)> func)
{
    Size processed = 0;

    while (buf.len) {
        Size cut = Cut(buf, last);
        if (!cut)
            break;

        Span<const uint8_t> chunk = buf.Take(0, cut);

        if (!func(idx++, total, chunk))
            return -1;

        buf.ptr += cut;
        buf.len -= cut;
        processed += cut;
        total += cut;
    }

    return processed;
}

Size FastSplitter::Cut(Span<const uint8_t> buf, bool last)
{
    if (buf.len <= min)
        return last ? buf.len : 0;
    buf.len = std::min(buf.len, max);

    Size offset = min;
    Size limit = CenterSize(avg, min, buf.len);
    uint32_t hash = 0;
    size_t byte = (size_t)total % 8;

    while (offset < limit) {
        hash = (hash >> 1) + GearTable[buf.ptr[offset++] ^ salt[byte++ % 8]];
        if (!(hash & mask1))
            return offset;
    }
    while (offset < buf.len) {
        hash = (hash >> 1) + GearTable[buf.ptr[offset++] ^ salt[byte++ % 8]];
        if (!(hash & mask2))
            return offset;
    }

    if (!last && buf.len < max)
        return 0;
    return buf.len;
}

}
