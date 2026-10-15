#pragma once

#include <chrono>
#include <cstddef>
#include <istream>
#include <ostream>
#include <string>
#include <vector>

#include "formulat/track.h"
#include "formulat/engines.h"
#include "formulat/qualifying.h"
#include "formulat/runsection.h"

// Text serialization (given code), for files people read and edit: the track,
// the qualifying results sheet, the accident data recorders. write(os, x) puts
// x on the stream; read(is, x) takes it back, so that write then read gives
// back an equal x (bit for bit, for doubles: max_digits10). Everything that
// fails to read throws std::runtime_error. Compare with binary.h: text needs
// names for enums, quotes for strings and 17 digits for a double; binary just
// copies the bytes.

namespace formulat {

// --- numbers ---------------------------------------------------------------------

void write(std::ostream& os, double value);   // max_digits10: the exact double comes back
void read(std::istream& is, double& value);   // also "nan", "inf" and denormals
void write(std::ostream& os, int value);
void read(std::istream& is, int& value);
void write(std::ostream& os, unsigned value);
void read(std::istream& is, unsigned& value);
void write(std::ostream& os, std::size_t value);
void read(std::istream& is, std::size_t& value);
void write(std::ostream& os, bool value);
void read(std::istream& is, bool& value);

// --- strings: quoted, so they may contain spaces ---------------------------------

void write(std::ostream& os, const std::string& value);
void read(std::istream& is, std::string& value);

// --- enums: by name ("Straight", "Toms748", "Warm", "Clean") ---------------------

void write(std::ostream& os, SectionType value);
void read(std::istream& is, SectionType& value);
void write(std::ostream& os, Team value);
void read(std::istream& is, Team& value);
void write(std::ostream& os, Driver value);
void read(std::istream& is, Driver& value);
void write(std::ostream& os, Outcome value);
void read(std::istream& is, Outcome& value);

// --- durations: the nanosecond count ----------------------------------------------

void write(std::ostream& os, std::chrono::nanoseconds value);
void read(std::istream& is, std::chrono::nanoseconds& value);

// --- small structs: one line, fields in order ---------------------------------------

void write(std::ostream& os, const SectionSpec& value);
void read(std::istream& is, SectionSpec& value);
void write(std::ostream& os, const SectionResult& value);
void read(std::istream& is, SectionResult& value);

// --- big structs: one labelled field per line ("laps 1000") -------------------------

void writeLabel(std::ostream& os, const std::string& label);   // "label "
void expectLabel(std::istream& is, const std::string& label);  // throws if the next word differs

void write(std::ostream& os, const TrackConfig& value);
void read(std::istream& is, TrackConfig& value);
void write(std::ostream& os, const CarResult& value);
void read(std::istream& is, CarResult& value);
void write(std::ostream& os, const AccidentDataRecorder& value);
void read(std::istream& is, AccidentDataRecorder& value);

// --- vectors: the size, then one element per line (one overload per type) -----------

void write(std::ostream& os, const std::vector<SectionSpec>& values);
void read(std::istream& is, std::vector<SectionSpec>& values);
void write(std::ostream& os, const std::vector<SectionResult>& values);
void read(std::istream& is, std::vector<SectionResult>& values);
void write(std::ostream& os, const std::vector<CarResult>& values);
void read(std::istream& is, std::vector<CarResult>& values);

// --- files: a header line "formulat-<kind> <version>", then the content --------------
// Loading throws if the file can't be opened, or is of another kind or version.

void saveTrackConfig(const std::string& path, const TrackConfig& config);
TrackConfig loadTrackConfig(const std::string& path);
void saveQualifying(const std::string& path, const std::vector<CarResult>& cars);
std::vector<CarResult> loadQualifying(const std::string& path);
void saveAdr(const std::string& path, const AccidentDataRecorder& adr);
AccidentDataRecorder loadAdr(const std::string& path);

}  // namespace formulat
