# Parte dinámica del DBACP

El dinamismo se incorpora mediante un ciclo que se repite al terminar cada
semestre: **planificar → simular resultados → actualizar el estado →
replanificar**.

## 1. Generar una planificación inicial

En el semestre actual $t$, el algoritmo construye una solución

$$
X_t=(x_i)_{i\in U_t},
$$

donde $U_t$ es el conjunto de asignaturas pendientes y $x_i$ indica el
semestre asignado a la asignatura $i$. Cada asignatura pendiente aparece una
vez en el plan.

## 2. Simular el resultado académico

Se cursan las asignaturas programadas para el semestre actual:

$$
K_t=\{i\in U_t:x_i=t\}.
$$

Para cada $i\in K_t$, se simula si el estudiante aprueba o reprueba:

$$
Y_i^t\sim\operatorname{Bernoulli}(q_i),
$$

donde $Y_i^t=1$ significa **aprobado** y $Y_i^t=0$ significa
**reprobado**. Por ejemplo, $q_i=0{,}8$ representa una probabilidad de
aprobación del 80 % para esa asignatura.

## 3. Actualizar el estado del estudiante

Sea $Pass_t=\{i\in K_t:Y_i^t=1\}$ el conjunto de asignaturas aprobadas
durante el semestre. Al finalizarlo, se actualizan:

$$
A_{t+1}=A_t\cup Pass_t,\qquad
U_{t+1}=C\setminus A_{t+1},\qquad
P_{t+1}=\{t+1,\ldots,T\}.
$$

Las asignaturas reprobadas **permanecen en $U_{t+1}$** para volver a ser
planificadas.

## 4. Replanificar

Con el nuevo estado, el algoritmo genera otra solución:

$$
X_{t+1}=(x_i)_{i\in U_{t+1}}.
$$

Esta solución considera los cursos pendientes, los prerrequisitos aún
activos y los semestres restantes. Luego se repiten los pasos 2 a 4 después
de cada semestre. Si los cursos pendientes ya no caben en el horizonte que
termina en $T$, el estado se informa como **sin planificación factible**.

## Uso implementado

`advance_state(instance, state, plan, passed_this_period)` aplica los pasos
2 y 3 a partir de resultados observados. Exige un plan factible y solo
admite aprobaciones de cursos programados en `t`; cualquier curso cursado
que no figure entre las aprobaciones queda pendiente. Después,
`initialize_plans(instance, next_state, options)` o
`run_mmas(instance, next_state, options)` vuelven a validar y planificar
dentro de `t+1,\ldots,T`. El ejecutable permite demostrar un
avance con `--init ... --advance-passed curso1,curso2`; una cadena vacía
indica que todos reprueban.

`simulate_semester(instance, state, plan, q, seed)` genera los resultados
$Y_i^t$ solo para los cursos programados en $t$. El vector `q` tiene una
probabilidad por curso en el orden de la instancia; todos sus valores deben
estar en $[0,1]$. Cada resultado compara un uniforme independiente con
$q_i$, de modo que aprobar tiene probabilidad $q_i$. Se usa un sorteo fijo
por par `(curso, semestre)` y semilla: el resultado para ese par no cambia
si otro algoritmo construye un plan distinto. Esta independencia es una
decisión de simulación, no una condición adicional del modelo matemático.

El ejecutable recorre todos los semestres con `--simulate --approval-prob Q
--simulation-seed N` junto con los parámetros de `--init`. Se puede fijar
un $q_i$ concreto con `--course-approval id=Q`, repetido para varios cursos.
Después de cada semestre informa aprobaciones, reprobaciones y nuevo estado;
sin `--period-cap`, si no encuentra un plan factible se detiene con
diagnóstico y sin extender $T$. El valor $q_i=0{,}8$ sigue siendo un
escenario sintético propuesto, no un dato de la instancia.

## Variante con cota de extensión

La ejecución con MMAS acepta `--period-cap N`, con $N>T$. Si el horizonte
original se agota o no se valida un plan tras una reprobación, el programa
vuelve a inicializar y a ejecutar MMAS sobre los cursos que sigan pendientes,
usando los semestres desde el estado actual hasta `N`. El ciclo de aprobación
y reprobación se repite también en esta segunda fase. El límite `N` es un
máximo, no una cantidad de semestres que deban ocuparse.

Para que un curso aislado pueda planificarse, en la fase extendida se relajan
a cero los mínimos de créditos y de cursos de los semestres aún disponibles.
Los máximos y prerrequisitos siguen vigentes. Esta variante se debe reportar
separadamente del modelo con $T$ fijo del informe de Francisco.
