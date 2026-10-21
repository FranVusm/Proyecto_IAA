# Representación de una solución para DBACP y MMAS

La solución sigue el modelo del informe: en el estado académico actual
$(A_t,t)$, el conjunto de cursos pendientes es $U_t=C\setminus A_t$ y el
plan es el vector $X_t=(x_i)_{i\in U_t}$.

## Cómo está implementada

- `AcademicState` guarda el período actual y qué cursos están aprobados.
- `Plan::course_indices[k]` identifica el curso pendiente de la posición
  `k`. Las posiciones siguen el orden de `courses` en la instancia `.dat`.
- `Plan::assigned_periods[k]` guarda el período $x_i$ asignado a ese curso.
  El valor `0` significa *sin asignar* mientras una hormiga construye el
  plan. En una solución terminada, todos los valores deben estar en
  $P_t=\{t,\ldots,T\}$.

Ejemplo: si `course_indices = [0, 2, 4]` y
`assigned_periods = [1, 2, 3]`, los tres cursos pendientes de esas
posiciones se cursarán respectivamente en los períodos 1, 2 y 3. Los
cursos ya aprobados no aparecen en $X_t$.

## Relación con Max-Min Ant System

Una hormiga completa el vector eligiendo el período de cada curso
pendiente. Esta estructura permite asociar una feromona a cada decisión
`(curso, período)`, consultar la carga parcial y verificar restricciones
durante la construcción. Al replanificar, se crea un **nuevo vector** para
los cursos que permanezcan pendientes. La política de feromonas y la
construcción guiada por ellas están descritas en `mmas.md`.

Se eligió el vector porque corresponde exactamente a $X_t$ y cada curso
pendiente tiene una sola posición. Una matriz binaria curso-período
necesitaría comprobar adicionalmente que cada curso tenga exactamente una
asignación.

## Evaluación disponible

`evaluate_plan` comprueba que el vector corresponda a $U_t$, que todas
las asignaciones estén en el horizonte, que los prerrequisitos pendientes
vayan antes de sus cursos dependientes y que créditos y cantidad de cursos
respeten los límites de **cada** período. Para planes factibles calcula:

1. `maximum_load` $=\max_{p\in P_t}L_p(X_t)$.
2. `squared_balance_error` $=\sum_{p\in P_t}(L_p(X_t)-AVG_t)^2$.

La comparación es lexicográfica. En un mismo estado, el desempate se
calcula internamente con $\sum_p L_p^2$, equivalente a ordenar por el error
cuadrático y sin problemas de redondeo.

La plantilla que muestra `min_max` sin `--init` aún contiene ceros: **no es
una solución factible**. La inicialización greedy aleatorizada con
reparación ya puede generar planes completos y validarlos; véase
`inicializacion.md`. La búsqueda guiada por feromonas de MMAS se ejecuta
con `--mmas` y se documenta en `mmas.md`.
