# Presentación de avance INF388: DBACP con MMAS

Material para una exposición de **máximo 5 minutos**: unos 4 minutos y 20 segundos para las seis diapositivas y hasta 40 segundos para una demostración o video. La rúbrica pide centrarse en las decisiones de diseño, su justificación, las alternativas descartadas y el objetivo de cada componente; no hace falta explicar el problema desde cero. Los textos «Para decir» son notas del expositor, no contenido obligatorio de la diapositiva.

## 1. Qué está implementado — 20 segundos

**En la diapositiva**

- Solución en **C++17** para Ubuntu, compilada con `make`.
- Flujo: instancia → estado académico → vector de planificación → inicialización → búsqueda MMAS → validación.
- La instancia y todos los parámetros de búsqueda se reciben por línea de comandos.

**Para decir:** «El avance ya genera y evalúa planes válidos. El mismo evaluador se utiliza en la inicialización, la reparación y la búsqueda; así todos los componentes aplican las mismas restricciones».

## 2. Lectura y representación — 40 segundos

**En la diapositiva**

- El lector almacena cursos, créditos, períodos, límites y prerrequisitos de BACP8, BACP10 y BACP12.
- `AcademicState` separa el período actual `t` y los cursos aprobados `A_t` de los datos de la instancia.
- El plan es `X_t = (x_i)_{i∈U_t}`: una posición por curso pendiente y un período asignado por posición.
- Una decisión `(curso, período)` corresponde directamente a una entrada de la matriz de feromonas.

**Para decir:** «El vector garantiza una sola asignación por curso pendiente y permite reconstruir cargas y prerrequisitos. Una matriz binaria curso por período exigiría controlar adicionalmente que cada fila tenga exactamente una asignación. El lector conserva las instancias originales e informa sus irregularidades de formato».

**Código:** [`instance.cpp`](../src/instance.cpp), [`plan.hpp`](../include/dbacp/plan.hpp).

## 3. Restricciones y evaluación — 50 segundos

**En la diapositiva**

| Componente | Objetivo y efecto esperado |
| --- | --- |
| Codificación estructural | Asignar dentro de `t…T` y respetar prerrequisitos al construir o mover cursos; evita candidatos estructuralmente inválidos. |
| Reparación | Mover o intercambiar cursos entre períodos para corregir límites de créditos y cantidad de cursos. |
| Penalización auxiliar | Medir `V_L + V_N` para orientar la reparación de planes todavía infactibles. |
| `evaluate_plan` | Comprobar todas las restricciones de **cada** período y calcular la calidad de planes factibles. |

**Para decir:** «Las restricciones duras son precedencia, horizonte, créditos y cantidad de cursos. Un plan infactible nunca supera a uno factible. Entre planes factibles se minimiza primero la carga máxima y, solo en empate, el error cuadrático de balance. No se mezclan ambos objetivos con pesos arbitrarios».

**Alternativa descartada:** una penalización única sumada al objetivo necesitaría calibrar pesos y podría cambiar la prioridad matemática del modelo.

**Código:** [`plan.cpp`](../src/plan.cpp), [`initialization.cpp`](../src/initialization.cpp).

## 4. Inicialización greedy aleatorizada — 55 segundos

**En la diapositiva**

1. Prechequear condiciones necesarias de factibilidad del estado.
2. Elegir cursos disponibles, priorizando ventanas de períodos estrechas.
3. Seleccionar aleatoriamente entre los mejores candidatos de una lista restringida (`RCL`).
4. Completar, evaluar y reparar el plan; conservar solo soluciones factibles distintas.

**Para decir:** «El criterio greedy dirige la construcción hacia asignaciones prometedoras; la aleatorización aporta diversidad para iniciar MMAS. Los movimientos y los intercambios de reparación conservan horizonte y prerrequisitos. Si se agota el presupuesto sin hallar un plan válido, el programa lo informa sin afirmar que el problema sea matemáticamente infactible».

**Alternativas consideradas:** la asignación completamente aleatoria genera muchas violaciones estructurales; el greedy determinista produce poca diversidad; la búsqueda exacta puede certificar infactibilidad, pero añade un costo y una complejidad mayores para este avance.

**Código:** [`initialization.cpp`](../src/initialization.cpp).

## 5. Búsqueda MMAS y replanificación — 60 segundos

**En la diapositiva**

- Cada hormiga construye `X_t` con probabilidad proporcional a `τ(curso, período)^α · η(curso, período)^β`; `η` proviene del rango heurístico del período.
- Se evapora feromona, se refuerzan las decisiones del mejor plan factible global y se acotan los valores entre `τ_min` y `τ_max`.
- `--eval-budget` limita las evaluaciones totales, incluida la inicialización; la semilla reproduce la búsqueda.
- Tras cada período se registran aprobaciones y reprobaciones, se actualiza `A_t` y se vuelve a planificar `U_t` en el horizonte restante.

**Para decir:** «La feromona favorece decisiones que participaron en buenos planes; la evaporación evita conservarlas indefinidamente. La simulación usa resultados Bernoulli con probabilidades entregadas externamente. Sin una cota de extensión, si una reprobación hace imposible continuar dentro de `T`, el programa lo diagnostica. La opción experimental `--period-cap` permite seguir replanificando hasta un semestre máximo indicado por CLI».

**Código:** [`mmas.cpp`](../src/mmas.cpp), [`simulation.cpp`](../src/simulation.cpp), [`main.cpp`](../src/main.cpp).

## 6. Evidencia y cierre — 35 segundos

**En la diapositiva**

| Instancia | Mejor carga en una inicialización de ejemplo | Mejor carga en una ejecución MMAS de ejemplo |
| --- | ---: | ---: |
| BACP8 | 18 | 17 |
| BACP10 | 16 | 14 |
| BACP12 | 21 | 18 |

- `make test`: pasan las cinco suites (lector, plan, inicialización, simulación y MMAS).
- Compilación verificada en Ubuntu con `g++` y `-std=c++17`.
- Resultados **demostrativos**: las dos columnas usan configuraciones y presupuestos distintos; no constituyen una comparación controlada ni prueban optimalidad.

**Para decir:** «El avance cubre los componentes exigidos y además una búsqueda funcional y su evaluación. El siguiente trabajo es acordar escenarios, automatizar las ejecuciones y analizar resultados reproducibles. En BACP12 se mantiene abierta la discrepancia entre referencias de 17 y 18 créditos».

## Demostración breve o video acelerado

La rúbrica **sugiere** un video acelerado de ejecución. Si se usa, mostrar solo la compilación, las pruebas y el resumen de un plan MMAS; reservar el resto del tiempo para explicar decisiones.

```bash
make
make test
./min_max instances/classic/bacp8.dat --mmas \
  --seed 1 --attempts 5 --repair-steps 30 --rcl 3 \
  --init-budget 500 --eval-budget 5000 \
  --ants 5 --alpha 1 --beta 2 \
  --evaporation 0.2 --tau-min-ratio 0.05
```

En la salida, señalar `MMAS t=1`, el número de evaluaciones, `Mejor plan MMAS: carga máxima=17` y las cargas de los ocho semestres. Los parámetros de este comando son **demostrativos**, no una configuración optimizada. La simulación dinámica puede mostrarse aparte si hay tiempo:

```bash
./min_max instances/classic/bacp8.dat --mmas \
  --seed 1 --attempts 5 --repair-steps 30 --rcl 3 \
  --init-budget 500 --eval-budget 5000 \
  --ants 5 --alpha 1 --beta 2 \
  --evaporation 0.2 --tau-min-ratio 0.05 \
  --simulate --approval-prob 0.8 --simulation-seed 7
```

## Referencias internas para preguntas

- Formulación: [`Valencia-Francisco.pdf`](Valencia-Francisco.pdf), sección III, páginas 3–4.
- Criterios del hito: [`Rubrica_PresentacionAvance_INF388.pdf`](Rubrica_PresentacionAvance_INF388.pdf).
- Diseño detallado: [`representacion.md`](representacion.md), [`inicializacion.md`](inicializacion.md), [`mmas.md`](mmas.md) y [`parte_dinamica.md`](parte_dinamica.md).
- Los mínimos de créditos y cursos se aplican a **todos** los períodos restantes. El estado inicial y las probabilidades de aprobación se entregan por CLI; los archivos `.dat` clásicos no los contienen.
