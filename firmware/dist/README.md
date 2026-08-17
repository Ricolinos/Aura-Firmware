# firmware/dist/

Este directorio recibe los artefactos empaquetados del firmware cuando corres
`firmware/tools/package_dist.sh`. **No se versionan aquí** (ver `.gitignore`)
porque cada versión pesa varios MB y el historial ya cambió de tamaño una vez
por este motivo (ver `AUDIT-pre-split.md` de la separación de repositorios).

Artefactos que produce el script:

- `rockbox.ipod` — binario del firmware para el iPod Classic 6G.
- `rockbox.zip` — árbol `.rockbox/` completo (fuentes, íconos, códecs,
  plugins) para copiar al disco del dispositivo.
- `bootloader-ipod6g.ipod` — bootloader de arranque dual (compilación manual,
  ver nota abajo).
- `mks5lboot` — herramienta para grabar el bootloader por DFU.
- `AuraPalette.swift` — paleta de colores para Aura Studio (`design-system/generate.py --swift-out`, no lleva checksum: Studio lo reemplaza directo, no lo verifica en runtime).
- `MODIFICATIONS.md` — copia del listado GPL §2a, para adjuntar al Release.
- `THIRD-PARTY-NOTICES.txt` — licencias de Inter (SIL OFL), Lucide (ISC) y Phosphor (MIT), consolidadas para el Release — no lleva checksum, mismo trato que `MODIFICATIONS.md`.
- `checksums.txt` — SHA-256 de `rockbox.ipod`, `rockbox.zip`, `mks5lboot` y (si está presente) `bootloader-ipod6g.ipod`.

Con `--release-tag <tag>` (ej. `v0.1.0-beta`), además escribe
`.rockbox/aura/version.txt` **dentro de** `rockbox.zip` con ese tag —
es la única forma en que Aura Studio puede leer, del dispositivo
montado, qué versión tiene instalada (ver `PLAN-release-updates.md` en
la carpeta padre y `DECISIONS.md`). Sin el flag (build de desarrollo)
ese archivo no se escribe.

Para instalarlos en un dispositivo real, la vía sancionada del proyecto es
Aura Studio (repositorio aparte), que descarga estos artefactos desde un
[Release](../../../releases) de este repositorio, verifica sus checksums y
los copia/flashea siguiendo su propio procedimiento — ver `CONTRATO-firmware-studio.md`
en la raíz de este repo para el contrato completo entre ambos.

**Nota sobre el bootloader**: `package_dist.sh` compila `rockbox.ipod` y
`mks5lboot` de forma automática, pero compilar `bootloader-ipod6g.ipod`
requiere un toolchain específico del bootloader (`configure --type=B` en
`firmware/rockbox/`) que el script no automatiza todavía — se documenta el
paso manual dentro del propio script. Una vez compilado a mano, el binario
queda en `firmware/dist/` y **sobrevive** a las corridas siguientes de
`package_dist.sh` (el script no lo borra ni lo regenera) — así que un
Release cortado después de ese paso manual sí lo incluye, y el dispositivo
queda instalable desde cero (sin necesitar un Rockbox/Aura previo), no solo
actualizable.
