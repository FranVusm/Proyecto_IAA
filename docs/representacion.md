# Representación de una solución para DBACP y MMAS

La solución sigue el modelo del informe: en el estado académico actual
$(A_t,t)$, el conjunto de cursos pendientes es $U_t=C\setminus A_t$ y el
plan es el vector $X_t=(x_i)_{i\in U_t}$.

## Cómo está implementada

- `AcademicState` guarda el período actual y qué cursos están aprobados.
- `Plan::assigned_periods[i]` es la única estructura de asignación: la
  posición `i` identifica directamente `Instance::courses[i]` y guarda
  su período $x_i$ si el curso está pendiente.
- El vector almacenado tiene $|C|$ posiciones estables. En las posiciones
  de cursos aprobados se guarda `0`; en cursos pendientes, `0` significa
  *aún sin asignar* durante la construcción. `AcademicState::passed[i]`
  distingue los dos casos. Un plan completo tiene valores en
  $P_t=\{t,\ldots,T\}$ para todos los pendientes y `0` para los aprobados.

Ejemplo: con cinco cursos y el curso 1 aprobado,
`passed = [0, 1, 0, 0, 0]` y `assigned_periods = [1, 0, 2, 3, 3]`.
Así, el curso 2 queda en el período 2 y el curso 4 en el 3. La posición
1 existe en memoria, pero no pertenece al plan matemático $X_t$, que
incluye solo los cursos pendientes.

## Relación con Max-Min Ant System

Una hormiga completa el vector eligiendo el período de cada curso
pendiente en su posición fija. Esta estructura permite asociar una feromona a cada decisión
`(curso, período)`, consultar la carga parcial y verificar restricciones
durante la construcción. Al replanificar, se crea un **nuevo vector** del
mismo tamaño; los cursos aprobados permanecen en cero y los pendientes
se vuelven a asignar. La política de feromonas y la
construcción guiada por ellas están descritas en `mmas.md`.

Se eligió el vector porque cada curso tiene una posición estable y cada
pendiente recibe una sola asignación. Una matriz binaria curso-período
necesitaría comprobar adicionalmente que cada curso tenga exactamente una
asignación.

## Evaluación disponible

`evaluate_plan` comprueba el tamaño del vector, los ceros de los aprobados,
que todos los pendientes tengan asignaciones en el horizonte y que los prerrequisitos pendientes
vayan antes de sus cursos dependientes y que créditos y cantidad de cursos
respeten los límites de **cada** período. Para planes factibles calcula:

1. `maximum_load` $=\max_{p\in P_t}L_p(X_t)$.
2. `squared_balance_error` $=\sum_{p\in P_t}(L_p(X_t)-AVG_t)^2$.

La comparación es lexicográfica. En un mismo estado, el desempate se
calcula internamente con $\sum_p L_p^2$, equivalente a ordenar por el error
cuadrático y sin problemas de redondeo.

La plantilla que muestra `min_max` sin `--init` aún contiene ceros en cursos
pendientes: **no es una solución factible**. La inicialización greedy aleatorizada con
reparación ya puede generar planes completos y validarlos; véase
`inicializacion.md`. La búsqueda guiada por feromonas de MMAS se ejecuta
con `--mmas` y se documenta en `mmas.md`.
