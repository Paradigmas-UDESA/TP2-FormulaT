#pragma once

#include <chrono>
#include <cstddef>
#include <memory>
#include <optional>
#include <queue>
#include <string>
#include <vector>

#include "formulat/track.h"
#include "formulat/engines.h"
#include "formulat/qualifying.h"
#include "formulat/runsection.h"

// El auto y su recorrido (clase 3). Implementen en src/car.cpp.

namespace formulat {

// El recorrido: vueltas x secciones índices a track.sections
// (0 1 ... n-1 0 1 ...), sin los primeros `completed`. Tira
// std::invalid_argument si completed > vueltas x secciones.
std::queue<std::size_t> makeQueue(const Track& track, std::size_t completed = 0);

class Car {
public:
    // Arranca después de `completed` secciones (0: desde la grilla; más:
    // retomando una carrera), con `start` de tiempo de carrera ya en el reloj
    // (el de su puesto en la grilla).
    Car(Team team, Driver driver, const Track& track, std::size_t completed = 0,
        std::chrono::nanoseconds start = {});

    Team team() const;
    Driver driver() const;

    // true cuando corrió todas las secciones, o abandonó.
    bool done() const;

    // Secciones completadas, contando las salteadas al construirlo. La del
    // DNF no cuenta.
    std::size_t completed() const;

    // Corre la próxima sección de la queue con runSection (nada si done()) y
    // agrega su SectionResult a result().sections, aunque sea un DNF. Un DNF
    // del árbitro lo retira, con su accident data recorder (el motivo:
    // refereeVerdict). Las excepciones de los motores NO se atrapan acá: se
    // propagan, y quien corre el auto decide (ver retire()).
    void runNext(const Track& track);

    // Lo retira desde afuera, después de que runNext tiró una excepción: el
    // ADR registra la última sección que tomó runNext, con residuo NaN. Solo
    // para un auto que no terminó; retirarlo dos veces no hace nada.
    void retire(const std::string& reason);

    const CarResult& result() const;
    const std::optional<AccidentDataRecorder>& adr() const;

private:
    // Dado: el estado del auto. No hace falta agregarle nada.
    std::unique_ptr<Engine> engine_;
    std::unique_ptr<Engine> fallback_;   // el suyo: los motores guardan estado de prepare()
    std::queue<std::size_t> queue_;      // las secciones que faltan, como índices a track.sections
    std::size_t completed_;
    std::size_t sectionsPerLap_;         // para la vuelta del ADR
    std::size_t current_ = 0;            // la última sección que tomó runNext
    SectionType currentType_ = SectionType::Straight;  // y su tipo
    unsigned pitStops_ = 0;
    unsigned penalties_ = 0;
    CarResult result_;
    std::optional<AccidentDataRecorder> adr_;  // vacío hasta que abandona

    // Dado (en src/car.cpp): lo retira en current_, con result().finished en
    // false y su ADR. Lo usan runNext y retire.
    void retireAt(double residual, const std::string& reason);
};

// Todos los equipos con sus dos pilotos: 12 autos, en el orden de Team, y
// dentro de cada equipo primero Cold y después Warm. Todos salen al apagado
// del semáforo; el auto i arranca con i x gap de tiempo de carrera, lo que le
// cuesta su puesto en la grilla.
std::vector<Car> makeGrid(const Track& track, std::chrono::nanoseconds gap = {});

}  // namespace formulat
