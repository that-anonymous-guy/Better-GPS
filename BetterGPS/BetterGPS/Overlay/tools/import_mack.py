"""Import supplied Mack/TomTom artwork and losslessly encode its intro.

Uses the existing BGC3 XPRESS-HUFF frame container; no runtime video decoder.
"""
import argparse
import ctypes
import hashlib
import json
from pathlib import Path
import shutil
import struct
import subprocess

from PIL import Image


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source', type=Path, default=Path('C:/Users/Crym2/Downloads/newgpsoverlay/tomtom'))
    parser.add_argument('--preview', type=Path)
    args = parser.parse_args()
    destination = Path(__file__).resolve().parents[1] / 'BetterGPS_assets/material/tomtom'
    copied = []
    for folder in ['mack_infotainment', 'trucker6000']:
        for source in sorted((args.source / folder).rglob('*.dds')):
            if source.name == 'mack_cluster_intro.dds':
                continue  # Cluster importer owns the native-instrument splash.
            relative = source.relative_to(args.source)
            target = destination / relative
            target.parent.mkdir(parents=True, exist_ok=True)
            image = Image.open(source).convert('RGBA')
            header = source.read_bytes()[:148]
            fourcc = header[84:88]
            dxgi = struct.unpack_from('<I', header, 128)[0] if fourcc == b'DX10' else 0
            compressed_dds = fourcc in [b'DXT1', b'DXT3', b'DXT5'] or dxgi in [71, 72, 74, 75, 77, 78, 98, 99]
            # D3D11 rejects BC textures whose top-level dimensions are not
            # multiples of four. Decode offline, retaining the exact extent
            # and alpha; never pad/stretch the artwork or alter HomeDecoder.
            converted = compressed_dds and (image.width % 4 or image.height % 4)
            if converted:
                image.save(target, format='DDS')  # Uncompressed RGBA32.
                assert Image.open(target).convert('RGBA').tobytes() == image.tobytes()
            else:
                shutil.copy2(source, target)
            copied.append(dict(path=relative.as_posix(), width=image.width, height=image.height, source_sha256=digest(source), sha256=digest(target), encoding='RGBA32 lossless' if converted else 'source DDS unchanged'))
            if args.preview:
                preview = args.preview / relative.with_suffix('.png')
                preview.parent.mkdir(parents=True, exist_ok=True)
                image.save(preview)

    source = args.source / 'mack_infotainment/mack_infotainment_intro.mp4'
    probe = json.loads(subprocess.check_output(['ffprobe', '-v', 'error', '-select_streams', 'v:0', '-show_entries', 'stream=width,height,r_frame_rate,nb_frames,duration', '-of', 'json', str(source)]))['streams'][0]
    assert probe['r_frame_rate'] == '30/1' and int(probe['nb_frames']) == 140, probe
    width, height, count = 900, 544, 140
    frame_bytes = width * height * 4
    api = ctypes.WinDLL('cabinet', use_last_error=True)
    for name in ['CreateCompressor', 'CreateDecompressor']:
        getattr(api, name).argtypes = [ctypes.c_ulong, ctypes.c_void_p, ctypes.POINTER(ctypes.c_void_p)]
    for name in ['Compress', 'Decompress']:
        getattr(api, name).argtypes = [ctypes.c_void_p, ctypes.c_void_p, ctypes.c_size_t, ctypes.c_void_p, ctypes.c_size_t, ctypes.POINTER(ctypes.c_size_t)]
    for name in ['CloseCompressor', 'CloseDecompressor']:
        getattr(api, name).argtypes = [ctypes.c_void_p]
    compressor, decoder = ctypes.c_void_p(), ctypes.c_void_p()
    assert api.CreateCompressor(4, None, ctypes.byref(compressor))
    assert api.CreateDecompressor(4, None, ctypes.byref(decoder))
    compressed = ctypes.create_string_buffer(frame_bytes + 65536)
    restored = ctypes.create_string_buffer(frame_bytes)
    process = subprocess.Popen(['ffmpeg', '-v', 'error', '-i', str(source), '-vf', f'fps=30,scale={width}:{height}:flags=lanczos', '-frames:v', str(count), '-f', 'rawvideo', '-pix_fmt', 'bgra', 'pipe:1'], stdout=subprocess.PIPE)
    unique, lookup, timeline, hashes = [], {}, [], []
    try:
        for frame in range(count):
            raw = process.stdout.read(frame_bytes)
            assert len(raw) == frame_bytes, f'Short movie frame {frame}'
            # Preserve the established exact RGB10 neutral-black startup floor.
            raw = raw.replace(bytes((9, 9, 9, 255)), bytes((10, 10, 10, 255)))
            written = ctypes.c_size_t()
            assert api.Compress(compressor, raw, frame_bytes, compressed, len(compressed), ctypes.byref(written)), ctypes.get_last_error()
            block = compressed.raw[:written.value]
            size = ctypes.c_size_t()
            assert api.Decompress(decoder, block, len(block), restored, frame_bytes, ctypes.byref(size)), ctypes.get_last_error()
            assert size.value == frame_bytes and restored.raw == raw, f'Lossy frame {frame}'
            hashes.append(hashlib.sha256(raw).hexdigest())
            if block not in lookup:
                lookup[block] = len(unique)
                unique.append(block)
            timeline.append(lookup[block])
            if args.preview and frame in [0, count // 2, count - 1]:
                Image.frombytes('RGBA', (width, height), raw, 'raw', 'BGRA').save(args.preview / f'mack-intro-{frame:03}.png')
        assert process.wait() == 0
    finally:
        if process.poll() is None:
            process.kill()
            process.wait()
        api.CloseCompressor(compressor)
        api.CloseDecompressor(decoder)
    movie = destination / 'mack_infotainment/mack_infotainment_intro.bga'
    movie.write_bytes(struct.pack('<5I', 0x33434742, width, height, count, len(unique)) + struct.pack(f'<{count}H', *timeline) + struct.pack(f'<{len(unique)}I', *(len(frame) for frame in unique)) + b''.join(unique))
    report = dict(source=str(source), source_sha256=digest(source), source_stream=probe, width=width, height=height, frames=count, fps=30, duration_ms=(count * 1000 + 29) // 30, encoding='BGC3 BGRA8 XPRESS_HUFF lossless', unique_frames=len(unique), verified_frames=count, frame_sha256=hashes, bga_sha256=digest(movie), assets=copied)
    (movie.parent / 'import-manifest.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    print(json.dumps({key: value for key, value in report.items() if key not in ['frame_sha256', 'assets']}, indent=2))


if __name__ == '__main__':
    main()
