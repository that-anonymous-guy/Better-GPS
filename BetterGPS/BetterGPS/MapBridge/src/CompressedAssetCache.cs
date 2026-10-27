using System.IO.Compression;
using System.Security.Cryptography;
using System.Text.Json;
namespace BetterGPSNavigation;

// Stream JSON into compression: never retain an expanded JSON string and an
// escaped JSON envelope alongside the object graph. The hash covers compressed bytes.
internal static class CompressedAssetCache
{
    internal static byte[] Encode<T>(T value,string fingerprint)
    {
        using var payload=new MemoryStream();
        using(var brotli=new BrotliStream(payload,CompressionLevel.Fastest,true))JsonSerializer.Serialize(brotli,value,LiveProtocol.Json);
        if(payload.Length>StorageBudget.CacheBytes-68)throw new IOException("Asset cache entry exceeds its budget.");
        if(!payload.TryGetBuffer(out var buffer))throw new IOException("Cache payload unavailable.");
        var bytes=GC.AllocateUninitializedArray<byte>(checked(68+(int)payload.Length));
        "BGC3"u8.CopyTo(bytes);Convert.FromHexString(fingerprint).CopyTo(bytes,4);
        var compressed=buffer.AsSpan(0,(int)payload.Length);SHA256.HashData(compressed).CopyTo(bytes,36);compressed.CopyTo(bytes.AsSpan(68));return bytes;
    }
    internal static bool TryRead<T>(Stream stream,string fingerprint,out T value)
    {
        value=default!;Span<byte> header=stackalloc byte[68];if(stream.Length<68)return false;stream.ReadExactly(header);
        if(!header[..4].SequenceEqual("BGC3"u8)){stream.Position=0;return false;}
        if(stream.Length>StorageBudget.CacheBytes||!header.Slice(4,32).SequenceEqual(Convert.FromHexString(fingerprint))||!header.Slice(36,32).SequenceEqual(SHA256.HashData(stream)))throw new InvalidDataException("Asset cache integrity mismatch.");
        stream.Position=68;using var brotli=new BrotliStream(stream,CompressionMode.Decompress,true);using var bounded=new CountingReadStream(brotli,512L*1024*1024);
        value=JsonSerializer.Deserialize<T>(bounded,LiveProtocol.Json)!;return true;
    }
}
