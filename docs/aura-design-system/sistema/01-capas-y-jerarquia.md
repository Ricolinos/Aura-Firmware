# Capas y Jerarquía de Dibujo

En un sistema web esto sería `z-index`. Aquí, sin compositor, es **orden de
dibujo por frame**: qué se pinta primero (fondo) y qué se pinta al final
(encima de todo). Cualquier componente nuevo tiene que declarar en qué capa
vive y bajo qué condición se monta.

## La pila de capas (de fondo a frente)

| Token | Nivel | Qué vive aquí | ¿Siempre montada? |
|---|---|---|---|
| `--layer-base` | 0 | Contenido base del panel derecho (o de pantalla completa cuando no hay split) | Sí |
| `--layer-content` | 10 | Contenido opcional *sobre* el panel derecho: animación de carátula, íconos con el nombre del menú seleccionado, etc. | **No** — opcional, puede no haber nada en esta capa |
| `--layer-panel` | 20 | Panel izquierdo (lista de menú) | Solo en estado `split` |
| `--layer-chrome` | 30 | Status Bar | **No** — depende de la pantalla; hay pantallas sin Status Bar |

## Reglas fijas

1. **Cuando `--layer-chrome` (Status Bar) está presente, es la capa más
   alta, sin excepción.** Ningún componente, en ningún estado, se dibuja
   encima de la status bar. Pero su presencia misma es condicional: no toda
   pantalla la monta. La regla no es "siempre existe", es "si existe, gana".
2. **`--layer-panel` (panel izquierdo) siempre está por encima de todo lo que
   haya del lado derecho**, sea `--layer-base` o `--layer-content`. Esto solo
   aplica cuando el panel izquierdo está montado (estado `split`).
3. **`--layer-content` es opcional.** No todo lo que se muestra en el panel
   derecho tiene una capa de contenido extra encima de `--layer-base` — hay
   que definir, por componente, si la tiene o no. La ausencia de
   `--layer-content` es un estado válido, no un error.
4. **El montaje de capas depende del estado de pantalla**, no solo del
   componente. `--layer-panel` no existe (no se dibuja, no ocupa presupuesto
   de frame) cuando estamos en pantalla completa — no es que esté oculta,
   es que no está montada.

## Por qué esto importa para Rockbox

En un dispositivo de 2008 sin GPU, redibujar capas que no cambiaron es
presupuesto de CPU desperdiciado (y batería). Declarar explícitamente qué capa
está montada en cada estado nos deja optimizar: si `--layer-panel` no está
montada en `(full)`, el renderer ni siquiera evalúa esa región en el frame.

## Pendiente de definir

- [ ] ¿`--layer-content` puede tener más de un elemento apilado, o es
      estrictamente 0 o 1 elemento?
- [ ] ¿Existen casos donde `--layer-base` mismo cambia de contenido sin pasar
      por una transición de capa (ej. scroll dentro del panel derecho)?
