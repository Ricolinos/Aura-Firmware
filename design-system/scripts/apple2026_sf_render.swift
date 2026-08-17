// Renderiza un lote de SF Symbols (o SVG propios, D-263) a PNGs
// cuadrados, negro sobre alfa 0.
//
// D-286 (2026-08-16, separacion de repositorios): este script ya NO es
// parte del pipeline que genera el tema COMPILADO por defecto de este
// repositorio (ver design-system/tokens.json, icon.svg_overrides -- hoy
// cubre los 89 icon_key con SVG de Lucide/Phosphor, vendorizados). Se
// conserva intacto, sin borrar, como el CONSTRUCTOR del futuro tema
// opcional "Apple (uso personal)": Aura Studio lo invocaria localmente en
// la Mac del usuario, pidiendo los SF Symbols ya instalados en ESE
// sistema -- nunca redistribuidos (ver AUDIT-pre-split.md 0.1 y
// PLAN-theme-system.md). Solo corre en un Mac con SF Pro/SF Compact
// instaladas; no se ejecuta como parte de build_sim.sh/package_dist.sh
// de este repo salvo que alguien construya ese tema a mano.
//
// SF Symbols no se distribuyen como archivos sueltos que se puedan
// versionar (la app SF Symbols exporta SVG de a uno a mano, y ni siquiera
// esta instalada en esta maquina); la via programatica y reproducible es
// pedirselos al sistema con NSImage(systemSymbolName:) -- la MISMA fuente
// que usa cualquier app de Apple. Requiere macOS 11+.
//
// D-263 (encargo del dueno de producto): dos iconos (Musica, iPod para
// Acerca de) vienen de un SVG propio en vez de un SF Symbol -- Apple no
// publica un glifo de iPod, y el dueno queria una forma de nota musical
// distinta a la de SF Symbols para Musica. `svgPath` (job.svgPath, en vez
// de job.symbol) carga el archivo con NSImage(contentsOfFile:) y lo
// dibuja con el mismo 'contain'+centrado que un SF Symbol -- SIN
// SymbolConfiguration (eso solo aplica a simbolos del sistema, no a una
// imagen vectorial propia): el peso/grosor del trazo lo define el SVG
// mismo, no este script.
//
// Se procesa un lote entero por invocacion, leyendo la lista de trabajos
// como JSON por stdin: `swift file.swift` recompila el script en cada
// ejecucion (~2s), asi que 57 invocaciones costarian minutos y una sola
// cuesta segundos.
//
// Entrada (stdin):  [{"symbol":"music.note","px":20,"weight":"medium","out":"/ruta/x.png"}, ...]
//              o:    [{"svgPath":"/ruta/music.svg","px":20,"out":"/ruta/x.png"}, ...]
// Salida: los PNG pedidos. Codigo != 0 si algun simbolo/SVG no existe.
//
// El color no importa aca: generate.py umbraliza el alfa y aplica el
// color del tema/variante. Este script solo define la FORMA.

import AppKit
import Foundation

struct Job: Decodable {
    // Exactamente uno de los dos: `symbol` (SF Symbol del sistema) o
    // `svgPath` (SVG propio, D-263) -- `weight` solo aplica al primero.
    let symbol: String?
    let svgPath: String?
    let px: Int
    let weight: String?
    let out: String
    // pointSize fijo opcional: se dibuja a tamano NATURAL centrado en el
    // lienzo, sin contain -- para familias cuyo cuerpo debe medir igual
    // entre variantes de distinto ancho (bocina dinamica, 2026-08-12).
    let pt: Double?
    // Alto de lienzo opcional, independiente del ancho (`px`): por
    // defecto el lienzo es cuadrado (py = px), pero un simbolo de
    // aspecto natural muy distinto de 1:1 (bateria, 2:1 ancho:alto) se
    // aplasta verticalmente si se le fuerza a un cuadrado -- 'contain'
    // queda limitado por el ancho y el alto resultante encoge con el
    // (bateria en 12px cuadrados: solo 6px de alto real -- auditoria
    // de status-bar del dueno del diseno, 2026-08-14). Un
    // lienzo explicito ancho != alto deja que 'contain' quede limitado
    // por el alto real cuando corresponde, igual que ya haria un
    // lienzo cuadrado para un simbolo mas cuadrado.
    let py: Int?
}

let weights: [String: NSFont.Weight] = [
    "ultralight": .ultraLight, "thin": .thin, "light": .light,
    "regular": .regular, "medium": .medium, "semibold": .semibold,
    "bold": .bold, "heavy": .heavy, "black": .black,
]

func fail(_ msg: String) -> Never {
    FileHandle.standardError.write((msg + "\n").data(using: .utf8)!)
    exit(1)
}

let input = FileHandle.standardInput.readDataToEndOfFile()
guard let jobs = try? JSONDecoder().decode([Job].self, from: input) else {
    fail("no se pudo leer la lista de trabajos JSON por stdin")
}

for job in jobs {
    let image: NSImage
    let label: String
    let isSvg = job.svgPath != nil

    if let svgPath = job.svgPath {
        guard let svgImage = NSImage(contentsOfFile: svgPath) else {
            fail("no se pudo cargar el SVG: \(svgPath)")
        }
        image = svgImage
        label = svgPath
    } else if let symbolName = job.symbol {
        guard let weightName = job.weight, let weight = weights[weightName] else {
            fail("peso desconocido para \(symbolName): \(job.weight ?? "(ninguno)")")
        }
        guard let symbol = NSImage(systemSymbolName: symbolName, accessibilityDescription: nil) else {
            fail("SF Symbol no disponible en este macOS: \(symbolName)")
        }
        // pointSize se pide algo menor que el lienzo: a un pointSize P la
        // caja de un SF Symbol es mas alta que P, asi que pedir P = px lo
        // dejaria recortado o con un relleno optico distinto al resto.
        let config = NSImage.SymbolConfiguration(pointSize: job.pt ?? Double(job.px) * 0.82, weight: weight)
        guard let configured = symbol.withSymbolConfiguration(config) else {
            fail("no se pudo configurar \(symbolName)")
        }
        image = configured
        label = symbolName
    } else {
        fail("job sin 'symbol' ni 'svgPath': \(job.out)")
    }

    let canvasH = job.py ?? job.px

    guard let rep = NSBitmapImageRep(
        bitmapDataPlanes: nil, pixelsWide: job.px, pixelsHigh: canvasH,
        bitsPerSample: 8, samplesPerPixel: 4, hasAlpha: true, isPlanar: false,
        colorSpaceName: .deviceRGB, bytesPerRow: 0, bitsPerPixel: 0) else {
        fail("no se pudo crear el bitmap para \(label)")
    }
    rep.size = NSSize(width: job.px, height: canvasH)

    NSGraphicsContext.saveGraphicsState()
    NSGraphicsContext.current = NSGraphicsContext(bitmapImageRep: rep)
    NSGraphicsContext.current?.imageInterpolation = .high

    // Escalar para caber (contain) y centrar: preserva las proporciones
    // nativas de cada simbolo/SVG, que es lo que los hace ver opticamente
    // consistentes entre si dentro de un mismo tamano.
    let natural = image.size
    var scale = min(Double(job.px) / natural.width, Double(canvasH) / natural.height)
    if job.pt != nil && !isSvg {
        if natural.width > Double(job.px) || natural.height > Double(canvasH) {
            fail("\(label) a pointSize fijo no cabe en el lienzo de \(job.px)x\(canvasH)px "
                 + "(natural \(natural)) -- agranda el lienzo en tokens.json")
        }
        scale = 1.0 // tamano natural: el cuerpo mide igual en toda la familia
    }
    let drawn = NSSize(width: natural.width * scale, height: natural.height * scale)
    let origin = NSPoint(x: (Double(job.px) - drawn.width) / 2.0,
                         y: (Double(canvasH) - drawn.height) / 2.0)

    NSColor.black.set()
    image.draw(in: NSRect(origin: origin, size: drawn),
               from: .zero, operation: .sourceOver, fraction: 1.0)

    NSGraphicsContext.restoreGraphicsState()

    guard let png = rep.representation(using: .png, properties: [:]) else {
        fail("no se pudo codificar el PNG de \(label)")
    }
    do {
        try png.write(to: URL(fileURLWithPath: job.out))
    } catch {
        fail("no se pudo escribir \(job.out): \(error)")
    }
}
