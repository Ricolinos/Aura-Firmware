# Checklist de fidelidad L1–L15 (Fase 25)

Verificación cruzada de los 15 principios de diseño de `PLAN-UX.md` §1 contra lo
que quedó realmente implementado en las Fases 12–24. Fuente de verdad:
`DECISIONS.md` (D-024 en adelante) — no se marca nada como "hecho" sin una
decisión que lo respalde, y todo lo parcial/diferido cita la decisión que lo
explica, siguiendo el mismo criterio de honestidad técnica del resto del
proyecto.

Leyenda: ✅ Completo · 🟡 Parcial · ⏸️ Diferido/no implementado.

## L1 — Composición por capas
**🟡 Parcial (por diseño).** D-024/D-030/D-058. Las transiciones no mueven
capas/viewports literales — un intento temprano de hacerlo así causó un
crash real por viewports fuera de límites (D-030); en su lugar se
implementó un "wipe/reveal" que reproduce el efecto visual sin el riesgo.
Deviación consciente y documentada, no un olvido.

## L2 — Pantalla dividida izquierda/derecha
**✅ Completo.** D-057 (Fase 15). `aura_widgets_draw_list()` generaliza el
split 168px/151px a prácticamente todas las listas, cae a ancho completo en
modo Ultra.

## L3 — Panel derecho con retardo ~1s
**✅ Completo.** D-057 (Fase 15). Debounce real (`s_panel_pending_*`),
verificado en el simulador.

## L4 — Gramática de transiciones T1–T4
**🟡 Parcial.** D-024/D-030 (T1/T3), D-058 (T4, Fase 16). T2
("panel-que-se-estira") nunca se construyó — diferido dos veces (D-058,
D-060) por falta de una pantalla consumidora real (la candidata natural,
fecha/hora icónica, también está diferida — ver L10).

## L5 — Barra de estado con reglas fijas
**✅ Completo.** D-054 (Fase 13). `aura_statusbar.c` implementa todas las
reglas (batería siempre a la derecha, play/pausa, candado de Hold, título
izq/centrado).

## L6 — Carátulas vivas en panel derecho
**⏸️ Diferido.** Existió un prototipo (D-025) con rotación/crossfade sobre
un layout arriba/abajo que resultó ser el layout incorrecto (ver L2); D-057
lo retiró al corregir a izquierda/derecha y el panel hoy muestra un icono
estático agrandado, no carátulas/posters/fotos rotando cada 7s con deriva.
No reconstruido todavía sobre el layout correcto.

## L7 — Jerarquía musical profunda con atajos
**⏸️ Diferido.** D-059 (Fase 17), explícito: "Todas las canciones" y el
cruce Género×Artista/Álbum quedan fuera, calificados como "un cambio de
navegación real" que merece su propia pasada.

## L8 — Búsqueda como widget del clickwheel
**⏸️ Diferido.** D-059 (Fase 17), explícito: "un subsistema de interacción
completo... merece su propia fase dedicada". No existe ningún código de
búsqueda en el árbol.

## L9 — El estado nunca se pierde
**🟡 Parcial.** El mecanismo subyacente (pila de navegación con memoria de
selección por nivel, `aura_nav`) funciona y fue corregido de un bug real de
desincronización (D-027) — pero dos de las tres garantías nombradas por L9
son discutibles porque las features que necesitarían persistir estado
(búsqueda, L8; cronómetro, Fase 21) no existen: D-063 omitió Extras por
completo.

## L10 — Configuración icónica de fecha/hora animada
**⏸️ Diferido.** D-060 (Fase 18), explícito: implementarlo bien requiere
RTC + dos selectores de dígitos + T2 a la vez, "demasiado para esta
pasada".

## L11 — Booleanos in situ, opción múltiple con checkmark
**✅ Completo.** D-054 construyó el widget; D-059 (Aleatorio) y D-060
(Clicker) son sus primeros consumidores reales; la lista de opción
múltiple con checkmark (Tema/Gráficos/EQ/Idioma/Repetir) es anterior a
este rango y sigue vigente.

## L12 — "Acerca de" en 3 modos
**🟡 Parcial.** D-060 diferí­a "Acerca de" entero por falta de datos reales;
D-066 (Fase 24) cierra la mitad del hueco — manifest real de Studio
(`sync_summary.cfg`) mostrado en pantalla, con un bug de unidades
encontrado y corregido por captura visual — pero **no** implementa los 3
modos navegables con prev/next que pedía el plan original; hoy es una
única vista.

## L13 — Fotos sin zoom (fit/fill + paneo)
**⏸️ Diferido.** D-062 (Fase 20), explícito: "una función nueva real...
merece su propia pasada". El visor actual (D-028) decodifica JPG/BMP pero
no tiene modos fit/fill ni paneo.

## L14 — Menú configurable con "Restaurar menú"
**🟡 Parcial.** D-060 (Fase 18): pantalla "Menú principal" con 3 booleanos
reales (mostrar Videos/Fotos/Ahora suena) que filtran el menú raíz — pero
no existe un "Restaurar menú" dedicado, solo el "Restablecer ajustes"
general que de paso también restaura esos tres booleanos.

## L15 — Ninguna voz ajena
**✅ Mayormente completo, con excepciones documentadas a propósito.**
D-051 (higiene: sin voz, sin defaults heredados), D-052 (logo de arranque
propio), D-055/D-056 (splash re-skineado + ~20 mensajes traducidos),
D-061 (logo USB propio + fade de retroiluminación), D-064 (bootloader
silencioso en el camino feliz). Dos excepciones **deliberadas**, no
descuidos: la pantalla de pánico/excepciones se dejó intacta a propósito
(D-061, no puede depender de código de Aura que podría ser la causa del
propio fallo) y el texto de splash sin mapear cae al inglés original de
Rockbox en vez de una frase genérica de Aura (D-056, para no ocultar
información de diagnóstico real).

## Resumen

| Estado | Cantidad | Principios |
|---|---|---|
| ✅ Completo | 5 | L2, L3, L5, L11, L15 |
| 🟡 Parcial | 6 | L1, L4, L9, L12, L14, (L15 con matices) |
| ⏸️ Diferido | 5 | L6, L7, L8, L10, L13 |

Los 5 principios diferidos comparten un patrón: cada uno es, en la práctica,
una feature completa nueva (carátulas vivas, jerarquía+búsqueda musical,
reloj icónico animado, fit/fill de fotos), no un ajuste incremental — de ahí
que cada decisión que los toca (D-057 a D-062) los haya calificado
explícitamente como fuera de alcance de "una pasada más" en lugar de
apretarlos al final de una fase ya cargada. Ninguno quedó a medias en
silencio: los 11 principios no-✅ tienen una decisión de `DECISIONS.md` que
explica por qué y qué haría falta para cerrarlos.
