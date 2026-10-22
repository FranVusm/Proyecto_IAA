# Guion oral — Presentación avance 1 DBACP con MMAS

**Expositor:** Francisco Valencia Salgado  
**Duración objetivo:** 4 min 40 s; quedan 20 s de margen frente al máximo de 5 min.  
**Material:** portada y seis diapositivas de `Presentation_Template_USM/main.tex`.

El texto entre comillas es para decirlo en voz alta. Los tiempos son una guía
para ensayar; no incluyen una demostración en vivo.

## Portada — 0:00 a 0:10

> «Buenas tardes. Soy Francisco Valencia y presentaré el avance 1 de DBACP
> con Max-Min Ant System».

## 1. Flujo del algoritmo — 0:10 a 0:35

> «Leemos la instancia y, con el semestre actual y las aprobaciones, creamos
> el estado académico. Después preparamos una plantilla con los cursos
> pendientes. La inicialización les asigna semestres y genera una solución
> X sub t. MMAS intenta mejorarla; tras cada semestre actualizamos las
> aprobaciones y replanificamos».

## 2. Lectura y representación — 0:35 a 1:15

> «Usamos BACP8, BACP10 y BACP12. El lector extrae cursos, créditos,
> semestres, límites y prerrequisitos. `passed[i]` indica si un curso está
> aprobado. El plan tiene una posición por curso de la instancia:
> `assigned_periods[i]` guarda directamente el semestre del curso `i`. Si
> `passed[i]` vale uno, esa posición queda en cero y no forma parte de
> $X_t$; durante la construcción, cero también significa pendiente sin
> asignar. Así agrupamos cursos y sumamos créditos por período. Una matriz
> binaria exigiría verificar una asignación única por cada curso pendiente».

## 3. Restricciones y evaluación — 1:15 a 2:05

> «Exigimos una asignación por curso dentro del horizonte, prerrequisitos
> antes de sus dependientes y límites de créditos y cantidad en cada
> semestre. La construcción respeta las dos primeras; para las otras usa
> una heurística y la reparación corrige violaciones con movimientos o
> intercambios. El evaluador valida el plan completo. Entre planes factibles
> minimizamos la carga máxima. Solo si hay empate usamos E de X sub t: la
> suma de desviaciones cuadráticas respecto del promedio. Descartamos
> combinar objetivo y violaciones en una suma ponderada porque alteraría la
> prioridad del modelo».

## 4. Inicialización greedy aleatorizada — 2:05 a 3:00

> «Preparamos una plantilla con semestres sin asignar, comprobamos condiciones
> necesarias y calculamos ventanas por prerrequisitos. Una lista restringida
> permite sortear cursos disponibles y semestres prometedores. Repetimos,
> evaluamos y reparamos para obtener planes factibles distintos. Elegimos
> greedy aleatorizado: el azar puro genera muchas violaciones; el greedy
> determinista aporta poca diversidad. Del mejor plan inicial obtenemos M,
> su carga máxima. Tau máximo es el inverso del producto de M por la
> evaporación rho; tau mínimo es gamma veces ese valor. Las feromonas parten
> iguales; el plan inicial queda como
> referencia y las hormigas construirán planes nuevos».

## 5. Búsqueda MMAS y replanificación — 3:00 a 4:05

> «Cada hormiga construye un plan desde cero. Para elegir semestre combina
> feromona y rango heurístico. Evaluamos y reparamos su plan; si mejora al
> mejor factible global, lo conservamos. Al terminar la iteración evaporamos
> feromona y reforzamos las asignaciones de ese mejor plan, manteniendo los
> límites tau mínimo y tau máximo. El presupuesto cuenta inicialización y
> búsqueda. Después de cursar cada semestre, Bernoulli determina
> aprobaciones; los reprobados siguen pendientes y ejecutamos MMAS de nuevo.
> La cota opcional de semestres es una extensión experimental: conserva
> máximos y prerrequisitos, pero relaja los mínimos en la fase adicional».

## 6. Evidencia y cierre — 4:05 a 4:40

> «Las cargas de ejemplo son 18 y 17 para BACP8, 16 y 14 para BACP10, y 21
> y 18 para BACP12, comparando inicialización y MMAS. Como se usaron
> presupuestos distintos, estos números no son una comparación controlada
> ni prueban optimalidad. Sigue pendiente acordar escenarios y repetir
> experimentos. En BACP12 declaramos la discrepancia entre referencias 17
> y 18».

## Recordatorios para el ensayo

- Señalar la fila o fórmula correspondiente mientras se explica; no leer
  símbolos uno por uno.
- Mantener la explicación del problema al mínimo, como pide la pauta.
- Si se habla de la extensión `--period-cap`, distinguirla del modelo
  original, cuyo horizonte termina en `T`.
- Las cifras de la última diapositiva son demostrativas; no presentar las
  diferencias entre columnas como una comparación experimental concluida.
