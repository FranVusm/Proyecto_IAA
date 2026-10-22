# Presentación de avance 1: DBACP con MMAS

Presentación Beamer basada en la plantilla USM. El archivo principal es
`main.tex`; las seis diapositivas de contenido están en `slides/`.

## Compilar en Overleaf

1. En **New Project → Upload Project**, sube el archivo
   `../presentacion/Presentacion_Avance_1_DBACP_MMAS_Overleaf.zip`.
2. Comprueba que **Main document** sea `main.tex` y el compilador sea
   **pdfLaTeX**.
3. Pulsa **Recompile**. Cuando aparezca la vista previa, pulsa
   **Download PDF**. El archivo descargado es la presentación final.

El ZIP contiene `main.tex` en la raíz; conserva `slides/`, `images/`,
`config.sty` y `packages.sty` al importarlo.

## Compilar localmente en Ubuntu

Instala una vez los paquetes de TeX Live necesarios:

```bash
sudo apt update
sudo apt install texlive-latex-base texlive-latex-recommended texlive-pictures texlive-lang-spanish
```

Después, desde la raíz del proyecto:

```bash
make -C Presentation_Template_USM pdf
```

El PDF queda en `Presentation_Template_USM/main.pdf`. El comando compila dos
veces para actualizar las referencias de Beamer. Al modificar algún archivo
`.tex`, vuelve a ejecutar el mismo comando; `make` detecta los cambios.

## Guion de tiempo

Texto completo para ensayar: [Guion_presentacion.md](../presentacion/Guion_presentacion.md).

| Parte | Tiempo objetivo |
| --- | ---: |
| Portada | 10 s |
| Flujo del algoritmo | 25 s |
| Lectura y representación | 40 s |
| Restricciones y evaluación | 50 s |
| Inicialización | 55 s |
| MMAS y replanificación | 65 s |
| Evidencia y cierre | 35 s |

Total previsto: **4 min 40 s**, con 20 segundos de margen respecto del
máximo de cinco minutos de la rúbrica. La exposición debe centrarse en las
decisiones de diseño, alternativas descartadas y objetivo/efecto de cada
componente; no hace falta explicar el problema desde cero. La rúbrica solo
**sugiere** mostrar un video acelerado de la ejecución.

Para la demostración, el comando de ejemplo está en
[`docs/presentacion_avance.md`](../docs/presentacion_avance.md). Las cargas
máximas de la última diapositiva provienen de ejecuciones demostrativas con
presupuestos distintos y no constituyen una comparación controlada ni una
prueba de optimalidad. La referencia BACP12 de 18 es la usada en el informe
de Francisco; la discrepancia con 17 sigue abierta.
