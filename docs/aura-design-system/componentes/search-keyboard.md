# SearchKeyboard (Búsqueda)

El teclado clásico del iPod: **una tira de caracteres que la rueda
recorre**, no una cuadrícula. Pantalla completa, con `StatusBar (full)`
titulada "Búsqueda". Confirmado 2026-08-13.

## Anatomía (320×240)

| Zona | Contenido |
|---|---|
| y=26 | **Texto completo** escrito, en letra chica (Regular 8) centrado |
| y=44, alto 34 | **Caja de búsqueda**: pastilla de **ACENTO** a todo el ancho útil |
| dentro, x+6 | **Campo** blanco de 84×24 con lo escrito y el cursor |
| dentro, tras el campo | **Tira** de caracteres deslizante |
| y=92 en adelante | **Resultados en vivo** |

## La tira

- Caracteres: `A-Z`, `0-9` y el espacio (mostrado como `_`, para que la
  rueda nunca caiga sobre algo invisible).
- Se muestra en **MAYÚSCULAS** y **escribe minúsculas** — la búsqueda no
  distingue mayúsculas, así que lo único que decide el caso es cómo se
  lee lo escrito.
- **Ventana deslizante**: el carácter activo se mantiene en la **tercera
  posición visible** (2 de contexto a su izquierda) y la tira se corta
  por la derecha cuando el siguiente glifo ya no cabe.
- Tres señales distinguen el activo a la vez: **fuente** (Bold 14 vs
  Regular 12), **color** (blanco puro vs blanco al 59% sobre el acento)
  y **posición fija** en la ventana.

## El texto escrito

- **Arriba, completo**: hasta **22 caracteres** y después **puntos
  suspensivos a la derecha**.
- **En el campo, la cola**: cuando no cabe, se descarta desde el
  principio y se antepone `…` (`…abc`), de modo que siempre se ve lo
  último que se escribió.
- **Cursor**: barra fina justo tras el texto, **medio segundo encendido,
  medio apagado**. Sin él, la rueda recorriendo la tira no daría
  ninguna señal de que hay un punto de escritura.

## Controles

| Botón | Efecto |
|---|---|
| Rueda | Recorre la tira (con la dinámica de velocidad del sistema) |
| Select | Añade el carácter activo |
| Forward | Espacio |
| Backward (tap) | Borra un carácter |
| **Backward (mantener)** | **Borra todo** |
| Play | Confirma: abre los resultados a pantalla completa |
| Menu | Sale **conservando lo escrito** |

## Persistencia (requisito duro)

Lo escrito, la posición en la tira y los resultados **persisten mientras
dure la sesión**: salir con Menu a cualquier profundidad y volver a
entrar reanuda la búsqueda exactamente como estaba. Salir "sin querer"
nunca cuesta el texto.

## Resultados

- **En vivo**, bajo la caja: se recalculan solo cuando el texto cambia.
- Coincidencia por **subcadena en cualquier posición, sin distinguir
  mayúsculas** (mismo criterio que el operador `~` de tagcache).
- Se busca sobre la **etiqueta visible** de cada pista (título real, o
  nombre de archivo si no tiene tag), nunca sobre el crudo de la base
  de datos — si no, `<Untagged>` entraría como resultado de cualquier
  búsqueda que contenga "a", "un" o "tag", además de mostrarse tal cual,
  que es jerga prohibida.
- Sin coincidencias: mensaje de lista vacía centrado.
- **Play** abre la lista completa a pantalla completa (template de lista
  de elementos, con su riel A-Z); ahí **Select reproduce** la pista en
  un playlist de un solo elemento — la búsqueda no define un álbum ni un
  orden, así que encolar el resto sería inventar un contexto que el
  usuario no pidió.
