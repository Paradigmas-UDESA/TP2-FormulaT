#include "formulat/serialize.h"

#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <iterator>
#include <limits>
#include <stdexcept>

namespace formulat {

namespace {

std::string nextWord(std::istream& is, const std::string& what)
{
    std::string word;
    if (!(is >> word))
        throw std::runtime_error("read: expected " + what + ", found the end of the input");
    return word;
}

const char* const kSectionTypes[] = {"Straight", "Hairpin", "Sweepers", "Chicane", "Kerb"};
const char* const kTeams[] = {"Bisection", "Toms748", "Newton", "Halley", "Secant", "RegulaFalsi"};
const char* const kDrivers[] = {"Cold", "Warm"};
const char* const kOutcomes[] = {"Clean", "Penalty", "DNF"};

// The position of `word` in `names`, or throws.
int indexOf(const std::string& word, const char* const* names, std::size_t count)
{
    for (std::size_t i = 0; i < count; ++i)
        if (word == names[i])
            return i;
    throw std::runtime_error("read: unknown name '" + word + "'");
}

void writeHeader(std::ostream& os, const std::string& kind)
{
    os << "formulat-" << kind << " 1\n";
}

void checkHeader(std::istream& is, const std::string& kind, const std::string& path)
{
    std::string header;
    int version = 0;
    if (!(is >> header >> version) || header != "formulat-" + kind)
        throw std::runtime_error(path + " is not a formulat-" + kind + " file");
    if (version != 1)
        throw std::runtime_error(path + ": unsupported version " + std::to_string(version));
}

std::ofstream openForWriting(const std::string& path)
{
    std::ofstream os(path);
    if (!os)
        throw std::runtime_error("cannot write " + path);
    return os;
}

std::ifstream openForReading(const std::string& path)
{
    std::ifstream is(path);
    if (!is)
        throw std::runtime_error("cannot open " + path);
    return is;
}

// `>>` reads "-1" into an unsigned as its largest value, without failing.
void rejectNegative(std::istream& is)
{
    if ((is >> std::ws).peek() == '-')
        throw std::runtime_error("read: expected a non-negative integer");
}

// Closing flushes what is still in the buffer: a write error may only show up here.
void finish(std::ofstream& os, const std::string& path)
{
    os.close();
    if (!os)
        throw std::runtime_error("error writing " + path);
}

}  // namespace

// --- numbers ---------------------------------------------------------------------

void write(std::ostream& os, double value)
{
    const std::streamsize old = os.precision(std::numeric_limits<double>::max_digits10);
    os << value;
    os.precision(old);
}

void read(std::istream& is, double& value)
{
    // `is >> value` rejects the "nan" and "inf" that `<<` writes, and
    // std::stod throws on denormals (ERANGE); strtod handles both.
    const std::string token = nextWord(is, "a number");
    char* end = nullptr;
    value = std::strtod(token.c_str(), &end);
    if (end != token.c_str() + token.size())
        throw std::runtime_error("read: not a number: " + token);
}

void write(std::ostream& os, int value) { os << value; }

void read(std::istream& is, int& value)
{
    if (!(is >> value))
        throw std::runtime_error("read: expected an integer");
}

void write(std::ostream& os, unsigned value) { os << value; }

void read(std::istream& is, unsigned& value)
{
    rejectNegative(is);
    if (!(is >> value))
        throw std::runtime_error("read: expected an integer");
}

void write(std::ostream& os, std::size_t value) { os << value; }

void read(std::istream& is, std::size_t& value)
{
    rejectNegative(is);
    if (!(is >> value))
        throw std::runtime_error("read: expected an integer");
}

void write(std::ostream& os, bool value) { os << (value ? 1 : 0); }

void read(std::istream& is, bool& value)
{
    int v = 0;
    if (!(is >> v) || (v != 0 && v != 1))
        throw std::runtime_error("read: expected 0 or 1");
    value = v == 1;
}

// --- strings -----------------------------------------------------------------------

void write(std::ostream& os, const std::string& value) { os << std::quoted(value); }

void read(std::istream& is, std::string& value)
{
    if (!(is >> std::quoted(value)))
        throw std::runtime_error("read: expected a quoted string");
}

// --- enums -------------------------------------------------------------------------

void write(std::ostream& os, SectionType value) { os << kSectionTypes[static_cast<int>(value)]; }

void read(std::istream& is, SectionType& value)
{
    value = static_cast<SectionType>(
        indexOf(nextWord(is, "a section type"), kSectionTypes, std::size(kSectionTypes)));
}

void write(std::ostream& os, Team value) { os << kTeams[static_cast<int>(value)]; }

void read(std::istream& is, Team& value)
{
    value = static_cast<Team>(indexOf(nextWord(is, "a team"), kTeams, std::size(kTeams)));
}

void write(std::ostream& os, Driver value) { os << kDrivers[static_cast<int>(value)]; }

void read(std::istream& is, Driver& value)
{
    value = static_cast<Driver>(indexOf(nextWord(is, "a driver"), kDrivers, std::size(kDrivers)));
}

void write(std::ostream& os, Outcome value) { os << kOutcomes[static_cast<int>(value)]; }

void read(std::istream& is, Outcome& value)
{
    value = static_cast<Outcome>(indexOf(nextWord(is, "an outcome"), kOutcomes, std::size(kOutcomes)));
}

// --- durations ------------------------------------------------------------------------

void write(std::ostream& os, std::chrono::nanoseconds value) { os << value.count(); }

void read(std::istream& is, std::chrono::nanoseconds& value)
{
    std::chrono::nanoseconds::rep count = 0;
    if (!(is >> count))
        throw std::runtime_error("read: expected a duration");
    value = std::chrono::nanoseconds{count};
}

// --- small structs ---------------------------------------------------------------------

void write(std::ostream& os, const SectionSpec& value)
{
    write(os, value.type);
    os << ' ';
    write(os, value.degree);
    os << ' ';
    write(os, value.targets);
}

void read(std::istream& is, SectionSpec& value)
{
    read(is, value.type);
    read(is, value.degree);
    read(is, value.targets);
}

void write(std::ostream& os, const SectionResult& value)
{
    write(os, value.outcome);
    os << ' ';
    write(os, value.pitStop);
    os << ' ';
    write(os, value.time);
    os << ' ';
    write(os, value.residual);
}

void read(std::istream& is, SectionResult& value)
{
    read(is, value.outcome);
    read(is, value.pitStop);
    read(is, value.time);
    read(is, value.residual);
}

// --- labels and big structs --------------------------------------------------------------

void writeLabel(std::ostream& os, const std::string& label) { os << label << ' '; }

void expectLabel(std::istream& is, const std::string& label)
{
    const std::string found = nextWord(is, "'" + label + "'");
    if (found != label)
        throw std::runtime_error("read: expected '" + label + "', found '" + found + "'");
}

void write(std::ostream& os, const TrackConfig& value)
{
    writeLabel(os, "name");
    write(os, value.name);
    os << '\n';
    writeLabel(os, "laps");
    write(os, value.laps);
    os << '\n';
    writeLabel(os, "eps");
    write(os, value.eps);
    os << '\n';
    writeLabel(os, "penaltyPct");
    write(os, value.penaltyPct);
    os << '\n';
    writeLabel(os, "seed");
    write(os, value.seed);
    os << '\n';
    writeLabel(os, "layout");
    write(os, value.layout);
    os << '\n';
}

void read(std::istream& is, TrackConfig& value)
{
    expectLabel(is, "name");
    read(is, value.name);
    expectLabel(is, "laps");
    read(is, value.laps);
    expectLabel(is, "eps");
    read(is, value.eps);
    expectLabel(is, "penaltyPct");
    read(is, value.penaltyPct);
    expectLabel(is, "seed");
    read(is, value.seed);
    expectLabel(is, "layout");
    read(is, value.layout);
}

void write(std::ostream& os, const CarResult& value)
{
    writeLabel(os, "team");
    write(os, value.team);
    os << '\n';
    writeLabel(os, "driver");
    write(os, value.driver);
    os << '\n';
    writeLabel(os, "finished");
    write(os, value.finished);
    os << '\n';
    writeLabel(os, "total");
    write(os, value.total);
    os << '\n';
    writeLabel(os, "sections");
    write(os, value.sections);
    os << '\n';
}

void read(std::istream& is, CarResult& value)
{
    expectLabel(is, "team");
    read(is, value.team);
    expectLabel(is, "driver");
    read(is, value.driver);
    expectLabel(is, "finished");
    read(is, value.finished);
    expectLabel(is, "total");
    read(is, value.total);
    expectLabel(is, "sections");
    read(is, value.sections);
}

void write(std::ostream& os, const AccidentDataRecorder& value)
{
    writeLabel(os, "team");
    write(os, value.team);
    os << '\n';
    writeLabel(os, "driver");
    write(os, value.driver);
    os << '\n';
    writeLabel(os, "lap");
    write(os, value.lap);
    os << '\n';
    writeLabel(os, "section");
    write(os, value.section);
    os << '\n';
    writeLabel(os, "type");
    write(os, value.type);
    os << '\n';
    writeLabel(os, "time");
    write(os, value.time);
    os << '\n';
    writeLabel(os, "pitStops");
    write(os, value.pitStops);
    os << '\n';
    writeLabel(os, "penalties");
    write(os, value.penalties);
    os << '\n';
    writeLabel(os, "residual");
    write(os, value.residual);
    os << '\n';
    writeLabel(os, "reason");
    write(os, value.reason);
    os << '\n';
}

void read(std::istream& is, AccidentDataRecorder& value)
{
    expectLabel(is, "team");
    read(is, value.team);
    expectLabel(is, "driver");
    read(is, value.driver);
    expectLabel(is, "lap");
    read(is, value.lap);
    expectLabel(is, "section");
    read(is, value.section);
    expectLabel(is, "type");
    read(is, value.type);
    expectLabel(is, "time");
    read(is, value.time);
    expectLabel(is, "pitStops");
    read(is, value.pitStops);
    expectLabel(is, "penalties");
    read(is, value.penalties);
    expectLabel(is, "residual");
    read(is, value.residual);
    expectLabel(is, "reason");
    read(is, value.reason);
}

// --- vectors: the same code three times (binary.h does it with one template) ------------

void write(std::ostream& os, const std::vector<SectionSpec>& values)
{
    write(os, values.size());
    for (const SectionSpec& v : values) {
        os << '\n';
        write(os, v);
    }
}

void read(std::istream& is, std::vector<SectionSpec>& values)
{
    std::size_t n = 0;
    read(is, n);
    values.clear();
    for (std::size_t i = 0; i < n; ++i) {
        SectionSpec v{};
        read(is, v);
        values.push_back(v);
    }
}

void write(std::ostream& os, const std::vector<SectionResult>& values)
{
    write(os, values.size());
    for (const SectionResult& v : values) {
        os << '\n';
        write(os, v);
    }
}

void read(std::istream& is, std::vector<SectionResult>& values)
{
    std::size_t n = 0;
    read(is, n);
    values.clear();
    for (std::size_t i = 0; i < n; ++i) {
        SectionResult v{};
        read(is, v);
        values.push_back(v);
    }
}

void write(std::ostream& os, const std::vector<CarResult>& values)
{
    write(os, values.size());
    for (const CarResult& v : values) {
        os << '\n';
        write(os, v);
    }
}

void read(std::istream& is, std::vector<CarResult>& values)
{
    std::size_t n = 0;
    read(is, n);
    values.clear();
    for (std::size_t i = 0; i < n; ++i) {
        CarResult v{};
        read(is, v);
        values.push_back(v);
    }
}

// --- files ------------------------------------------------------------------------------

void saveTrackConfig(const std::string& path, const TrackConfig& config)
{
    std::ofstream os = openForWriting(path);
    writeHeader(os, "track");
    write(os, config);
    finish(os, path);
}

TrackConfig loadTrackConfig(const std::string& path)
{
    std::ifstream is = openForReading(path);
    checkHeader(is, "track", path);
    TrackConfig config;
    read(is, config);
    return config;
}

void saveQualifying(const std::string& path, const std::vector<CarResult>& cars)
{
    std::ofstream os = openForWriting(path);
    writeHeader(os, "qualifying");
    write(os, cars);
    os << '\n';
    finish(os, path);
}

std::vector<CarResult> loadQualifying(const std::string& path)
{
    std::ifstream is = openForReading(path);
    checkHeader(is, "qualifying", path);
    std::vector<CarResult> cars;
    read(is, cars);
    return cars;
}

void saveAdr(const std::string& path, const AccidentDataRecorder& adr)
{
    std::ofstream os = openForWriting(path);
    writeHeader(os, "adr");
    write(os, adr);
    finish(os, path);
}

AccidentDataRecorder loadAdr(const std::string& path)
{
    std::ifstream is = openForReading(path);
    checkHeader(is, "adr", path);
    AccidentDataRecorder adr{};
    read(is, adr);
    return adr;
}

}  // namespace formulat
