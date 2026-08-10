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
- [DECISIONS.md](DECISIONS.md) — registro de decisiones técnicas (más de 40 entradas, cada una con el problema real encontrado y la alternativa implementada).
- [docs/guia-instalacion.md](docs/guia-instalacion.md) — guía para el usuario final: instalar Aura y sincronizar tu biblioteca.
- [docs/guia-flasheo-restauracion.md](docs/guia-flasheo-restauracion.md) — detalle técnico del flasheo, dual-boot y restauración.
- [docs/guia-desarrollo.md](docs/guia-desarrollo.md) — cómo compilar cada parte del proyecto.

## Estado del proyecto

Las 12 fases del plan están completas (código, tests y evidencia visual committeados). Lo que **no** se pudo verificar en esta sesión de desarrollo, por no contar con un iPod físico ni un Mac con Xcode completamente configurado, es: el arranque/flasheo en hardware real, y `xcodebuild` de `AuraStudio.xcodeproj` (verificado en cambio con `swift build`/`swift test` sobre el mismo código fuente — ver D-034 en DECISIONS.md). El detalle completo, fase por fase, está en el resumen de cierre del historial de esta sesión y en las guías de arriba.

## Licencias

- `firmware/` — GPL v2 (heredada de Rockbox).
- `studio/` y `design-system/` — código independiente del proyecto Aura.
- Tipografía Inter (SIL OFL), íconos Lucide (ISC). Este proyecto no incluye ningún asset propietario de Apple.
