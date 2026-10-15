// Clase 1: binary serialization, by hand. CarResult::serialize/deserialize
// and the qualifying file. Everything written must come back equal (doubles bit
// for bit), the bytes must follow the layout in qualifying.h and binary.h, and
// every broken file must throw std::runtime_error.

#include <gtest/gtest.h>

#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <limits>
#include <sstream>
#include <string>
#include <vector>

#include "formulat/binary.h"
#include "formulat/qualifying.h"
#include "formulat/serialize.h"
#include "formulat/track.h"

using namespace formulat;
using namespace std::chrono_literals;

namespace {

std::string bytesOf(const CarResult& car)
{
    std::stringstream ss(std::ios::in | std::ios::out | std::ios::binary);
    car.serialize(ss);
    return ss.str();
}

CarResult fromBytes(const std::string& bytes)
{
    std::stringstream ss(bytes, std::ios::in | std::ios::out | std::ios::binary);
    CarResult car{};
    car.deserialize(ss);
    return car;
}

// The raw bytes of x, appended: how the expected layout is spelled out below.
template <typename T>
void append(std::string& bytes, const T& x)
{
    bytes.append(reinterpret_cast<const char*>(&x), sizeof x);
}

// Same bits: tells -0.0 from 0.0, and any NaN equals itself.
bool sameBits(double x, double y)
{
    return std::memcmp(&x, &y, sizeof x) == 0;
}

void expectEqual(const CarResult& x, const CarResult& y)
{
    EXPECT_EQ(x.team, y.team);
    EXPECT_EQ(x.driver, y.driver);
    EXPECT_EQ(x.finished, y.finished);
    EXPECT_EQ(x.total, y.total);
    ASSERT_EQ(x.sections.size(), y.sections.size());
    for (std::size_t i = 0; i < x.sections.size(); ++i) {
        EXPECT_EQ(x.sections[i].outcome, y.sections[i].outcome);
        EXPECT_EQ(x.sections[i].pitStop, y.sections[i].pitStop);
        EXPECT_EQ(x.sections[i].time, y.sections[i].time);
        EXPECT_TRUE(sameBits(x.sections[i].residual, y.sections[i].residual));
    }
}

// A cold Newton that crashed on its third section (NaN residual).
CarResult crashedNewton()
{
    return {Team::Newton, Driver::Cold, false, 3456789ns,
            {{Outcome::Clean, 0, 1234567ns, 3e-13},
             {Outcome::Penalty, 1, 2222222ns, -0.0},
             {Outcome::DNF, 0, 0ns, std::numeric_limits<double>::quiet_NaN()}}};
}

std::string tempPath(const std::string& name)
{
    return (std::filesystem::temp_directory_path() / ("formulat-test-" + name)).string();
}

std::string slurp(const std::string& path)
{
    std::ifstream is(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(is), std::istreambuf_iterator<char>()};
}

void spit(const std::string& path, const std::string& content)
{
    std::ofstream(path, std::ios::binary) << content;
}

}  // namespace

// --- one car ---------------------------------------------------------------------------

TEST(CarResultTest, ComesBackEqual)
{
    const CarResult car = crashedNewton();
    expectEqual(fromBytes(bytesOf(car)), car);
}

TEST(CarResultTest, WithoutSections)
{
    const CarResult car{Team::Halley, Driver::Warm, true, 0ns, {}};
    expectEqual(fromBytes(bytesOf(car)), car);
}

TEST(CarResultTest, BytesFollowTheLayout)
{
    // team, driver, finished, total, the number of sections, each section whole.
    // The vector's elements are written, not the vector itself: writing the
    // vector would write a pointer, which means nothing in another run.
    const CarResult car = crashedNewton();
    std::string expected;
    append(expected, car.team);
    append(expected, car.driver);
    append(expected, car.finished);
    append(expected, car.total);
    append(expected, car.sections.size());
    for (const SectionResult& s : car.sections)
        append(expected, s);
    EXPECT_EQ(bytesOf(car), expected);
}

TEST(CarResultTest, DeserializeReplacesWhatWasThere)
{
    const CarResult small{Team::Bisection, Driver::Cold, true, 5ns, {{Outcome::Clean, 0, 5ns, 0.0}}};
    std::stringstream ss(std::ios::in | std::ios::out | std::ios::binary);
    small.serialize(ss);
    CarResult car = crashedNewton();  // three sections before reading one
    car.deserialize(ss);
    expectEqual(car, small);
}

TEST(CarResultTest, ShortInputThrows)
{
    const std::string bytes = bytesOf(crashedNewton());
    for (std::size_t n : {std::size_t{0}, std::size_t{6}, bytes.size() / 2, bytes.size() - 1}) {
        SCOPED_TRACE(n);
        EXPECT_THROW(fromBytes(bytes.substr(0, n)), std::runtime_error);
    }
}

// --- the file ----------------------------------------------------------------------------

TEST(QualifyingFileTest, RoundTrip)
{
    const auto cars = classify(qualify(generate(defaultTrack())).cars);
    const std::string path = tempPath("qualifying.bin");
    saveQualifyingBinary(path, cars);
    const auto back = loadQualifyingBinary(path);
    ASSERT_EQ(back.size(), cars.size());
    for (std::size_t i = 0; i < cars.size(); ++i)
        expectEqual(back[i], cars[i]);
}

TEST(QualifyingFileTest, BytesFollowTheLayout)
{
    // "FTQB", version 1 (a uint32), the number of cars (a size_t), then each car.
    const std::vector<CarResult> cars{crashedNewton(), {Team::Halley, Driver::Warm, true, 7ns, {}}};
    const std::string path = tempPath("layout.bin");
    saveQualifyingBinary(path, cars);
    std::string expected = "FTQB";
    append(expected, std::uint32_t{1});
    append(expected, cars.size());
    for (const CarResult& car : cars)
        expected += bytesOf(car);
    EXPECT_EQ(slurp(path), expected);
}

TEST(BrokenFileTest, SavingWhereItCantWriteThrows)
{
    EXPECT_THROW(saveQualifyingBinary(tempPath("no-such-dir/qualifying.bin"), {}),
                 std::runtime_error);
}

TEST(BrokenFileTest, MissingFileThrows)
{
    EXPECT_THROW(loadQualifyingBinary(tempPath("does-not-exist.bin")), std::runtime_error);
}

TEST(BrokenFileTest, EmptyFileThrows)
{
    const std::string path = tempPath("empty.bin");
    spit(path, "");
    EXPECT_THROW(loadQualifyingBinary(path), std::runtime_error);
}

TEST(BrokenFileTest, OtherKindThrows)
{
    const std::string path = tempPath("text-as-binary");
    saveTrackConfig(path, defaultTrack());
    EXPECT_THROW(loadQualifyingBinary(path), std::runtime_error);
}

TEST(BrokenFileTest, OtherVersionThrows)
{
    const std::string path = tempPath("v2.bin");
    saveQualifyingBinary(path, {});
    std::string bytes = slurp(path);
    bytes[4] = 2;
    spit(path, bytes);
    EXPECT_THROW(loadQualifyingBinary(path), std::runtime_error);
}

TEST(BrokenFileTest, TruncatedFileThrows)
{
    const std::string path = tempPath("truncated.bin");
    saveQualifyingBinary(path, {crashedNewton(), crashedNewton()});
    const std::string bytes = slurp(path);
    spit(path, bytes.substr(0, bytes.size() - 10));
    EXPECT_THROW(loadQualifyingBinary(path), std::runtime_error);
}
