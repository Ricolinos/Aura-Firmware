# Checklist §10 (Apple2026 v2) — barrido final, Fase 32

Verificación del checklist de 10 puntos que cierra `Reglas de diseño
Apple2026 (v2).md` (§10, "Lista de control para toda pantalla nueva"),
aplicado al estado real del árbol tras las Fases 26–32. Fuente de verdad:
`DECISIONS.md` — nada se marca hecho sin una decisión que lo respalde, y
todo lo parcial/diferido cita la decisión que lo explica.

Leyenda: ✅ Completo · 🟡 Parcial (documentado) · ⏸️ Diferido (documentado).

## 1. ¿La firmaría Apple?
**✅** Es el criterio rector de todo el sistema desde D-072 en adelante —
paleta por token, tipografía de dos pesos, iconografía lineal, geometría
concéntrica, movimiento con resorte en la capa de controles.

## 2. Colores por token, nada hardcodeado
**✅, con guardia activa.** `a26_color()` es la única vía. Se encontraron
y corrigieron dos derivas reales de RGB hardcodeado en la frontera de
plugin (D-072, `apps/main.c` y `mpegplayer.c`) y un uso de tokens
equivocados —no un valor crudo, pero sí la semántica incorrecta— en
`aura_widgets_draw_slider()` (D-081, `SHELL_RAIL`/`ACCENT` en vez de
`PROGRESS_TRACK`/`PROGRESS_FILL`). `grep -rn "LCD_RGBPACK"` en `apps/aura/`
solo resuelve a los tres blends legítimos (D-073/D-077/D-080), auditado en
D-081.

## 3. Se ve correcta en claro y oscuro (capturas de ambos)
**✅.** Todas las decisiones D-072 en adelante incluyen capturas de ambos
temas; la matriz completa (`docs/screenshots/matrix/`, este mismo cierre
de Fase 32) cubre 2 temas × 3 modos de Gráficos para cada pantalla real
del inventario.

## 4. Esperas con pastilla flotante; estados con página de símbolo
**🟡 Parcial.** La cápsula flotante de espera existe y tiene consumidor
real (D-073: reemplaza "Preparando la biblioteca..." de página completa).
Las páginas de símbolo (96×96, tinta terciaria) para estados vacíos
**no se construyeron** — D-073 y D-077 lo diferieron dos veces por la
misma razón: es una herramienta nueva completa
(`apple2026_symbol_page.py`) sin alcanzar con el mismo rigor que el resto.
Los estados vacíos siguen como texto centrado (`draw_message_centered`,
`draw_empty_state`).

## 5. Íconos SF Symbols en ambas tiras, sin repetirse entre hermanos
**✅.** Todo ícono nuevo esta sesión se verificó contra el catálogo real
de AppKit antes de usarse (mismo método que encontró `ipod.slash`
inexistente, Fase 26) — nunca a ciegas. Auditado en D-081 que ninguna
lista de la app repite ícono entre filas hermanas.

## 6. Textos en español mexicano, al final de ambas tablas
**✅, con dos vacíos reales corregidos en el barrido final.** La
disciplina de "solo-añadir-al-final" se respetó en todas las cadenas
nuevas de la sesión. D-081 encontró y corrigió dos casos de jerga en
inglés que habían quedado sin traducir desde antes de esta sesión
("Playlists", "Clicker") y un caso de jerga de archivo (extensión
`.m3u8` visible al usuario) — ninguno introducido esta pasada, pero
tampoco atrapados hasta el barrido explícito.

## 7. Animaciones con `lcd_active()` y cadencia de la tabla
**✅.** D-076 encontró cero usos de `lcd_active()` en todo el módulo
`aura/` y centralizó la puerta en el ticker principal. Todas las
animaciones nuevas de esta sesión (pastilla, barra de deslizamiento,
resorte de modo en Ahora suena) pasan por esa misma puerta central.

## 8. El vidrio, si aparece, está solo en la capa de controles
**✅.** Decisión ya cerrada (doc §2.1: plano, sin transparencia real). El
único "fundido" simulado es la barra de deslizamiento (D-073), lograda
por interpolación de color contra el fondo, no por composición alfa
sobre contenido — y es la barra misma (un control), nunca contenido.

## 9. Contrato del auditor actualizado si cambió geometría o assets
**✅.** Cada cambio de geometría/asset de esta sesión (nuevos tokens de
radio, `list_inset`, iconos nuevos, supersampleo) se documentó en el doc
de diseño y/o `DECISIONS.md` en el mismo commit que lo introdujo.

## 10. Verificada en el simulador con el arnés
**✅.** Todo lo descrito en D-072 a D-081 se verificó con capturas reales
del simulador (`apple2026_sim_shot.sh`) — no solo "compila". La matriz
completa de este cierre de Fase 32 usa `apple2026_sim_matrix.sh` sobre el
inventario real de pantallas.

---

## Alcance explícitamente fuera de esta pasada

Igual que aclara PLAN-APPLE2026.md §32: el inventario del iPod original
incluye secciones que Aura no tiene (Podcasts, Extras completo, Salida
TV). Este barrido alinea lo que EXISTE — construir secciones nuevas del
árbol original es trabajo futuro por fase propia.

También quedan fuera de esta pasada, documentados con su decisión:
- Deriva ambiental de carátulas (D-076) — sin plomería de imagen real en
  el panel derecho todavía.
- Quickscreen (D-077) — necesita un gesto de "hold" que no existe en el
  vocabulario de botones de Aura.
- Vista de letra comprimida FULL-CARRY y crossfade de carátula (D-078).
- Transición modal 2.6 y medición de rendimiento real en ARM (D-079/D-080,
  Fase 31.3/31.4) — pendientes de sesión guiada con el dispositivo.
- Ícono de curva por preset de EQ (D-081) — necesita extender
  `aura_widgets_draw_list()` con un callback de dibujo por fila.
