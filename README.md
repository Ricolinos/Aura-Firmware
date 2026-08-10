# Aura

Ecosistema para revivir el iPod Classic 6G (2008) con un firmware moderno, minimalista y cerrado, más una app nativa de macOS para instalarlo y gestionar la biblioteca.

## Partes

| Directorio | Qué es |
|---|---|
| `firmware/` | Firmware **Aura**: fork de Rockbox (GPL) usado como capa de hardware, con interfaz propia "Aura UI". Compila para el simulador SDL (desarrollo en Mac) y para el target real `ipod6g`. |
| `design-system/` | Tokens de diseño (temas Claro/Oscuro) y pipeline de generación de assets: fuentes bitmap Inter, íconos Lucide. |
| `studio/` | **Aura Studio**: app nativa macOS (Swift + SwiftUI, Apple Silicon). Instalador del firmware + gestor de biblioteca con enriquecimiento automático y transcodificación de video. |
| `docs/` | Guías de instalación, desarrollo y flasheo (en español) + capturas de evidencia. |

## Documentos clave

- [PLAN.md](PLAN.md) — plan maestro con fases, riesgos y criterios de aceptación.
- [DECISIONS.md](DECISIONS.md) — registro de decisiones técnicas.

## Licencias

- `firmware/` — GPL v2 (heredada de Rockbox).
- `studio/` y `design-system/` — código independiente del proyecto Aura.
- Tipografía Inter (SIL OFL), íconos Lucide (ISC). Este proyecto no incluye ningún asset propietario de Apple.
