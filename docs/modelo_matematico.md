# Resumen del modelo matemático DBACP

El **Dynamic Balanced Academic Curriculum Problem (DBACP)** planifica los
cursos que le faltan a un estudiante desde el período actual hasta el final
del horizonte académico. Al terminar cada período, el plan se recalcula con
los cursos que efectivamente aprobó.

## Conjuntos y datos

| Símbolo | Significado |
| --- | --- |
| $C$ | Conjunto de todos los cursos del plan. |
| $P=\{1,\ldots,T\}$ | Períodos académicos; $T$ es el último período permitido. |
| $t$ | Período actual. |
| $r_i$ | Créditos del curso $i$. |
| $R$ | Relaciones de prerrequisito: $(i,j)\in R$ significa que $i$ es prerrequisito de $j$. |
| $A_t$ | Cursos aprobados antes del período $t$. |
| $U_t=C\setminus A_t$ | Cursos todavía pendientes. |
| $P_t=\{t,\ldots,T\}$ | Períodos disponibles para planificar desde $t$. |
| $L_{\min},L_{\max}$ | Mínimo y máximo de créditos permitidos por período. |
| $N_{\min},N_{\max}$ | Mínimo y máximo de cursos permitidos por período. |
| $q_i$ | Probabilidad de aprobar el curso $i$, entre 0 y 1. |

## Representación del plan

Para cada curso pendiente $i\in U_t$, la variable $x_i\in P_t$ indica el
período al que se asigna. La solución completa es el vector
$X_t=(x_i)_{i\in U_t}$. Como cada curso tiene una sola posición en el vector,
queda asignado exactamente una vez.

En cada período $p\in P_t$ se calculan:

$$
L_p(X_t)=\sum_{i\in U_t}r_i\,\mathbf{1}[x_i=p],\qquad
N_p(X_t)=\sum_{i\in U_t}\mathbf{1}[x_i=p].
$$

$L_p$ es la suma de créditos y $N_p$ la cantidad de cursos. El indicador
$\mathbf{1}[x_i=p]$ vale 1 si el curso $i$ está asignado a $p$ y 0 en caso
contrario.

## Objetivo y restricciones

El objetivo principal es **minimizar la mayor carga de créditos** entre los
períodos restantes: $\min_{X_t}\max_{p\in P_t}L_p(X_t)$.

Si dos planes tienen la misma carga máxima, se prefiere el que tenga menor
desviación cuadrática respecto de la carga promedio:

$$
AVG_t=\frac{\sum_{i\in U_t}r_i}{|P_t|},\qquad
E(X_t)=\sum_{p\in P_t}\bigl(L_p(X_t)-AVG_t\bigr)^2.
$$

La comparación es, por tanto, **lexicográfica**: primero carga máxima y
después $E(X_t)$. La medida secundaria no se suma con un peso al objetivo
principal.

Un plan válido debe cumplir, en **cada** período restante:

- **Prerrequisitos:** si $i$ es prerrequisito de $j$ y ambos siguen
  pendientes, $x_i<x_j$. Si $i\in A_t$, el requisito ya está satisfecho.
- **Créditos:** $L_{\min}\leq L_p(X_t)\leq L_{\max}$.
- **Número de cursos:** $N_{\min}\leq N_p(X_t)\leq N_{\max}$.

## Replanificación dinámica

En el período $t$ se cursan $K_t=\{i\in U_t:x_i=t\}$. Para cada
$i\in K_t$, el resultado $Y_{it}\sim\operatorname{Bernoulli}(q_i)$ vale 1
si aprueba y 0 si reprueba. Si $P_t^{\mathrm{ass}}$ es el conjunto de cursos
aprobados en ese período, el estado se actualiza así:

$$
A_{t+1}=A_t\cup P_t^{\mathrm{ass}},\qquad
U_{t+1}=C\setminus A_{t+1},\qquad
P_{t+1}=\{t+1,\ldots,T\}.
$$

Los cursos reprobados siguen en $U_{t+1}$ y se genera un nuevo plan
$X_{t+1}$. Por eso el DBACP es una **secuencia de planificaciones**, una por
estado académico. La probabilidad $q_i$ interviene en la simulación de
resultados, no en la función objetivo de un plan concreto.

**Límite del modelo:** el horizonte termina en $T$. Una reprobación puede
dejar un estado sin plan factible dentro de los períodos restantes. La
formulación tampoco define períodos específicos de oferta para cada curso.

Fuente: [Valencia-Francisco.pdf](Valencia-Francisco.pdf), sección III.
