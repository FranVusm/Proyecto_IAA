# Inicialización de soluciones para DBACP con MMAS

## Alternativas evaluadas

| Método | Ventaja | Costo o límite |
| --- | --- | --- |
| Asignación aleatoria seguida de reparación | Muy simple y diversa. | Produce muchas violaciones estructurales; repararlas es difícil cuando hay cadenas de prerrequisitos. |
| Greedy determinista con reparación | Fácil de explicar y rápido. | Tiende a producir planes parecidos; aporta poca diversidad para iniciar MMAS. |
| **Greedy aleatorizado con reparación** | Mantiene prerrequisitos, explora varios planes y reutiliza la misma decisión `(curso, período)` que usarán las hormigas. | Puede quedar atrapado en un plan infactible o de calidad modesta; necesita varios intentos. |
| Retroceso exhaustivo o programación exacta | Puede probar factibilidad o infactibilidad si termina la búsqueda completa. | El costo puede crecer mucho y añade complejidad al avance actual. |

Se eligió **greedy aleatorizado con reparación** por indicación de
Francisco. Esta elección es un diseño para el proyecto, no una conclusión
experimental de superioridad universal. El modelo de BACP ya se ha tratado
con [ACO](https://doi.org/10.1155/2013/793671), y [MMAS](https://iridia.ulb.ac.be/~stuetzle/pub.html)
construye soluciones a partir de decisiones guiadas por feromonas. La
inicialización greedy prepara esas mismas decisiones sin feromonas; la
búsqueda posterior se describe en `mmas.md`.

## Procedimiento implementado

1. Se parte del estado $(A_t,t)$ y de un vector indexado por curso, con
   valor inicial cero. Solo las posiciones de $U_t$ se asignan; `passed[i]`
   permite distinguir aprobados de pendientes sin asignar. Un prechequeo detecta imposibilidades evidentes por créditos,
   cantidad de cursos, horizonte y cadenas de prerrequisitos.
2. Para cada curso pendiente se calcula su semestre más temprano y más
   tardío posible según los prerrequisitos pendientes. Durante la
   construcción solo se consideran cursos cuyos prerrequisitos ya fueron
   asignados.
3. Se priorizan cursos con ventana más estrecha. Una **lista restringida de
   candidatos** (RCL) permite elegir aleatoriamente entre los primeros
   cursos. Para cada curso, se ordenan los períodos válidos por exceso de
   límites, reducción de violaciones y carga proyectada; se elige al azar
   dentro de la RCL correspondiente.
4. El plan completo se evalúa con la función central `evaluate_plan`. Si
   incumple límites de créditos o número de cursos, se intenta repararlo
   mediante movimientos de cursos entre semestres afectados. Cada curso se
   mueve solo dentro del rango que permiten sus prerrequisitos y sucesores;
   este rango se recalcula después de cada cambio. Cuando no hay un
   movimiento que mejore, se prueban intercambios validados completos.
   Cada cambio aceptado mantiene horizonte y prerrequisitos, y reduce la
   violación total o mejora el desempate.
5. Se repite la construcción hasta agotar los intentos o el presupuesto de
   evaluaciones. Solo los planes **completamente factibles y distintos**
   entran a la población inicial. Se conserva además el mejor intento
   infactible para diagnosticar fallos.

La penalización auxiliar es $V_L+V_N$, donde $V_L$ y $V_N$ son exactamente
las magnitudes definidas en `restricciones.md`. No se suma una constante
grande a la función objetivo: entre planes factibles se compara primero
$\max L_p$ y luego $E(X_t)$; los infactibles no compiten con ellos.
La comparación común `better_plan` rechaza planes parciales o con errores
estructurales, prioriza factibilidad, luego $V_L+V_N$ para infactibles, y
aplica el objetivo lexicográfico a los factibles. `evaluate_plan` detalla
el valor observado y los límites en cada semestre que incumple reglas.

## Uso

Todos los parámetros de inicialización se entregan por línea de comandos:

```bash
make
./min_max "instances/classic/bacp8.dat" --init \
  --seed 1 --attempts 20 --repair-steps 100 --rcl 3 --eval-budget 92000
```

`--seed` controla el azar; `--attempts` es el máximo de construcciones;
`--repair-steps` limita cambios aceptados por plan; `--rcl` determina
cuántos candidatos mejor clasificados pueden elegirse; `--eval-budget`
limita las evaluaciones de planes completos. Cada candidata completa
evaluada durante reparación cuenta una vez, conforme al criterio propuesto
en `tabla_decisiones.pdf`. También se aceptan `--period` y `--passed` para
replanificar desde un estado académico.

## Resultados iniciales y límites

En una verificación con semilla 1, 20 intentos, 100 pasos de reparación y
RCL 3 se obtuvieron 20 planes factibles distintos en BACP8 y BACP10, y 16
en BACP12. Las mejores cargas máximas observadas fueron 18 en BACP8, 16 en
BACP10 y 21 en BACP12. Son resultados de inicialización, no
resultados finales del MMAS ni un estudio estadístico.

La reparación solo acepta mejoras estrictas, así que puede detenerse en un
óptimo local infactible. Un prechequeo fallido demuestra que el estado viola
una condición necesaria; si se agota el presupuesto sin hallar plan, el
programa informa **no encontrado** y no afirma que el problema sea
infactible. La búsqueda MMAS posterior reutiliza esta inicialización;
véase `mmas.md`.

Las instancias `.dat` no incluyen probabilidades $q_i$. La transición
`advance_state` recibe las aprobaciones de los cursos cursados; pueden ser
resultados observados o los generados por `simulate_semester` con
probabilidades y semilla indicadas por línea de comandos. Una reprobación
en el último semestre deja el curso pendiente y se informa que terminó el
horizonte.
