# Búsqueda Max-Min Ant System para DBACP

## Flujo implementado

1. `initialize_plans` obtiene planes greedy aleatorizados y reparados. Su
   presupuesto es `--init-budget`, menor que `--eval-budget`. Si no halla un
   plan factible, las hormigas igualmente pueden buscar, salvo que el
   prechequeo haya demostrado una condición necesaria incumplida.
2. Cada hormiga construye un vector $X_t$ sobre los cursos pendientes.
   Selecciona un curso listo entre los primeros `--rcl` según estrechez de
   ventana, sucesores y créditos. Solo considera semestres que respetan
   sus prerrequisitos y el horizonte restante.
3. Los semestres candidatos se ordenan por exceso de máximos, cambio de
   violaciones y carga proyectada. Al candidato de rango $r$ se le asigna
   $\eta=1/r$. La elección aleatoria usa
   $P(p\mid i)\propto \tau_{i,p}^{\alpha}\eta_{i,p}^{\beta}$, calculada
   mediante logaritmos para evitar desbordamientos. La matriz $\tau$ tiene
   una entrada por decisión `(curso, semestre)`.
4. El plan completo se valida con `evaluate_plan` y se repara si es
   necesario. Solo los planes factibles compiten por el mejor global;
   entre ellos se minimiza primero la carga máxima y luego $E(X_t)$.
5. Al terminar cada iteración se evapora la feromona y **solo el mejor plan
   factible global** deposita $1/B$ en sus decisiones, donde $B$ es su
   carga máxima. Todas las entradas activas se acotan a
   $[\tau_{\min},\tau_{\max}]$.

Se usa una fracción de evaporación $\rho$ indicada por `--evaporation`:
$\tau\leftarrow(1-\rho)\tau+\Delta\tau$. Si existe un mejor plan con
$B>0$, se toma $\tau_{\max}=1/(\rho B)$ y
$\tau_{\min}=\texttt{tau-min-ratio}\cdot\tau_{\max}$, y se inicializan
las feromonas en $\tau_{\max}$. Si la inicialización no encontró un plan
factible, las feromonas comienzan uniformes en 1 hasta que alguna hormiga
encuentre uno. El error cuadrático decide empates al seleccionar el mejor
plan; el depósito usa solo $B$ para preservar una escala positiva simple.
Esta es una adaptación de los mecanismos centrales de
[MMAS de Stützle y Hoos](https://iridia.ulb.ac.be/~stuetzle/publications/Thesis.ThomasStuetzle.pdf)
al vector `(curso, semestre)` del DBACP.

## Parámetros y ejecución

```bash
make
./min_max instances/classic/bacp8.dat --mmas \
  --seed 1 --attempts 5 --repair-steps 30 --rcl 3 \
  --init-budget 500 --eval-budget 5000 \
  --ants 5 --alpha 1 --beta 2 \
  --evaporation 0.2 --tau-min-ratio 0.05
```

- `--ants`: máximo de hormigas por iteración.
- `--alpha`, `--beta`: influencia de feromona y heurística; ambos deben
  estar entre 0 y 100.
- `--evaporation`: fracción evaporada, en $(0,1]$.
- `--tau-min-ratio`: razón $\tau_{\min}/\tau_{\max}$, en $(0,1)$.
- `--init-budget`: evaluaciones máximas para inicialización, estrictamente
  menor que `--eval-budget`.
- `--eval-budget`: máximo **global** de evaluaciones de planes completos.
  Incluye inicialización, planes de hormigas y candidatos examinados en
  reparación. La iteración final puede contener menos de `--ants` hormigas.

`--seed` reproduce la inicialización y la búsqueda. Los valores del ejemplo
son una configuración demostrativa, no parámetros optimizados. La tabla de
decisiones propone como presupuesto comparable $2000|U_t|$ por ejecución;
ese acuerdo todavía debe confirmarse para los experimentos finales.

El modo `--mmas` puede combinarse con `--simulate` o
`--advance-passed`. Cada nuevo estado académico ejecuta MMAS de nuevo y
reinicia sus feromonas, porque cambian $U_t$ y $P_t$. El escenario de
aprobación usa el mismo valor de `--seed` que la búsqueda para que los
resultados académicos sean comparables entre algoritmos.

## Cota experimental de semestres

Con `--period-cap N` se fija un último semestre mayor que el `T` original.
La opción requiere `--mmas` y `--simulate` o `--advance-passed`. Se mantiene
la planificación original mientras tenga un plan factible validado. Si una
reprobación deja sin plan antes de `T`, o quedan pendientes después de `T`,
se activa una fase extendida hasta `N`. `run_mmas` vuelve a llamar a
`initialize_plans` para todos los cursos pendientes y después busca con
hormigas. Los resultados Bernoulli se siguen simulando en cada semestre,
incluidos los adicionales; cada reprobación provoca otro estado y otro MMAS.

La fase extendida permite mínimos de cero créditos y cero cursos por
semestre restante. Se conservan máximos y precedencias. Esta relajación es
necesaria para poder ubicar un único curso reprobado, pero cambia el modelo
original; los resultados con cota deben analizarse por separado. Si el
MMAS original no halla un plan dentro de su presupuesto, también se prueba
la fase extendida; ese hecho no demuestra que el horizonte original fuera
matemáticamente infactible. La cota no garantiza que el estudiante apruebe:
si reprueba hasta `N`, se informa que quedaron cursos pendientes.

## Verificación y límites

Con semilla 1, la configuración anterior y 5000 evaluaciones globales se
obtuvieron planes factibles con carga máxima 17 en BACP8, 14 en BACP10 y
18 en BACP12. Son resultados de una ejecución, no una evaluación
estadística ni una prueba de optimalidad. En BACP12, con una sola
evaluación inicial, la inicialización quedó infactible y la búsqueda MMAS
sí encontró un plan factible dentro del presupuesto.

El valor 18 de BACP12 coincide con la referencia usada en el informe de
Francisco y con resultados publicados para esa instancia. La tabla de
decisiones del proyecto anota 17 y otras publicaciones también lo reportan.
Por ello, esta ejecución permite afirmar que se alcanzó **la referencia 18
del informe**, pero no resuelve por sí sola la discrepancia sobre el óptimo.
La cota inferior de 17 créditos (`204/12`) no garantiza una planificación
factible con ese máximo.

`make test` incluye pruebas de búsqueda posterior a la inicialización,
presupuesto, límites de feromona, reproducibilidad y validación
independiente. MMAS puede agotar el presupuesto sin hallar un plan aunque
exista alguno; en ese caso no declara infactibilidad demostrada. No hay
reinicio por estancamiento ni búsqueda local para mejorar planes ya
factibles; estas son posibles extensiones posteriores.
