#pragma once

#include <chrono>
#include <cstddef>
#include <vector>

#include "formulat/car.h"
#include "formulat/engines.h"
#include "formulat/runsection.h"

// La torre de tiempos (clase 4, sin locks; la clase 5 la protege con un
// std::mutex). Implementen en src/timingtower.cpp.

namespace formulat {

// Lo que la torre sabe de un auto.
struct CarStatus {
    enum class State { Running, Finished, Retired, Stopped };  // Stopped: clase 5

    Team team;
    Driver driver;
    std::size_t completed = 0;           // secciones
    std::chrono::nanoseconds total{0};   // tiempo de carrera hasta ahora
    State state = State::Running;
    unsigned position = 0;               // 1.. al terminar, 0 si no
};

// Lo único que los threads de los autos comparten y modifican. Los autos se
// identifican por su índice en la grilla.
class TimingTower {
public:
    // Un estado Running por cada auto de la grilla.
    explicit TimingTower(const std::vector<Car>& grid);

    // Después de cada sección: el progreso del auto. También cuenta la sección.
    void report(std::size_t car, std::size_t completed, std::chrono::nanoseconds total);

    // El auto cruzó la meta: su posición es la siguiente (1, 2, ...). La devuelve.
    unsigned finish(std::size_t car);

    void retire(std::size_t car);

    // (Clase 5) Bandera roja: el auto se detuvo al final de una vuelta.
    void stop(std::size_t car);

    // Una COPIA del estado de todos los autos: quien la pide trabaja sobre la
    // copia, sin tocar la torre.
    std::vector<CarStatus> snapshot() const;

    // true cuando ningún auto está Running.
    bool allDone() const;

    // Todos los report() hasta ahora, de todos los autos.
    std::size_t sectionsRun() const;

private:
    // TODO: el estado de la torre.
};

}  // namespace formulat
