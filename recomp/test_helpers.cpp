#include <cstdint>
#include <cstdio>
#include <climits>
#include <cmath>
#define SIMDE_ENABLE_NATIVE_ALIASES
#include <simde/x86/sse4.1.h>

// Exact copies of the helpers added by the historical XenonRecomp patch.
inline simde__m128i simde_mm_vctuxs(simde__m128 src1)
{
    simde__m128 big = simde_mm_set1_ps(2147483648.0f);
    simde__m128 clamped = simde_mm_max_ps(src1, simde_mm_setzero_ps());
    simde__m128 overflow = simde_mm_cmpge_ps(clamped, simde_mm_set1_ps(4294967296.0f));
    simde__m128 isbig = simde_mm_cmpge_ps(clamped, big);
    simde__m128i small_cvt = simde_mm_cvttps_epi32(clamped);
    simde__m128i big_cvt = simde_mm_xor_si128(simde_mm_cvttps_epi32(simde_mm_sub_ps(clamped, big)), simde_mm_set1_epi32(INT_MIN));
    simde__m128i res = simde_mm_blendv_epi8(small_cvt, big_cvt, simde_mm_castps_si128(isbig));
    return simde_mm_or_si128(res, simde_mm_castps_si128(overflow));
}
inline simde__m128i simde_mm_vslo(simde__m128i a, simde__m128i b)
{
    union { simde__m128i v; uint8_t u8[16]; } src, dst;
    src.v = a;
    uint8_t shb = (uint8_t(simde_mm_extract_epi8(b, 0)) >> 3) & 0xF;
    for (int i = 0; i < 16; i++)
        dst.u8[i] = (i >= shb) ? src.u8[i - shb] : 0;
    return dst.v;
}

int fails = 0;
#define CHECK(cond, msg) do { if(!(cond)) { printf("FAIL: %s\n", msg); fails++; } } while(0)

int main() {
    // vctuxs
    float in[4] = { -1.0f, 3.7f, 3000000000.0f, 5000000000.0f };
    uint32_t out[4];
    simde_mm_storeu_si128((simde__m128i*)out, simde_mm_vctuxs(simde_mm_loadu_ps(in)));
    CHECK(out[0] == 0, "vctuxs(-1) must saturate to 0");
    CHECK(out[1] == 3, "vctuxs(3.7) must truncate to 3");
    CHECK(out[2] == 3000000000u, "vctuxs(3e9) must preserve a value above 2^31");
    CHECK(out[3] == 0xFFFFFFFFu, "vctuxs(5e9) must saturate to 0xFFFFFFFF");
    float nanv[4] = { NAN, 0.5f, 4294967296.0f, 2147483648.0f };
    simde_mm_storeu_si128((simde__m128i*)out, simde_mm_vctuxs(simde_mm_loadu_ps(nanv)));
    CHECK(out[0] == 0, "vctuxs(NaN) must produce 0");
    CHECK(out[1] == 0, "vctuxs(0.5) must truncate to 0");
    CHECK(out[2] == 0xFFFFFFFFu, "vctuxs(2^32) must saturate");
    CHECK(out[3] == 2147483648u, "vctuxs(2^31) must be exact");

    // vslo: a = 0..15, shift by 3 bytes.
    uint8_t av[16], bv[16] = {0};
    for (int i = 0; i < 16; i++) av[i] = i + 1;
    bv[0] = 3 << 3;
    uint8_t rv[16];
    simde_mm_storeu_si128((simde__m128i*)rv, simde_mm_vslo(simde_mm_loadu_si128((simde__m128i*)av), simde_mm_loadu_si128((simde__m128i*)bv)));
    for (int i = 0; i < 3; i++) CHECK(rv[i] == 0, "vslo low bytes must be zero");
    for (int i = 3; i < 16; i++) CHECK(rv[i] == av[i-3], "vslo shift must be correct");
    // A zero shift is the identity.
    bv[0] = 0;
    simde_mm_storeu_si128((simde__m128i*)rv, simde_mm_vslo(simde_mm_loadu_si128((simde__m128i*)av), simde_mm_loadu_si128((simde__m128i*)bv)));
    for (int i = 0; i < 16; i++) CHECK(rv[i] == av[i], "vslo shift 0 must be identity");

    // vrlh emitted logic: rotation by zero must not be undefined behavior.
    uint16_t x = 0xABCD; int sh = 0;
    uint16_t rot = (uint16_t)((x << (sh & 0xF)) | (x >> ((16 - (sh & 0xF)) & 0xF)));
    CHECK(rot == x, "vrlh shift 0 must be identity");
    sh = 4; rot = (uint16_t)((x << (sh & 0xF)) | (x >> ((16 - (sh & 0xF)) & 0xF)));
    CHECK(rot == 0xBCDA, "vrlh rotate 4");

    printf(fails ? "%d FAILURES\n" : "ALL TESTS PASSED\n", fails);
    return fails;
}
