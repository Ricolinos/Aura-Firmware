---
name: apple2026-design-system
description: "Consulta esta skill antes de crear, modificar o revisar cualquier pantalla, animación, ícono, transición o componente visual de Aura (el firmware y Aura Studio). Cubre el sistema de diseño Apple2026 (color, tipografía, geometría, movimiento), el comportamiento heredado del firmware original del iPod Classic 2008 (taxonomía de pantallas, transiciones), y la especificación del reproductor Ahora suena. Dispárala para cualquier tarea de UI/UX aunque el usuario no diga la palabra 'diseño' — por ejemplo al implementar un menú nuevo, ajustar una transición, tocar apps/apple2026_shell.h, o trabajar en icons/ o skins/."
---

# Sistema de diseño Apple2026 — guía de consulta

Aura recrea el software que Apple enviaría con un iPod Classic en 2026. Antes de escribir o modificar cualquier código que afecte a la interfaz, lee los documentos fuente relevantes — no confíes en la memoria de la conversación ni improvises valores nuevos:

1. **`docs/aura-design-system/` — LA FUENTE DE VERDAD VIVA** (mapa en `00-INDICE.md`). Todo lo confirmado por el dueño del diseño vive aquí, actualizado con cada sesión de trabajo: `fundamentos/` (color, tipografía, espaciado, bordes), `componentes/` (status-bar, selector, left-panel, selection-summary, cover-flow, now-playing, lyrics-panel, scroll-indicator, marquee-text, dynamic-title…), `sistema/` (capas, navegación menús/contenido), `transiciones/00-vocabulario.md` (transiciones con nombre) y `efectos/` (sombras). Los valores numéricos ejecutables viven en `design-system/tokens.json` (que genera `apple2026_tokens.h`).
2. **`docs/design/` — base histórica original** (Reglas de diseño Apple2026 v2, Reglas de comportamiento iPod 2008, Reproductor Ahora suena, AUDITORIA-01). Consúltala SOLO para lo que el sistema vivo aún no cubra (p. ej. la taxonomía completa de pantallas del original). **Ante cualquier conflicto, gana `docs/aura-design-system/`** — p. ej. la spec vieja del reproductor describe una barra de progreso que ya no existe.

## Cómo aplicar esto

- Si la tarea no está resuelta en estos documentos, trátalo como una decisión de diseño abierta: propónla explícitamente en la respuesta en vez de inventar un valor o un patrón nuevo.
- Nunca hardcodees RGB, tamaños de fuente, radios o cadencias de animación que no aparezcan ya en estos documentos — si hace falta un valor nuevo, derívalo del sistema de tokens existente (`design-system/tokens.json`) en vez de uno suelto.
- Toda animación nueva reutiliza las cadencias y curvas ya definidas (fundido lineal ~330ms para contenido, resorte con sobrepaso para controles) — no introduzcas una curva de easing distinta sin dejarla documentada en el archivo correspondiente.
- Si el cambio toca geometría, assets o el contrato de transición de una pantalla, actualiza el archivo correspondiente de `docs/aura-design-system/` en la misma pasada — es la fuente viva, no un espejo que se sincroniza después.
- `DECISIONS.md` es bitácora histórica, no spec: registra la evolución (con correcciones incluidas); nunca tomes una entrada vieja como comportamiento vigente sin verificar contra el sistema vivo y el código.
