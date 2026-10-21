# Reportes de ejecución

Cada ejecución de `--init` o `--mmas` genera automáticamente en `results/`:

1. Un archivo Markdown único con los datos y todas las soluciones de esa
   ejecución.
2. Una fila en `resultados.csv` con las columnas propuestas por
   `tabla_decisiones.pdf`:
   `algoritmo,instancia,semilla,valor,factible,gap,tiempo_s,evaluaciones`.

`--results-dir RUTA` cambia el directorio de salida. Al terminar, la consola
imprime la ruta del Markdown. Si la ejecución no encuentra plan o termina
con cursos pendientes al llegar a la cota, también queda registrada con
`factible=0` y sus diagnósticos. No se generan reportes para una lectura de
instancia sin algoritmo ni para argumentos rechazados antes de iniciar.

## Contenido del Markdown

- Identificación de la instancia, horizonte original, cota opcional, estado
  inicial y parámetros efectivos del algoritmo.
- Semilla de búsqueda, semilla del escenario y probabilidades `q_i` por curso
  cuando se simula.
- Sistema operativo, CPU, RAM, compilador y cantidad de hilos usados.
- Una sección por planificación con `t`, `A_t`, `U_t`, horizonte, límites
  activos, evaluaciones, tiempo, diagnósticos, función objetivo, vector
  `X_t` como pares curso–semestre y créditos/cantidad de cursos por semestre.
- Una sección por semestre ejecutado con cursos cursados, `q_i`, resultados
  `Y_it`, aprobados, reprobados, `A_{t+1}` y `U_{t+1}`.
- Resumen con estado final, evaluaciones acumuladas, tiempo de búsqueda y
  tiempo total. La evaluación registrada se vuelve a comprobar con
  `evaluate_plan`; esto no reemplaza la validación cruzada independiente
  entre algoritmos prevista por la tabla.

## Interpretación del CSV

- `tiempo_s` suma los tiempos de búsqueda de todas las planificaciones de
  la ejecución; excluye impresión y escritura. `evaluaciones` suma también
  las evaluaciones de cada búsqueda. El presupuesto de CLI se aplica **por
  planificación**, no a toda la trayectoria dinámica.
- `factible=1` indica que el modo terminó correctamente: un plan factible
  para una ejecución estática, las dos planificaciones requeridas en un
  avance explícito, o la trayectoria completada en una simulación.
- `valor` es la carga máxima del plan para una ejecución estática. En
  simulaciones y avances queda vacío: cada estado `t` tiene su propia
  función objetivo y el Markdown registra cada valor por separado. No se
  inventa un objetivo único para toda la trayectoria.
- `gap` se calcula en ejecuciones estáticas con `--reference-value N`, usando
  `100 × (valor − N) / N`. Quien ejecuta debe haber validado esa referencia.
  En trayectorias dinámicas queda vacío, porque cada estado necesita su
  propio valor. Las referencias clásicas no se aplican a replanificaciones;
  además, BACP12 mantiene una discrepancia documentada entre 17 y 18.

El formato, la cantidad de semillas, el presupuesto común y la división de
escenarios de configuración/evaluación siguen siendo **propuestas** de
`tabla_decisiones.pdf` hasta que el grupo las acuerde. Generar reportes no
convierte una ejecución en un experimento final ni certifica optimalidad.
