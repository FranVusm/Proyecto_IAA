# Contexto del proyecto: DBACP con Max-Min Ant System

## Fuente de verdad

La formulación matemática elegida por Francisco Valencia está en
`docs/Valencia-Francisco.pdf`, sección III, páginas 3 y 4. Antes de modificar el
modelo, contrastar las decisiones con ese documento. La técnica de solución
elegida para este proyecto es **Max-Min Ant System (MMAS)**.

La entrega de avance se rige por `docs/Rubrica_PresentacionAvance_INF388.pdf`.
Su cumplimiento formal exige **C/C++ compilable en Ubuntu** y recibir la
instancia y los parámetros por línea de comandos; no fijarlos en el código.

`docs/modelo_matematico.md` resume la formulación y `docs/parte_dinamica.md` fija el
ciclo de cuatro pasos solicitado por el usuario para la simulación dinámica.

`docs/tabla_decisiones.pdf` contiene las decisiones experimentales que el usuario
pidió seguir paso a paso. Varios elementos están rotulados allí como
**propuestos** o **pendientes de acuerdo**; conservar esa distinción al
planificar los experimentos finales.
`restricciones.md` describe el enfoque híbrido de restricciones: codificación
estructural, reparación de cargas/cantidades y penalización auxiliar.

El proyecto usa C++17 y `make`. `./min_max "ruta/instancia.dat"` ya lee y
valida una instancia; `make test` verifica las tres instancias clásicas y
casos inválidos. El lector está en `src/instance.cpp` y su interfaz en
`include/dbacp/instance.hpp`. `include/dbacp/plan.hpp` y `src/plan.cpp`
implementan el estado académico, el vector $X_t$ y su evaluación. La
validación de la instancia es estructural y aplica condiciones necesarias de
factibilidad. `include/dbacp/initialization.hpp` y `src/initialization.cpp`
implementan la inicialización **greedy aleatorizada con reparación**, elegida
explícitamente por el usuario. `include/dbacp/mmas.hpp` y `src/mmas.cpp`
implementan la búsqueda MMAS: hormigas con feromona `(curso, período)`,
evaporación, refuerzo del mejor plan y límites. Véanse
`docs/representacion.md`, `docs/inicializacion.md` y `docs/mmas.md` para
las decisiones de diseño.
`include/dbacp/report.hpp` y `src/report.cpp` generan un Markdown por
ejecución y agregan una fila a `results/resultados.csv` cuando se ejecuta
`--init` o `--mmas`; el formato y las limitaciones se explican en
`docs/resultados.md`.
`advance_state` actualiza el estado con aprobaciones de los cursos
programados en el período actual; `--advance-passed` ejecuta un avance con
resultados explícitos. `simulate_semester` simula resultados Bernoulli con
un vector de probabilidades externo; `--simulate --approval-prob Q
--simulation-seed N` recorre el horizonte, con `--course-approval id=Q`
para ajustar cursos. La semilla fija sorteos por `(curso, período)` para
compartir escenarios entre algoritmos. La independencia entre resultados
es una decisión de simulación que el informe no impone.
La variante experimental `--period-cap N` (`N>T`) permite continuar con
MMAS si quedan cursos pendientes al terminar `T` o si no se valida un plan
tras una reprobación anterior. Se vuelve a inicializar y buscar en todos los
períodos restantes hasta `N`, manteniendo Bernoulli y replanificación en
cada período. En esta fase se relajan a cero los mínimos de créditos y de
cantidad de cursos; se conservan máximos y prerrequisitos. Esta variante
modifica el modelo original y debe reportarse por separado. Sin la opción,
`T` sigue siendo el horizonte final.

## Modelo matemático elegido

- `C`: cursos del plan; `P = {1, ..., T}`: períodos académicos; `r_i`:
  créditos del curso `i`; `R`: pares `(i, j)` donde `i` es prerrequisito de
  `j`.
- En el período actual `t`, `A_t` son los cursos ya aprobados,
  `U_t = C \ A_t` los pendientes y `P_t = {t, ..., T}` el horizonte restante.
- Para cada `i ∈ U_t`, `x_i ∈ P_t` indica su período asignado. La solución
  `X_t = (x_i)_{i∈U_t}` asigna exactamente una vez cada curso pendiente.
- `L_p(X_t) = Σ_{i∈U_t} r_i · 1[x_i=p]` es la carga de créditos;
  `N_p(X_t) = Σ_{i∈U_t} 1[x_i=p]` es el número de cursos.
- Objetivo principal: minimizar `max_{p∈P_t} L_p(X_t)`.
- Desempate secundario: minimizar
  `E(X_t) = Σ_{p∈P_t}(L_p(X_t) - AVG_t)^2`, donde
  `AVG_t = (Σ_{i∈U_t} r_i) / |P_t|`. Comparar soluciones de forma
  lexicográfica: primero la carga máxima y luego `E`. No reemplazarlo por
  una suma ponderada salvo que Francisco cambie el modelo.
- Restricciones para cada `p ∈ P_t`:
  `L_min ≤ L_p(X_t) ≤ L_max` y `N_min ≤ N_p(X_t) ≤ N_max`.
- Para `(i, j) ∈ R` con ambos cursos pendientes: `x_i < x_j`. Si `i` ya
  está aprobado, ese prerrequisito se considera satisfecho.

## Dinámica entre períodos

1. En `t` se cursan `K_t = {i ∈ U_t : x_i = t}`.
2. Cada curso cursado tiene probabilidad de aprobación `q_i ∈ [0, 1]` y
   resultado `Y_it ~ Bernoulli(q_i)`. El PDF no especifica independencia
   entre resultados; evitar asumirla como parte del modelo.
3. `Pass_t = {i ∈ K_t : Y_it = 1}`;
   `A_{t+1} = A_t ∪ Pass_t`;
   `U_{t+1} = C \ A_{t+1}`;
   `P_{t+1} = {t+1, ..., T}`.
4. Los cursos reprobados continúan pendientes y se obtiene una solución
   nueva `X_{t+1}`. La planificación es una secuencia de optimizaciones,
   no un único calendario estático.

## Criterios para implementar MMAS

- Construir asignaciones de cursos a períodos que respeten el modelo o
  detectar y reportar explícitamente cuando no exista solución factible.
- Evaluar soluciones factibles con el orden lexicográfico definido arriba.
- Mantener la distinción entre datos del problema, estado académico del
  estudiante, plan propuesto y resultados observados al cursar.
- El MMAS implementado usa feromonas acotadas, heurística de rangos,
  refuerzo del mejor plan global, evaporación y presupuesto total de
  evaluaciones. Los parámetros se entregan por CLI. El PDF no fija sus
  valores; no atribuirle decisiones de implementación no especificadas.

## Límites y casos que requieren atención

- El horizonte termina en `T`. Una reprobación puede volver imposible la
  replanificación dentro de los períodos restantes; esto debe detectarse.
- Los mínimos de carga y de número de cursos se aplican en el PDF a **cada**
  período restante. Pueden volver infactible un estado con pocos cursos;
  cualquier relajación sería una decisión nueva del modelo.
- El texto de estado del arte menciona períodos en que se ofrecen cursos,
  pero la sección III no incluye una restricción formal de disponibilidad.
  No agregarla sin decidirlo explícitamente.

## Hito de presentación de avance INF388

- Desarrollo (60 % de la nota): leer e interpretar una instancia (20 puntos
  del bloque); definir e implementar la representación (30); implementar
  un método coherente para manejar restricciones (25); inicializar el
  algoritmo generando soluciones válidas (25).
- Presentación (40 %): máximo 5 minutos, centrada en decisiones de diseño,
  justificación, alternativas descartadas y objetivo/efecto de cada
  componente. No es necesario explicar el problema; al final revisan el
  código. La rúbrica sugiere un video acelerado de la ejecución.
- Bonificación posible: 5 puntos por obtener soluciones, 5 por componentes
  de búsqueda posteriores a la inicialización y 5 por la función de
  evaluación. El código actual incluye los tres elementos; la asignación
  de puntos corresponde al docente.
- Otros requisitos formales: mantener el código ordenado y disponible para
  revisión. La rúbrica indica descuentos por incumplimiento: 20 puntos por
  cada requisito de C/C++, línea de comandos o tiempo; 5 por código no
  disponible/ordenado.

## Instancias y tabla de decisiones

- Instancias clásicas: `instances/classic/bacp8.dat` (46 cursos, 8 períodos,
  133 créditos), `instances/classic/bacp10.dat` (42, 10, 134) y
  `instances/classic/bacp12.dat` (66, 12, 204). En las tres,
  `p=T`, `a=L_min=10`, `b=L_max=24`, `c=N_min=2`, `d=N_max=10`.
  `courses` identifica los cursos y `credit` da los créditos en el mismo
  orden. `prereq` usa `<curso_dependiente, prerrequisito>`, que se debe
  invertir al construir los pares `(prerrequisito, dependiente)` de `R`.
  Estos `.dat` describen el BACP clásico; no contienen `A_t`, `t` ni `q_i`.
  Los escenarios DBACP deben añadir ese estado sin alterar el contenido
  base de las instancias.
- Los archivos originales tienen irregularidades de formato: `bacp10.dat`
  y `bacp12.dat` contienen un comentario `/*` sin cierre después de `d=10`;
  `bacp12.dat` omite tres comas entre pares de prerrequisitos; `bacp8.dat`
  repite cinco relaciones. El lector debe admitir este formato de forma
  explícita, validar conteos y deduplicar relaciones. No alterar los
  originales para ocultar estos problemas.
- Referencias para las instancias clásicas: BACP8 = 17 y BACP10 = 14.
  Francisco usa BACP12 = 18 como referencia, en concordancia con su informe
  y estudios que reportan ese valor. `docs/tabla_decisiones.pdf`, en cambio,
  consigna 17 para BACP12; también existen publicaciones que reportan 17.
  La cota `204/12 = 17` por sí sola no demuestra que una planificación con
  carga máxima 17 sea factible. Hasta validar una asignación de 17 sobre esta
  instancia o disponer de una certificación exacta de imposibilidad, registrar
  18 como referencia del proyecto y dejar explícita la discrepancia; no
  afirmar que la optimalidad quedó demostrada por la ejecución de MMAS.
- Formato compartido de solución: vector `X_t` con identificadores de curso;
  validar independientemente prerrequisitos, créditos, cantidad de cursos y
  objetivo. Todos los métodos deben obtener el mismo diagnóstico y valor.
- Criterio principal de término propuesto: `B = 2000 |U_t|` evaluaciones
  de la función objetivo por ejecución. Cada evaluación de una candidata
  cuenta una vez, incluso si se calcula incrementalmente. Registrar también
  el tiempo. La tabla indica que el grupo aún debe acordar estos detalles.
- Comparación propuesta: 20 ejecuciones independientes con semillas 1–20;
  para dinámica, compartir los resultados aleatorios de aprobación. Separar
  escenarios dinámicos para configuración (40 %) y evaluación final (60 %),
  sin usar evaluación final para ajustar parámetros. Configurar por búsqueda
  aleatoria inicial de 20 configuraciones con 5 ejecuciones cada una.
- Para escenarios dinámicos sintéticos se propone `q_i = 0,8` para todo
  curso. Registrar `A_t`, `t` y resultados académicos aleatorios. Cada estado
  dinámico requiere su propio óptimo o BKS validado; no reutilizar los
  valores clásicos. Si falta referencia fiable, dejar el gap sin calcular.
- Resultados finales propuestos: `resultados.csv` con una fila por ejecución
  y columnas `algoritmo,instancia,semilla,valor,factible,gap,tiempo_s,evaluaciones`;
  coma como separador, punto decimal y factibilidad 1/0. Ejecutar de
  preferencia en una misma máquina Ubuntu y registrar CPU, RAM, sistema,
  compilador e hilos. La tabla propone intercambio el 12-11-2026 y entrega
  final el 19-11-2026.
