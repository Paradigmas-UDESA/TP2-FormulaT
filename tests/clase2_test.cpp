// Clase 2: templates, refactoring clase 1's binary serialization. One
// template writes the bytes of anything trivially copyable (numbers, enums,
// durations, structs of those); one more writes any vector; CarResult gets
// its own overload. Everything written must come back equal
// (doubles bit for bit). Clase 1's tests must keep passing: same bytes.

#include <gtest/gtest.h>

#include <chrono>
#include <cmath>
#include <cstring>
#include <limits>
#include <sstream>
#include <vector>

#include "formulat/binary.h"
#include "formulat/qualifying.h"

using namespace formulat;
using namespace std::chrono_literals;

namespace {

template <typename T>
T roundTrip(const T& value)
{
    std::stringstream ss(std::ios::in | std::ios::out | std::ios::binary);
    writeBinary(ss, value);
    T back{};
    readBinary(ss, back);
    return back;
}

bool sameBits(double x, double y)
{
    return std::memcmp(&x, &y, sizeof x) == 0;
}

void expectEqual(const SectionResult& x, const SectionResult& y)
{
    EXPECT_EQ(x.outcome, y.outcome);
    EXPECT_EQ(x.pitStop, y.pitStop);
    EXPECT_EQ(x.time, y.time);
    EXPECT_TRUE(sameBits(x.residual, y.residual));
}

void expectEqual(const CarResult& x, const CarResult& y)
{
    EXPECT_EQ(x.team, y.team);
    EXPECT_EQ(x.driver, y.driver);
    EXPECT_EQ(x.finished, y.finished);
    EXPECT_EQ(x.total, y.total);
    ASSERT_EQ(x.sections.size(), y.sections.size());
    for (std::size_t i = 0; i < x.sections.size(); ++i)
        expectEqual(x.sections[i], y.sections[i]);
}

}  // namespace

// --- the generic template ------------------------------------------------------------

TEST(RawTest, DoublesComeBackBitForBit)
{
    for (double d : {0.1, 1.0 / 3, -0.0, std::numeric_limits<double>::denorm_min(),
                     std::numeric_limits<double>::quiet_NaN(), double(INFINITY),
                     std::numeric_limits<double>::max()}) {
        SCOPED_TRACE(d);
        EXPECT_TRUE(sameBits(roundTrip(d), d));
    }
}

TEST(RawTest, IntegersAndBools)
{
    EXPECT_EQ(roundTrip(-42), -42);
    EXPECT_EQ(roundTrip(std::size_t{1} << 40), std::size_t{1} << 40);
    EXPECT_EQ(roundTrip(true), true);
    EXPECT_EQ(roundTrip(false), false);
}

TEST(RawTest, WritesExactlySizeofBytes)
{
    std::stringstream ss(std::ios::in | std::ios::out | std::ios::binary);
    writeBinary(ss, 3.0);
    writeBinary(ss, 7);
    EXPECT_EQ(ss.str().size(), sizeof(double) + sizeof(int));
}

// Enums and durations are trivially copyable: the SAME template handles them,
// no extra code (compare with the name tables in the given serialize.cpp).
TEST(RawTest, EnumsAndDurationsNeedNoExtraCode)
{
    for (auto t : {Team::Bisection, Team::Toms748, Team::Newton, Team::Halley, Team::Secant,
                   Team::RegulaFalsi})
        EXPECT_EQ(roundTrip(t), t);
    EXPECT_EQ(roundTrip(Driver::Warm), Driver::Warm);
    EXPECT_EQ(roundTrip(Outcome::Penalty), Outcome::Penalty);
    EXPECT_EQ(roundTrip(SectionType::Kerb), SectionType::Kerb);
    EXPECT_EQ(roundTrip(std::chrono::nanoseconds{1234567890123}), std::chrono::nanoseconds{1234567890123});
}

TEST(RawTest, EndOfInputThrows)
{
    std::stringstream ss(std::ios::in | std::ios::out | std::ios::binary);
    ss.write("abc", 3);  // fewer bytes than a double
    double d;
    EXPECT_THROW(readBinary(ss, d), std::runtime_error);
}

// --- the vector template: any element type -------------------------------------------

TEST(BinaryVectorTest, OfNumbers)
{
    const std::vector<int> v{3, -1, 4, 1, -5};
    EXPECT_EQ(roundTrip(v), v);
    EXPECT_TRUE(roundTrip(std::vector<double>{}).empty());
}

TEST(BinaryVectorTest, Nested)
{
    const std::vector<std::vector<double>> v{{1.5, -0.0}, {}, {0.1, 0.2, 0.3}};
    const auto back = roundTrip(v);
    ASSERT_EQ(back.size(), 3u);
    EXPECT_TRUE(sameBits(back[0][1], -0.0));
    EXPECT_TRUE(back[1].empty());
    EXPECT_EQ(back[2], v[2]);
}

// --- structs: SectionResult through the generic template, CarResult through its own ----

TEST(BinaryStructTest, SectionResult)
{
    const SectionResult r{Outcome::DNF, true, 12345ns, NAN};
    expectEqual(roundTrip(r), r);
}

TEST(BinaryStructTest, CarResultWithItsSections)
{
    const CarResult c{Team::Secant, Driver::Warm, false, 999ns,
                      {{Outcome::Clean, false, 1ns, 0.0}, {Outcome::DNF, false, 2ns, INFINITY}}};
    expectEqual(roundTrip(c), c);
}
