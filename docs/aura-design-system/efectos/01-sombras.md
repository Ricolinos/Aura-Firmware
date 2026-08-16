# Sombras

## Regla unificada

`SelectionSummary` (`componentes/selection-summary.md`) y `CoverDrift`
(`componentes/cover-drift.md`) **siempre** renderizan una sombra que simula
que `LeftPanel` está posicionado por encima de ellos — esta es la
definición del componente, no una condición especial.

**Rendimiento:** el usuario la desactiva con **Ajustes → Mostrar sombras**
(`aura_shadows_enabled()`, `apple2026_shell.c`; nació como "Sombra de
panel" en D-088 y D-154 lo renombró y lo hizo global), que apaga **todas**
las sombras del sistema: la de `LeftPanel` sobre el panel derecho, la del
álbum y la de la hoja del Modo 4 de `NowPlaying`, y la del indicador de la
barra. Sigue el principio de "máxima fidelidad primero" (`00-INDICE.md`) —
la sombra activada es el comportamiento base documentado aquí; el toggle
simplemente la apaga, no cambia su definición. *(Hueco de implementación,
no de diseño: la sombra del tile de `SelectionSummary` de D-267/D-270 hoy
no consulta ese toggle — es la única que lo ignora.)*

*(Nota histórica: la primera versión de esta regla la ataba a un estado de
"máximo rendimiento" de los gráficos del panel izquierdo — quedó
reemplazada por esta versión, que es más simple: la sombra es parte del
componente por default, y el control de rendimiento es un toggle
independiente, no una condición de cuándo aparece. La pregunta "qué
significa máximo rendimiento" quedó obsoleta con esa reformulación, D-088.)*

## Valores (resync 2026-08-16)

Dos sombras distintas, ambas negras, ambas con caída **lineal** (no hay
primitiva de blur gaussiano en este LCD; el barrido lineal es su
equivalente práctico). Viven en `design-system/tokens.json`.

| Sombra | Extensión | Opacidad pico | Forma de la caída | Origen |
|---|---|---|---|---|
| `LeftPanel` → contenido del panel derecho | 8px de ancho (`aura_ds.metrics.shadow.left_panel_shadow_width`) | 20% (`aura_ds.opacity.shadow_pct`) | Lineal en una sola columna, desde el borde del panel hacia la derecha; compositada sobre el contenido real (`aura_shell_draw_left_panel_shadow_over_content()`, D-258), no sobre un color plano | D-088 (valores marcados **provisional** en `tokens.json`, vetables por el dueño), D-258 |
| Tile de `SelectionSummary` (drop shadow bajo el ícono flotante) | offset +4px en Y (`selection_summary.shadow_offset_y`), barrido de 12px (`shadow_blur_px`) | 35% (`shadow_alpha_pct`) | Lineal 2D alrededor de todo el perímetro, medida como distancia real al rectángulo redondeado (SDF, `draw_tile_shadow()`, `aura_selection_summary.c`); mismo radio que el tile | D-267 (drop shadow), D-270 (barrido tipo gaussiano pedido por el dueño, "como la sombra que refleja el leftpanel") |

## Comportamiento

- **Estados de pantalla:** la sombra de `LeftPanel` solo existe en `split`
  — en `full` no hay panel izquierdo montado, así que no hay nada que
  proyecte (D-088). Es estática mientras el panel está montado.
- **Animación:** en `NowPlaying` Modo 4, la sombra del panel izquierdo sobre
  la hoja de letras tiene su **propio fade** (~165ms, la mitad del morph de
  330ms) en la entrada, y se desvanece con el morph al salir (D-141) — no
  aparece de golpe.

## Pendiente de definir

- [ ] Otros componentes futuros que proyecten sombra: definir si reutilizan
      exactamente uno de los dos perfiles de arriba o necesitan uno propio.
