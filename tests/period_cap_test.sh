#!/usr/bin/env bash
set -euo pipefail

fixture=$(mktemp)
results_dir=$(mktemp -d)
trap 'rm -f "$fixture" "$results_dir"/*.md "$results_dir"/resultados.csv; rmdir "$results_dir"' EXIT

cat > "$fixture" <<'DATA'
p = 1;
a = 1;
b = 2;
c = 1;
d = 2;
courses = {A};
credit = [1];
prereq = {};
DATA

search=("$fixture" --mmas --seed 1 --attempts 2 --repair-steps 2
        --rcl 1 --init-budget 2 --eval-budget 10 --ants 2 --alpha 1
        --beta 1 --evaporation 0.2 --tau-min-ratio 0.05
        --results-dir "$results_dir")
common=("${search[@]}" --simulate --approval-prob 0 --simulation-seed 7)

set +e
output=$(./min_max "${common[@]}" --period-cap 3 2>&1)
status=$?
set -e
if [[ $status -ne 3 || $output != *"Extensión activada: T=1, cota=3"* ||
      $output != *"MMAS t=2:"* ||
      $output != *"Simulación semestre 3"* ||
      $output != *"cursos pendientes después de la cota 3"* ]]; then
    echo "Falló la simulación con reprobaciones en la fase adicional" >&2
    echo "$output" >&2
    exit 1
fi
report_path=${output##*Reporte guardado: }
if [[ ! -f $report_path ]] ||
   ! grep -q '### Vector X_t' "$report_path" ||
   ! grep -q '## Resultado académico del semestre 2' "$report_path" ||
   ! grep -q 'Evaluaciones acumuladas:' "$report_path"; then
    echo "El reporte debe guardar planes, resultados y métricas" >&2
    exit 1
fi

output=$(./min_max "${search[@]}" --simulate --approval-prob 0.5 \
         --simulation-seed 3 --period-cap 3 2>&1)
if [[ $output != *"Extensión activada: T=1, cota=3"* ||
      $output != *"Trayectoria completada en el semestre 2"* ]]; then
    echo "Un curso aprobado en la fase adicional debe completar la trayectoria" >&2
    echo "$output" >&2
    exit 1
fi

output=$(./min_max "${search[@]}" --advance-passed "" --period-cap 3 2>&1)
if [[ $output != *"Extensión activada: T=1, cota=3"* ||
      $output != *"MMAS t=2:"* ]]; then
    echo "La aprobación manual también debe activar la fase adicional" >&2
    exit 1
fi

set +e
output=$(./min_max "${common[@]}" 2>&1)
status=$?
set -e
if [[ $status -ne 3 || $output == *"Extensión activada"* ||
      $output == *"MMAS t=2:"* ]]; then
    echo "Sin cota debe mantenerse el horizonte original" >&2
    exit 1
fi

set +e
output=$(./min_max "${common[@]}" --period-cap 1 2>&1)
status=$?
set -e
if [[ $status -ne 1 || $output != *"--period-cap debe ser mayor que T"* ]]; then
    echo "Se debe rechazar una cota que no amplía T" >&2
    exit 1
fi

cat > "$fixture" <<'DATA'
p = 2;
a = 1;
b = 1;
c = 1;
d = 1;
courses = {A, B};
credit = [1, 1];
prereq = {<B, A>};
DATA

set +e
output=$(./min_max "${common[@]}" --period-cap 4 2>&1)
status=$?
set -e
if [[ $status -ne 3 || $output != *"Extensión activada: T=2, cota=4"* ||
      $output != *"Simulación semestre 2"* ||
      $output != *"Simulación semestre 3"* ]]; then
    echo "La extensión debe activarse si una reprobación bloquea el plan antes de T" >&2
    echo "$output" >&2
    exit 1
fi

output=$(./min_max "$fixture" --init --seed 1 --attempts 2 \
         --repair-steps 2 --rcl 1 --eval-budget 10 --reference-value 1 \
         --results-dir "$results_dir" 2>&1)
if [[ $output != *"Mejor plan factible"* ||
      $output != *"Reporte guardado:"* ]]; then
    echo "La inicialización estática también debe guardar su solución" >&2
    exit 1
fi
if [[ $(tail -n 1 "$results_dir/resultados.csv") != *',1,1,0.000000,'* ]]; then
    echo "El gap estático debe calcularse con la referencia entregada" >&2
    exit 1
fi

if [[ $(wc -l < "$results_dir/resultados.csv") -ne 7 ]] ||
   [[ $(head -n 1 "$results_dir/resultados.csv") != \
      'algoritmo,instancia,semilla,valor,factible,gap,tiempo_s,evaluaciones' ]]; then
    echo "El CSV debe tener una cabecera y una fila por ejecución válida" >&2
    exit 1
fi

echo "Cota de períodos: continuación MMAS, reprobación y compatibilidad verificadas."
