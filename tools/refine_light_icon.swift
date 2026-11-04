// macOS artwork maintenance: swift tools/refine_light_icon.swift
// Rebuild the light PNG family from the retained generated artwork.
import Foundation
import CoreGraphics
import ImageIO
import UniformTypeIdentifiers

func read(_ path: String) -> CGImage {
    let source = CGImageSourceCreateWithURL(URL(fileURLWithPath: path) as CFURL, nil)!
    return CGImageSourceCreateImageAtIndex(source, 0, nil)!
}
func context(_ size: Int) -> CGContext {
    CGContext(data: nil, width: size, height: size, bitsPerComponent: 8,
              bytesPerRow: size * 4, space: CGColorSpace(name: CGColorSpace.sRGB)!,
              bitmapInfo: CGImageAlphaInfo.premultipliedLast.rawValue)!
}
func save(_ image: CGImage, _ path: String) {
    let output = CGImageDestinationCreateWithURL(URL(fileURLWithPath: path) as CFURL,
                                               UTType.png.identifier as CFString, 1, nil)!
    CGImageDestinationAddImage(output, image, nil)
    precondition(CGImageDestinationFinalize(output))
}
let root = URL(fileURLWithPath: #filePath).deletingLastPathComponent().deletingLastPathComponent().path
let assets = root + "/assets/images"
let source = read(assets + "/candidates/SpentIt_light_v2.png")
precondition(source.width == 1254 && source.height == 1254)
// This interior crop excludes the generated fringe. Its contents are mapped
// into an 824 px tile on a 1024 px canvas, with 100 px transparent margins.
let crop = source.cropping(to: CGRect(x: 132, y: 146, width: 976, height: 976))!
let high = context(4096)
high.scaleBy(x: 4, y: 4)
let tile = CGRect(x: 100, y: 100, width: 824, height: 824)
high.addPath(CGPath(roundedRect: tile, cornerWidth: 190, cornerHeight: 190, transform: nil))
high.clip()
// Generated artwork has slight translucency even inside the tile. Composite
// onto opaque white before resampling, preserving transparency only outside.
high.setFillColor(CGColor(gray: 1, alpha: 1))
high.fill(tile)
high.interpolationQuality = .high
high.draw(crop, in: tile)
let master = high.makeImage()!
for size in [16, 32, 64, 128, 256, 512, 1024] {
    let output = context(size)
    output.interpolationQuality = .high
    output.draw(master, in: CGRect(x: 0, y: 0, width: size, height: size))
    save(output.makeImage()!, assets + "/SpentIt_light_\(size).png")
}
print("Generated seven light PNGs with geometric corners and transparent margins.")
