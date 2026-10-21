# DBACP con Max-Min Ant System

Proyecto para planificar y replanificar cursos pendientes mediante **Max-Min
Ant System (MMAS)**, siguiendo el modelo de `docs/Valencia-Francisco.pdf`.

## Estructura

```text
.
├── AGENTS.md                 Contexto y decisiones del proyecto
├── docs/                     Modelo, dinámica, rúbrica y tabla de decisiones
├── instances/classic/        Instancias BACP8, BACP10 y BACP12 originales
├── include/dbacp/            Interfaces C++ del lector y del algoritmo
├── src/                      Implementación C++
├── tests/                    Comprobaciones de lectura y factibilidad
├── results/                  Salidas experimentales
└── build/                    Archivos de compilación (generados)
```

## Estado del desarrollo

Están implementados el **lector y validador de instancias `.dat`**, la
**representación vectorial**, la **inicialización greedy aleatorizada con
reparación**, la **búsqueda Max-Min Ant System** y la simulación dinámica.
`min_max` puede mostrar una plantilla, generar planes iniciales con `--init`
o buscar con hormigas y feromonas mediante `--mmas`. Las decisiones se
explican en `docs/representacion.md`, `docs/inicializacion.md` y
`docs/mmas.md`.

## Compilación y uso

Se requiere `g++` con soporte C++17 y `make` en Ubuntu/Linux.

```bash
make
./min_max "instances/classic/bacp8.dat"
./min_max "instances/classic/bacp10.dat"
./min_max "instances/classic/bacp12.dat"
./min_max "instances/classic/bacp8.dat" --period 2 --passed dew100,fis100
./min_max "instances/classic/bacp8.dat" --init --seed 1 --attempts 20 --repair-steps 100 --rcl 3 --eval-budget 92000
./min_max "instances/classic/bacp8.dat" --init --seed 1 --attempts 20 --repair-steps 100 --rcl 3 --eval-budget 92000 --advance-passed ""
./min_max "instances/classic/bacp8.dat" --init --seed 1 --attempts 20 --repair-steps 100 --rcl 3 --eval-budget 92000 --simulate --approval-prob 0.8 --simulation-seed 7
./min_max "instances/classic/bacp8.dat" --mmas --seed 1 --attempts 5 --repair-steps 30 --rcl 3 --init-budget 500 --eval-budget 5000 --ants 5 --alpha 1 --beta 2 --evaporation 0.2 --tau-min-ratio 0.05 --reference-value 17
./min_max "instances/classic/bacp8.dat" --mmas --seed 1 --attempts 5 --repair-steps 30 --rcl 3 --init-budget 500 --eval-budget 5000 --ants 5 --alpha 1 --beta 2 --evaporation 0.2 --tau-min-ratio 0.05 --simulate --approval-prob 0.8 --simulation-seed 7 --period-cap 12
make test
```

También se puede pasar cualquier otra ruta `.dat`. `--period` fija el
período actual (por defecto 1) y `--passed` enumera cursos ya aprobados
separados por coma (por defecto ninguno). Si no se puede leer o validar la
instancia o el estado, el programa imprime el motivo y termina con código
distinto de cero. `./min_max --help` muestra la sintaxis.

Con `--init` deben indicarse **todos** los parámetros del ejemplo por
línea de comandos. El programa imprime cada semestre del mejor plan
factible encontrado, o un diagnóstico si no obtiene ninguno. En la tabla
de decisiones se propone `--eval-budget = 2000 × |U_t|`; para BACP8 en el
estado inicial esto corresponde a 92000 evaluaciones.

`--mmas` usa los mismos parámetros de inicialización y añade los de las
hormigas. `--init-budget` reserva como máximo esa cantidad de evaluaciones
para la inicialización; `--eval-budget` limita la suma de inicialización y
búsqueda. Cada iteración evapora feromonas, refuerza el mejor plan factible
y mantiene las feromonas entre límites. El modo MMAS también puede
combinarse con `--simulate` o `--advance-passed` para replanificar. Véase
`docs/mmas.md` para los detalles y el significado de cada parámetro.

Cada ejecución de `--init` o `--mmas` guarda automáticamente un Markdown
detallado y agrega una fila a `results/resultados.csv`. La consola muestra
la ruta del Markdown. `--results-dir RUTA` permite usar otro directorio.
El reporte incluye los vectores curso–semestre, estados y resultados de
simulación, parámetros, tiempos, evaluaciones y datos de la máquina. Los
campos de valor y gap que no admiten una referencia válida quedan vacíos;
para calcular el gap de una ejecución estática con referencia validada use
`--reference-value N`. Véase [docs/resultados.md](docs/resultados.md).

`--advance-passed` registra los cursos aprobados entre los programados en
el semestre actual y genera otra planificación para `t+1`. Los demás cursos
cursados se consideran reprobados y permanecen pendientes. Para indicar
que todos reprobaron, use `--advance-passed ""`. El programa rechaza
aprobaciones de cursos no programados. Sin `--period-cap`, el horizonte no
se amplía.
Si el prechequeo demuestra una incompatibilidad, muestra la restricción;
si la búsqueda no encuentra plan, muestra las violaciones del mejor intento
y aclara que no se obtuvo una solución factible validada. El código de
salida en ambos casos es `3`.

`--simulate` ejecuta el ciclo completo **planificar → simular aprobaciones →
actualizar estado → replanificar** hasta `T` o hasta que deje de existir un
plan factible validado. Requiere `--approval-prob Q` para fijar una
probabilidad general en `[0,1]` y `--simulation-seed N` para reproducir el
escenario. Puede ajustar cursos concretos con opciones repetidas como
`--course-approval fis100=0.6`. Este modo y `--advance-passed` son
excluyentes. Los resultados académicos de cada par `(curso, semestre)`
quedan fijados por la semilla aunque cambie la planificación; esto permite
compartir un escenario entre métodos. El valor `0.8` es la propuesta
sintética de `docs/tabla_decisiones.pdf`, no una tasa observada.

`--period-cap N` es una extensión experimental para `--mmas` combinado con
`--simulate` o `--advance-passed`. `N` es el **último semestre permitido** y
debe ser mayor que el `T` de la instancia; por ejemplo, BACP8 con cota 12
permite hasta cuatro semestres adicionales. La primera fase conserva las
restricciones originales hasta `T`. Si al llegar a `T` quedan cursos o una
reprobación impide encontrar un plan validado antes de `T`, se activa la fase
adicional. MMAS genera una nueva solución inicial aleatorizada para **todos
los cursos pendientes**, ejecuta la búsqueda y continúa con el mismo ciclo
Bernoulli y replanificación tras cada semestre. Cada nueva búsqueda tiene el
presupuesto indicado por `--eval-budget`.

En la fase adicional se fijan en cero los mínimos de créditos y de cursos
para los períodos todavía disponibles; los máximos y los prerrequisitos se
mantienen. Esto permite planificar incluso un solo curso reprobado. La
relajación se aplica también a períodos anteriores a `T` que aún no hayan
transcurrido si el plan original ya no se pudo validar. Al llegar a `N` con
cursos pendientes, la ejecución termina con código `3`. Esta extensión
modifica el horizonte y los mínimos del modelo de la sección III, por lo que
sus resultados deben identificarse como **escenarios extendidos**, separados
de los resultados clásicos con `T` fijo. Si no se indica `--period-cap`, el
programa conserva el comportamiento original.

El lector almacena los cursos y créditos en el mismo orden del archivo y
convierte cada `<curso, prerrequisito>` en una relación interna
`(prerrequisito, curso)`. Comprueba campos obligatorios, conteos,
identificadores, créditos positivos, límites globales, cursos desconocidos,
ciclos y cadenas de prerrequisitos más largas que el horizonte. Deduplica
relaciones repetidas. Admite los comentarios sin cierre y las comas ausentes
con avisos; los `.dat` originales permanecen intactos.

La validación estructural **no demuestra** que exista una asignación factible
que cumpla simultáneamente todas las restricciones. El inicializador busca
planes factibles y solo los presenta como tales tras validarlos; si no los
encuentra, no afirma que la instancia sea imposible.
