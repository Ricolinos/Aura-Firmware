---
name: apple2026-design-system
description: "Consulta esta skill antes de crear, modificar o revisar cualquier pantalla, animación, ícono, transición o componente visual de Aura (el firmware y Aura Studio). Cubre el sistema de diseño Apple2026 (color, tipografía, geometría, movimiento), el comportamiento heredado del firmware original del iPod Classic 2008 (taxonomía de pantallas, transiciones), y la especificación del reproductor Ahora suena. Dispárala para cualquier tarea de UI/UX aunque el usuario no diga la palabra 'diseño' — por ejemplo al implementar un menú nuevo, ajustar una transición, tocar apps/apple2026_shell.h, o trabajar en icons/ o skins/."
---

# Sistema de diseño Apple2026 — guía de consulta

Aura recrea el software que Apple enviaría con un iPod Classic en 2026. Antes de escribir o modificar cualquier código que afecte a la interfaz, lee completos los documentos fuente — no confíes en la memoria de la conversación ni improvises valores nuevos:

1. **`docs/design/Reglas de diseño Apple2026 (v2).md`** — tokens de color (`a26_palette`), tipografía, iconografía (SF Symbols), geometría (pastilla de selección, radios concéntricos, barra de deslizamiento), movimiento (resorte con sobrepaso para controles, fundido lineal para contenido), anti-patrones. Es la fuente de verdad para cualquier valor numérico o de color — nunca hardcodees uno que no esté ya aquí.
2. **`docs/design/Reglas de comportamiento - iPod Classic original (2008).md`** — taxonomía de pantallas (`SPLIT`, `LISTA-COMPLETA`, `FULL-COLD`, `FULL-CARRY`, `FULL-COVERFLOW`, `MODAL`), catálogo de transiciones con nombre, modelo de capas. Consúltalo antes de decidir cómo debe entrar o salir cualquier pantalla nueva.
3. **`docs/design/Reproductor - Ahora suena.md`** — especificación completa de la pantalla Ahora suena: reflejo de carátula, los 5 modos de la rueda (volumen, avance, listas, letra, estrellas), vista de letra.

## Cómo aplicar esto

- Antes de dar por terminada cualquier pantalla nueva, revisa la "Lista de control para toda pantalla nueva" al final del documento 1.
- Si la tarea no está resuelta en estos documentos, trátalo como una decisión de diseño abierta: propónla explícitamente en la respuesta en vez de inventar un valor o un patrón nuevo.
- Nunca hardcodees RGB, tamaños de fuente, radios o cadencias de animación que no aparezcan ya en estos documentos — si hace falta un valor nuevo, derívalo del sistema de tokens existente en vez de uno suelto.
- Toda animación nueva reutiliza las cadencias y curvas ya definidas (fundido lineal ~330ms para contenido, resorte con sobrepaso para controles) — no introduzcas una curva de easing distinta sin dejarla documentada en el archivo correspondiente.
- Si el cambio toca geometría, assets o el contrato de transición de una pantalla, dilo explícitamente para que se pueda actualizar el documento fuente después.
