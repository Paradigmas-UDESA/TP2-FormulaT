#pragma once

#include <cstddef>
#include <string>
#include <vector>

#include "formulat/car.h"
#include "formulat/track.h"
#include "formulat/engines.h"
#include "formulat/runsection.h"

// ============================================================================
// TODO (clase 5): la bandera roja, después de la carrera.
//
// Después de una bandera roja solo importa el orden: la carrera se relanza
// desde la grilla, cada auto en una vuelta nueva, con las vueltas que tenía.
// El orden se guarda en binario (los templates de la clase 2), y otra
// ejecución retoma desde el archivo (build/race --resume results/red-flag.bin).
//
// Preguntas para el diseño:
// - El orden se arma cuando ya terminaron todos los threads (después del
//   join). ¿Hace falta algún lock para leer los autos?
// - GridSlot no tiene código propio de serialización: ¿por qué alcanza con el
//   template genérico, y por qué no alcanzaba con CarResult?
//
// Las firmas son el contrato: no las cambien. Implementen en src/redflag.cpp.
// Mientras tanto, todo tira std::logic_error("sin implementar").
// ============================================================================

namespace formulat {

// Un lugar en la grilla de relanzamiento.
struct GridSlot {
    Team team;
    Driver driver;
    std::size_t laps;
};

// Los autos que no abandonaron: primero más vueltas, después menos tiempo de
// carrera. Si empatan en las dos cosas, en el orden de la grilla.
std::vector<GridSlot> redFlagOrder(const std::vector<Car>& grid, const Track& track);

// "FTRF", una versión de 32 bits (1), los lugares. Cargar tira
// std::runtime_error si el archivo no se puede abrir, o si es de otro tipo u
// otra versión.
void saveRedFlag(const std::string& path, const std::vector<GridSlot>& order);
std::vector<GridSlot> loadRedFlag(const std::string& path);

// La grilla de relanzamiento: los autos en ese orden, cada uno retomando
// después de sus vueltas.
std::vector<Car> makeGrid(const Track& track, const std::vector<GridSlot>& order);

}  // namespace formulat
