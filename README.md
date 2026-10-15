<p align="center">
  <img src="assets/enunciado/udesa_logo.png" alt="Universidad de San Andrés" width="200">
</p>

# Paradigmas de Programación

## TP2: Formula T

## Introducción

**Formula T** (de *Formula Thread*) es una carrera al estilo de la
[Fórmula 1](https://es.wikipedia.org/wiki/F%C3%B3rmula_1) en la que cada auto es un
**thread** y la pista es una sucesión de problemas de análisis numérico: encontrar, una y otra
vez, un `x` tal que `p(x) = c` (donde `p` es un polinomio). Gana el auto que resuelve todos los problemas de la
pista, vuelta tras vuelta, en el menor tiempo.

Hay seis equipos con dos pilotos cada uno: doce autos. Cada equipo es un **algoritmo
para encontrar raíces**, el motor del auto, y los dos pilotos de un equipo usan el
mismo motor pero lo manejan distinto. Ningún equipo es el más rápido en todas partes:
cada tipo de tramo favorece a unos y castiga a otros, y gana el que menos pierde donde
es débil.

Mientras un auto corre solo, todo es determinístico: la misma pista da siempre los
mismos resultados. Cuando corren los doce a la vez, en doce threads, aparece lo que no
se controla: los threads compiten por los núcleos de la máquina (que suelen ser menos
de doce) y el sistema operativo decide quién avanza en cada momento. Todos los autos
comparten la pista y la torre de tiempos, y doce threads escribiendo el mismo dato a la
vez es un problema que hay que resolver.

En este repositorio ya están implementados el problema matemático, los motores, el
árbitro, la pista y una interfaz gráfica. Falta guardar los resultados, armar los autos
y la grilla, largar la carrera con un thread por auto y coordinarlos para que no se
pisen. **Los headers (`include/formulat/`) son el contrato**: están documentados
función por función, y decir qué tiene que hacer cada una es trabajo de ellos, no de
este enunciado.

Este trabajo lo van a ir haciendo en sucesivas clases, a medida que vean los temas necesarios para resolverlo.

---

### El problema: encontrar raíces

Cada tramo de la pista es un polinomio `p`, un intervalo `[a, b]` y una lista de
**objetivos** `c₁, …, cₖ`. Para cada objetivo, el auto tiene que encontrar un `x` en
`[a, b]` tal que `p(x) = c` o, lo que es lo mismo, una raíz de `p(x) − c`. Salvo en
polinomios de grado muy bajo no hay fórmula, así que se aproxima: se arranca de algún
punto y se mejora paso a paso hasta que el **residuo** `|p(x) − c|` es a lo sumo una
tolerancia `ε` (en la pista del TP, `ε = 10⁻¹²`).

Hay dos familias de métodos:

- **Los que encierran la raíz**: si `p(a) − c` y `p(b) − c` tienen distinto signo, `p`
  cruza `c` en algún punto del medio, y se puede achicar el intervalo sin perder el
  cruce: es una búsqueda binaria sobre los reales. Nunca fallan, pero necesitan ese
  **cambio de signo** para arrancar.
- **Los abiertos**: arrancan de un punto y se mueven siguiendo la forma de `p`. Cuando
  arrancan cerca son rapidísimos; cuando no, pueden irse lejos, quedar dando vueltas o
  dividir por cero.

### Los equipos

- **Bisección (`Bisection`)**: encierra la raíz y parte el intervalo a la mitad en cada
  paso. Lento y seguro.
- **TOMS 748 (`Toms748`)**: encierra la raíz, y combina la bisección con
  interpolación. Confiable, de mitad de tabla.
- **Regula falsi (`RegulaFalsi`)**: encierra la raíz y corta donde la recta entre los
  extremos cruza `c`. A veces se estanca de un lado.
- **Newton (`Newton`)**: abierto; sigue la recta tangente, así que usa `p` y su
  derivada `p′`. El más rápido cuando anda, y sin ninguna protección cuando no.
- **Halley (`Halley`)**: abierto; como Newton pero también con `p″`. Converge todavía
  más rápido, con pasos más caros.
- **Secante (`Secant`)**: abierto; Newton sin derivada, que aproxima la tangente con
  los dos últimos puntos. Pasos baratos, igual de frágil que Newton.

La velocidad sale de dos cosas que tiran para lados opuestos. Una es **cuántos pasos**
necesita cada método: la bisección gana un bit de precisión por paso, y Newton duplica
los dígitos correctos en cada uno. La otra es **cuánto cuesta cada paso**: la bisección y
la secante evalúan `p` una vez, Newton evalúa `p` y `p′`, y Halley las tres. En un
polinomio de grado alto evaluar es caro, y la secante le puede ganar a Newton aunque
necesite más pasos.

Los motores ya están implementados (`src/engines.cpp`). No hace falta saber cómo
funcionan por dentro para hacer el TP.

### Los pilotos

Los dos pilotos de un equipo usan el mismo motor; lo que cambia es **desde dónde
arranca** cada búsqueda:

- **En frío (`Cold`)**: cada objetivo arranca de cero, del medio del intervalo (o del
  intervalo entero, si el motor encierra la raíz).
- **En caliente (`Warm`)**: arranca desde la raíz del objetivo anterior. Si los
  objetivos vienen ordenados, la raíz siguiente está cerca y casi no hay que buscar; si
  vienen desordenados, arrancar de una raíz vieja puede ser peor que arrancar del medio.

### La pista

Una vuelta recorre los tramos en orden, y la carrera es muchas vueltas seguidas: la
pista del TP (`tracks/formula-t.track`) tiene 10 tramos de 1000 objetivos cada uno, y
la carrera dura 1000 vueltas. Cada tramo es de un **tipo**, y cada tipo es una familia
de problemas:

- **`Straight`** (recta): grado bajo y `p` creciente, un solo cruce por objetivo.
  Terreno de Newton y Halley.
- **`Hairpin`** (horquilla): grado alto, así que evaluar `p` es caro. Terreno de la
  secante.
- **`Sweepers`** (curvones): los objetivos vienen ordenados. Terreno de los pilotos en
  caliente.
- **`Chicane`** (chicana): `p` es plano justo en el medio del intervalo, donde arrancan
  los pilotos en frío.
- **`Kerb`** (piano): una loma. Algunos objetivos se cruzan dos veces, y entre los
  extremos no hay cambio de signo.

Los polinomios los genera el programa a partir del archivo de pista y una semilla fija:
el mismo archivo da siempre la misma pista.

### El árbitro

Al terminar cada tramo, el árbitro mira el **peor residuo** entre todos los objetivos:

- **Limpio** si es a lo sumo `ε`.
- **Penalización** si está entre `ε` y `2ε`: el tiempo del tramo sube un 50%.
- **Abandono** (*DNF*, *did not finish*) si es más grande, si algún `x` no es un número
  (`NaN`, infinito) o quedó fuera de `[a, b]`, o si el motor se rinde o tira una
  excepción.

Los motores que encierran la raíz no pueden arrancar sin cambio de signo. En un tramo
así el auto **entra a boxes**: un motor de auxilio resuelve ese tramo por él, y su
tiempo cuenta. No es un error, es estrategia.

Cuando un auto abandona deja su **ADR** (*accident data recorder*, la caja negra que
llevan los autos de Fórmula 1): en qué vuelta y en qué tramo abandonó, cuánto tiempo
llevaba, sus paradas en boxes y penalizaciones, el residuo y el motivo.

Algunos autos abandonan siempre, y siempre en el mismo tramo, por las características del algoritmo y del problema.

### La clasificación

Antes de la carrera hay una **clasificación**: cada auto da una sola vuelta, uno después
del otro, sin threads. La tabla ordena primero a los que terminaron, por tiempo total, y
después a los que abandonaron, por cuántos tramos completaron. `make qualifying` la
corre, imprime la tabla y guarda los resultados en `results/`: una planilla para leer
(`qualifying.txt`), los mismos resultados en binario para la interfaz
(`qualifying.bin`) y un ADR por cada abandono.

### La carrera

En la carrera corren los doce autos **a la vez, cada uno en su propio thread**, todas las
vueltas. Se prende el semáforo y, cuando se apaga, los autos salen en orden de grilla.
Cada auto informa su avance a una **torre de tiempos** compartida, que el programa
principal consulta para mostrar la carrera en vivo. Con la **bandera roja** la carrera
se detiene: cada auto para al terminar su vuelta, se guarda el orden en que iban, y otra
ejecución puede relanzarla desde ahí.

En una máquina de 4 núcleos, una carrera dura alrededor de medio minuto, y dos carreras
seguidas no terminan igual.

### La interfaz gráfica

La interfaz es una página web que solo lee los archivos que escriben los programas; no es
parte del TP. Con `make gui` abran `http://localhost:8000` (VS Code reenvía
el puerto solo). Tiene dos vistas: la **repetición de la clasificación**, con los doce
autos dando su vuelta a la vez a partir de `results/qualifying.bin`, y la **carrera en
vivo**, mientras corre `make race`, con la torre de tiempos y un botón de bandera roja.

## Grupos

Como en el TP anterior, se hace en grupos de a 2: no más, no menos. Armen sus grupos
en Classroom50 y en el campus, y pónganles algún nombre divertido. Si alguien queda sin
grupo, se lo asignamos nosotros.

## Consigna

El TP se hace a lo largo de cinco clases. Cada una tiene sus propios tests y construye
sobre la anterior, así que conviene hacerlas en orden:

- **Clase 1, Serialización**: guardar y leer los resultados de la clasificación en
  binario. Al terminarla, la interfaz muestra los suyos.
- **Clase 2, Templates**: refactorizar la serialización de la clase 1 con templates.
- **Clase 3, Contenedores**: los autos y la grilla; cada auto recorre su cola de
  tramos.
- **Clase 4, Threads**: la carrera, con un thread por auto.
- **Clase 5, Sincronización**: la torre de tiempos compartida y la bandera roja, sin carreras de
  datos.

---

### Clase 1, Serialización

Implementen `CarResult::serialize`, `CarResult::deserialize`, `saveQualifyingBinary` y
`loadQualifyingBinary`, en `src/binary.cpp`.

`make qualifying` ya corre la clasificación, imprime la tabla y guarda la planilla de
texto. Lo que falta es `results/qualifying.bin`: los mismos resultados en binario, que es
lo que lee la interfaz para la repetición. Hasta que lo implementen, `make qualifying`
termina con `error: sin implementar`.

Cada `CarResult` es el resultado de un auto, con la lista de tramos que corrió
(`include/formulat/qualifying.h` y `include/formulat/runsection.h`):

```cpp
struct SectionResult {
    Outcome outcome;                       // Clean, Penalty o DNF
    int pitStop;                           // 1 si entró a boxes
    std::chrono::nanoseconds time;
    double residual;                       // el peor |p(x) - c|
};

struct CarResult {
    Team team;
    Driver driver;
    bool finished;
    std::chrono::nanoseconds total;
    std::vector<SectionResult> sections;

    void serialize(std::ostream& out) const;   // lo que implementan
    void deserialize(std::istream& in);
};
```

`serialize` lo escribe en binario, campo por campo, con `write`, `reinterpret_cast` y
`sizeof`, como en la teórica. `deserialize` lo lee de vuelta, en el mismo orden. El
archivo entero queda así:

```
qualifying.bin {
    magic      "FTQB"                     4 bytes
    version    1                          uint32_t, 4 bytes
    cantidad   12                         size_t, 8 bytes: cuántos autos vienen
    auto {                                12 veces, uno por auto
        team                              4 bytes
        driver                            4 bytes
        finished                          1 byte
        total                             8 bytes
        cantidad                          size_t, 8 bytes: cuántos tramos vienen
        SectionResult                     24 bytes cada uno
        ...
    }
    ...
}
```

Esto es un dibujo para leerlo: en el archivo no hay llaves, nombres ni espacios, solo los
bytes de cada campo, uno detrás del otro. El orden y los tamaños no los eligen ustedes: la
interfaz lee esos bytes tal cual, y los tests los comparan uno por uno.

Leer tiene que tirar `std::runtime_error` si el archivo no existe, es de otro tipo, es de
otra versión o se termina antes de tiempo. Guardar también, si no puede escribir el archivo
(cuando `!out`).

> **Tip:** los `SectionResult` de un `std::vector` están uno detrás del otro en memoria, y
> `.data()` da un puntero al primero. Con eso pueden escribir (y leer) todos los tramos con
> un solo `write` (o `read`), como en la teórica.

> **Tip (2):** para darse cuenta de que el archivo se terminó antes de tiempo, miren qué
> devuelve `in.read(...)` (el mismo stream, que se puede usar como condición en un `if`) o
> cuántos bytes leyó, con `in.gcount()`.

> **Tip (3):** para comparar los 4 bytes que leyeron con `"FTQB"`, usen `std::memcmp` (de
> `<cstring>`): compara dos zonas de memoria byte a byte, y devuelve 0 si son iguales.

> **Para pensar:** `SectionResult` tiene solo números y un enum, y se puede escribir
> entero, con un solo `write`. `CarResult` no. ¿Qué hay adentro de un `std::vector`? ¿Qué
> quedaría en el archivo si escribieran el `CarResult` entero, y qué pasaría al leerlo en
> otra ejecución del programa?

> **Para pensar (2):** al leer, ¿cómo saben cuántos tramos vienen? ¿Y para qué sirven los
> 4 bytes `FTQB` y la versión al principio del archivo?

Tests: `make test-clase1`. Cuando pasen, corran `make qualifying` y `make gui`: la
repetición de la clasificación muestra los resultados de su archivo.

### Clase 2, Templates

Implementen los templates `writeBinary` y `readBinary`, en `include/formulat/binary.h`, y
las sobrecargas para `CarResult`, en `src/binary.cpp`. Después reescriban
con ellos lo que hicieron en la clase 1.

En la clase 1 escribieron una y otra vez la misma línea, cambiando solo el campo:

```cpp
out.write(reinterpret_cast<const char*>(&team), sizeof(team));
out.write(reinterpret_cast<const char*>(&driver), sizeof(driver));
...
```

Un template escribe esa línea una sola vez, para cualquier tipo `T`. Hacen falta tres
funciones de escritura (y sus tres de lectura):

```cpp
template <typename T>
void writeBinary(std::ostream& os, const T& value);                 // sus bytes, tal cual

void writeBinary(std::ostream& os, const CarResult& value);         // llama a value.serialize(os)

template <typename T>
void writeBinary(std::ostream& os, const std::vector<T>& values);   // el tamaño y cada elemento
```

El primero sirve para todo lo que se puede copiar byte a byte: números, enums, duraciones,
y structs hechos solo de eso, como `SectionResult`. No necesitan escribir nada más para
ellos. Para que nadie le pase otra cosa, protéjanlo con un `static_assert` y
`std::is_trivially_copyable_v<T>` (búsquenlos). El de `vector<T>` escribe cada elemento
con *su* `writeBinary`, sea cual sea: un `vector<SectionResult>`, un `vector<CarResult>` o
un `vector<std::vector<double>>`.

Los templates van en el header: el compilador necesita ver el código entero para generar
cada versión. Las firmas ya están ahí; no las cambien.

Con eso, reescriban `CarResult::serialize`, `CarResult::deserialize`,
`saveQualifyingBinary` y `loadQualifyingBinary`: cada campo pasa a ser un `writeBinary`.
Los bytes del archivo no cambian, y eso lo controlan los tests de la clase 1, que tienen
que seguir pasando. Esos son los que les dicen si el refactor está bien.

No es solo para la clase 1: en las clases 4 y 5, el código dado usa sus templates para
escribir el estado de la carrera en vivo (`results/live.bin`, el que muestra la interfaz)
y para guardar el orden de largada después de una bandera roja.

> **Tip:** en la clase 1 escribieron todo el vector de tramos con un solo `write`, usando
> `.data()`. Acá no se puede: el template de `vector<T>` tiene que funcionar también para
> el `vector<CarResult>` del archivo. ¿Qué hay en los bytes de un `CarResult`? (Ya lo
> vieron en la clase 1.)

> **Para pensar:** para `CarResult` escribieron una sobrecarga, una función común con el
> mismo nombre. Otra opción era una especialización del template
> (`template <> void writeBinary<CarResult>(...)`). ¿En qué se diferencian? ¿Y el template
> de `vector<T>`, se podría escribir como una especialización?

Tests: `make test-clase2`, y que `make test-clase1` siga pasando.

### Clase 3, Contenedores

Implementen `makeQueue`, `makeGrid` y las funciones de `Car`, en `src/car.cpp`. La clase
está declarada en `include/formulat/car.h`, con su estado (la parte `private:`) y
`retireAt` ya dados.

En la carrera, cada auto da las vueltas de la pista, tramo por tramo. Su recorrido es una
`std::queue` con los **índices** de los tramos en `track.sections`: para 2 vueltas de 3
tramos, `0 1 2 0 1 2`. `makeQueue(track, completed)` arma esa cola, sin los primeros
`completed` (para retomar una carrera a la mitad), y tira `std::invalid_argument` si
`completed` es más que toda la carrera.

Cada `Car` tiene su motor y su propio fallback, su cola, y lo que lleva corrido:

```cpp
class Car {
public:
    Car(Team team, Driver driver, const Track& track, std::size_t completed = 0);

    bool done() const;                 // terminó todos los tramos, o abandonó
    std::size_t completed() const;     // tramos completados
    void runNext(const Track& track);  // corre el próximo tramo de la cola
    void retire(const std::string& reason);

    const CarResult& result() const;
    const std::optional<AccidentDataRecorder>& adr() const;
    ...
};
```

`runNext` saca el próximo índice de la cola, corre ese tramo con `runSection` y suma el
resultado: lo agrega a `result().sections`, suma el tiempo, las entradas a boxes y las
penalizaciones. Si el árbitro le da DNF, el auto abandona: llamen a `retireAt` con el
residuo y el motivo (`refereeVerdict`). El tramo del DNF queda en `result().sections`,
pero no cuenta en `completed()`.

Las excepciones de los motores **no** se atrapan en `runNext`: se propagan, y quien corre
el auto lo retira con `retire(e.what())`. El ADR registra el último tramo que tomó
`runNext`, el que tiró la excepción. Retirar un auto dos veces no hace nada.

`makeGrid` arma la grilla: un `std::vector<Car>` con los 12 autos, en el orden de `Team`, y
dentro de cada equipo primero `Cold` y después `Warm`.

`runSequential` (dado, en `src/sequential.cpp`) corre la carrera entera sin threads: cada
auto hasta el final, uno después del otro. En la clase 4 van a correr el mismo ciclo, con un
thread por auto.

> **Tip:** `src/qualifying.cpp` hace casi lo mismo que `runNext`, para una sola vuelta: lean
> cómo arma el `CarResult` y el ADR. Uno de los tests compara una vuelta de
> `runSequential` con la clasificación, auto por auto.

> **Para pensar:** ¿por qué la cola tiene índices y no copias de los tramos? (12 autos,
> 1000 vueltas, cada tramo con su polinomio y sus objetivos.) Una `std::queue` no se puede
> indexar: ¿qué alcanza con guardar para retomar la carrera en otra ejecución?

Tests: `make test-clase3`. Todavía no hay nada para ver en la interfaz: la carrera en vivo
necesita los threads de la clase 4.

### Clase 4, Threads

Próximamente...

### Clase 5, Sincronización

Próximamente...

## Cómo se corrige

Cada clase tiene su propio conjunto de tests, y son los que están en `tests/`:

```sh
make test-clase1      # serialización
make test-clase2      # templates
make test-clase3      # contenedores
make test-clase4      # threads
make test-clase5      # sincronización
```

Se corrige corriendo esos mismos tests: una clase está bien resuelta cuando pasan todos
sus tests.

## Nota

Este TP tiene 4 ítems:

- Clases 1 y 2 (serialización y templates)
- Clase 3 (contenedores)
- Clase 4 (threads)
- Clase 5 (sincronización)

Para aprobar hacen falta 2 de los 4 ítems bien resueltos (con todos sus tests pasando), y
con eso llegan al 4. Con 3 de 4 llegan al 7, y con los 4, al 10.

Las clases se construyen una sobre otra, así que el orden importa: la carrera de la clase
4 usa los autos de la clase 3, y la clase 5 sincroniza la carrera de la clase 4.

## Entrega

1. Hagan push de su trabajo al repositorio.
2. Suban por el campus el hash del último commit.

## Entorno

Este repo se abre dentro del **dev container** del curso. No hace falta instalar el
compilador, googletest ni Boost a mano, y no importa si están en macOS, Windows o Linux:
la imagen es la misma para todos.

1. Docker Desktop + la extensión **Dev Containers** de VS Code (una vez para todo el
   cuatrimestre).
2. Abran esta carpeta en VS Code y acepten **"Reopen in Container"** (o
   `Cmd/Ctrl+Shift+P` → `Dev Containers: Reopen in Container`).
3. La imagen del curso ya trae CMake, googletest, Boost, gdb, valgrind y Python
   instalados y configurados.

## Compilar y correr

```sh
make                  # compila todo
make test-clase1      # compila y corre los tests de la clase 1 (y así hasta test-clase5)
make test-given       # los tests del código dado: pasan desde el primer día
make qualifying       # la clasificación
make race             # la carrera (con opciones: make race ARGS="--red-flag 5")
make tsan             # la carrera en el circuito de stress, con ThreadSanitizer
make gui              # la interfaz, en http://localhost:8000
make clean            # borra build/ y build-tsan/
```

`make tsan` compila con ThreadSanitizer, que avisa de cada data race que ve (`WARNING:
ThreadSanitizer: data race`) y termina con código 66. En la clase 4 la torre no tiene locks,
así que **es esperable** que reporte races: es lo que arreglan en la clase 5, donde tiene que
salir limpio.

Conviene que corran los tests de la clase que están haciendo: los de las clases que
faltan van a fallar igual, porque dependen de ellas, y eso hace ruido.

## Debug

`gdb` y `valgrind` están instalados y configurados. En `.vscode/launch.json` hay una
configuración por programa (elíjanla con el desplegable de Run and Debug antes de
apretar F5):

- **gdb: qualifying** y **gdb: race**: compilan y debuggean cada programa.
- **gdb: test_clase1** … **gdb: test_clase5**: compilan y debuggean los tests de
  cada clase.

## Estructura

- `include/formulat/`, `src/`: el problema, los motores, el árbitro, la pista y lo que
  van a construir
- `apps/`: los programas: `qualifying` y `race`
- `tracks/`: el archivo de la pista
- `gui/`: la interfaz gráfica; no es parte del TP
- `tests/`: los tests con los que se corrige, uno por clase
