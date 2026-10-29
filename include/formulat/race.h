#pragma once

#include <chrono>
#include <ostream>
#include <string>
#include <thread>
#include <vector>

#include "formulat/car.h"
#include "formulat/timingtower.h"
#include "formulat/track.h"

// La carrera, un thread por auto (clase 4). Implementen en src/race.cpp.

// ============================================================================
// TODO (clase 5): el log con un std::mutex, y la bandera roja (redFlag).
// - ¿Por qué cada auto mira la bandera recién al final de una vuelta?
// - Un auto que está en su última vuelta cuando sale la bandera, ¿qué hace?
// - Un bool compartido protegido por un mutex funciona. ¿Qué es std::atomic<bool>?
// ============================================================================

namespace formulat {

class Race {
public:
    // La grilla tiene que estar completa: los threads guardan referencias a
    // sus autos. log: donde los autos escriben sus eventos; nullptr = silencio.
    // La largada: se prenden 5 luces, una cada lightInterval, y se apagan; con
    // 0, enseguida. Todos los autos salen al apagado.
    Race(std::vector<Car>& grid, const Track& track, TimingTower& tower, std::ostream* log,
         std::chrono::nanoseconds lightInterval = {});

    Race(const Race&) = delete;
    Race& operator=(const Race&) = delete;

    // Fija el apagado del semáforo (5 x lightInterval desde ahora) y lanza un
    // thread por auto.
    void start();

    // (Clase 5) Bandera roja: cada auto en carrera se detiene al final de su
    // vuelta (en la torre: Stopped). Se rechaza (false) si algún auto ya terminó.
    bool redFlag();

    // Espera a todos los autos.
    void join();

    // Tiempo desde el apagado del semáforo: negativo mientras está prendido.
    std::chrono::nanoseconds elapsed() const;

    // Si los threads siguen corriendo, los espera.
    ~Race();

private:
    // TODO: el estado de la carrera.
};

}  // namespace formulat
