#pragma once

#include <cstddef>
#include <istream>
#include <ostream>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <vector>

#include "formulat/qualifying.h"
#include "formulat/runsection.h"

// Serialización binaria: los archivos que intercambian los programas (la GUI
// lee de acá los resultados de la clasificación). Leer tira std::runtime_error
// si el archivo se termina antes de tiempo.

namespace formulat {

// --- clase 2: cualquier tipo que se pueda copiar byte a byte, tal cual -------------

template <typename T>
void writeBinary(std::ostream& os, const T& value)
{
    // TODO
    (void)os;
    (void)value;
    throw std::logic_error("sin implementar");
}

template <typename T>
void readBinary(std::istream& is, T& value)
{
    // TODO
    (void)is;
    (void)value;
    throw std::logic_error("sin implementar");
}

// --- clase 2: CarResult, con sus serialize/deserialize ----------------------------------

void writeBinary(std::ostream& os, const CarResult& value);
void readBinary(std::istream& is, CarResult& value);

// --- clase 2: cualquier vector, el tamaño y después cada elemento -----------------------

template <typename T>
void writeBinary(std::ostream& os, const std::vector<T>& values)
{
    // TODO
    (void)os;
    (void)values;
    throw std::logic_error("sin implementar");
}

template <typename T>
void readBinary(std::istream& is, std::vector<T>& values)
{
    // TODO
    (void)is;
    (void)values;
    throw std::logic_error("sin implementar");
}

// --- clase 1: el archivo de la clasificación (qualifying.bin) ---------------------------

void saveQualifyingBinary(const std::string& path, const std::vector<CarResult>& cars);
std::vector<CarResult> loadQualifyingBinary(const std::string& path);

}  // namespace formulat
