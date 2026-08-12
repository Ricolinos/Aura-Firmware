# Sombras

## Regla unificada

`SelectionSummary` (`componentes/selection-summary.md`) y `CoverDrift`
(`componentes/cover-drift.md`) **siempre** renderizan una sombra que simula
que `LeftPanel` está posicionado por encima de ellos — esta es la
definición del componente, no una condición especial.

**Rendimiento:** el usuario puede **desactivar este efecto desde Ajustes**
para mejorar el rendimiento. Sigue el principio de "máxima fidelidad
primero" (`00-INDICE.md`) — la sombra activada es el comportamiento base
documentado aquí; el toggle de Ajustes simplemente la apaga, no cambia su
definición.

*(Nota histórica: la primera versión de esta regla la ataba a un estado de
"máximo rendimiento" de los gráficos del panel izquierdo — quedó
reemplazada por esta versión, que es más simple: la sombra es parte del
componente por default, y el control de rendimiento es un toggle
independiente, no una condición de cuándo aparece.)*

## Pendiente de definir (necesario antes de poder implementarse)

- [ ] **Qué significa "máximo rendimiento" exactamente** — ¿un modo específico
      de renderizado del panel izquierdo (ej. una animación o visualizador a
      su framerate/calidad más alta), o el estado de rendimiento del
      dispositivo en general? Esto es ambiguo tal cual está descrito y hay
      que aterrizarlo antes de que sea implementable.
- [ ] Valores: offset, blur, opacidad, color de la sombra
- [ ] ¿La sombra es estática mientras dura ese estado, o tiene su propia
      animación de entrada/salida al activarse/desactivarse el modo de
      máximo rendimiento?
- [ ] ¿Aplica en ambos estados de pantalla (`split` y `full`), o solo tiene
      sentido en `split` porque en `full` no hay panel izquierdo montado?
