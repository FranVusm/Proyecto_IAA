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
- El plan matemático es `X_t = (x_i)_{i∈U_t}`. En el código, `assigned_periods[i]` tiene una posición fija por curso de la instancia; para `i∈U_t` guarda `x_i`, y los aprobados conservan 0.
- Una decisión `(curso, período)` corresponde directamente a una entrada de la matriz de feromonas.

**Para decir:** «El vector garantiza una sola asignación por curso pendiente y permite reconstruir cargas y prerrequisitos. Una matriz binaria curso por período exigiría controlar adicionalmente que cada fila tenga exactamente una asignación. El lector conserva las instancias originales e informa sus irregularidades de formato».

**Código:** [`instance.cpp`](../src/instance.cpp), [`plan.hpp`](../include/dbacp/plan.hpp).

## 3. Restricciones y evaluación — 50 segundos

**En la diapositiva**

| Restricción | Cómo se cumple |
| --- | --- |
| Asignación única y horizonte | El vector contiene una posición fija por curso; `evaluate_plan` exige un semestre entre `t` y `T` para cada pendiente y 0 para cada aprobado. |
| Prerrequisitos | La construcción usa períodos posteriores a los requisitos; los cambios se validan antes de aceptarse. |
| Créditos por semestre | La heurística considera `L_min ≤ L_p ≤ L_max`; reparación y evaluación final corrigen y verifican cada período. |
| Cursos por semestre | Se aplica el mismo proceso a `N_min ≤ N_p ≤ N_max`. |

**Para decir:** «Primero se exige que el plan sea factible. Los prerrequisitos y el horizonte se controlan al construir; créditos y cantidad se consideran al elegir y pueden corregirse mediante reparación. Entre planes factibles se minimiza la mayor carga de créditos. Solo si dos tienen la misma carga máxima, se compara la medida secundaria `E(X_t)`, la suma de desviaciones cuadráticas respecto de los créditos promedio por período. Durante la reparación, `V_L + V_N` orienta planes aún infactibles; no se añade al objetivo final».

**Alternativa descartada:** una penalización única sumada al objetivo necesitaría calibrar pesos y podría cambiar la prioridad matemática del modelo.

**Código:** [`plan.cpp`](../src/plan.cpp), [`initialization.cpp`](../src/initialization.cpp).

## 4. Inicialización greedy aleatorizada — 55 segundos

**En la diapositiva**

1. Crear el vector indexado por curso con ceros; `passed[i]` distingue aprobados de pendientes aún sin asignar.
2. Prechequear condiciones necesarias y calcular ventanas de períodos según prerrequisitos.
3. Elegir cursos disponibles de una lista restringida (`RCL`) y sortear un período bien clasificado por violaciones y carga.
4. Completar, evaluar y reparar varios planes; conservar solo soluciones factibles distintas.
5. Elegir el mejor plan inicial, con carga máxima `M`, para calcular `τ_max = 1/(ρM)` y `τ_min = tau_min_ratio · τ_max`. Todas las feromonas empiezan en `τ_max`.

**Para decir:** «El criterio greedy dirige la construcción hacia asignaciones prometedoras y el azar genera varios planes. MMAS guarda el mejor inicial y usa su carga máxima para fijar los límites de feromona. Cada hormiga construye después un plan nuevo desde cero; la primera iteración todavía parte de feromonas uniformes. Si se agota el presupuesto sin hallar un plan válido, eso no demuestra infactibilidad matemática».

**Alternativas consideradas:** la asignación completamente aleatoria genera muchas violaciones estructurales; el greedy determinista produce poca diversidad; la búsqueda exacta puede certificar infactibilidad, pero añade un costo y una complejidad mayores para este avance.

**Código:** [`initialization.cpp`](../src/initialization.cpp), [`mmas.cpp`](../src/mmas.cpp).

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
  --simulate --approval-prob 0.8
```

## Referencias internas para preguntas

- Formulación: [`Valencia-Francisco.pdf`](Valencia-Francisco.pdf), sección III, páginas 3–4.
- Criterios del hito: [`Rubrica_PresentacionAvance_INF388.pdf`](Rubrica_PresentacionAvance_INF388.pdf).
- Diseño detallado: [`representacion.md`](representacion.md), [`inicializacion.md`](inicializacion.md), [`mmas.md`](mmas.md) y [`parte_dinamica.md`](parte_dinamica.md).
- Los mínimos de créditos y cursos se aplican a **todos** los períodos restantes. El estado inicial y las probabilidades de aprobación se entregan por CLI; los archivos `.dat` clásicos no los contienen.
