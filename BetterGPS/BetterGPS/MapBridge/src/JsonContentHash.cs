using System.Security.Cryptography;
using System.Text.Json;
namespace BetterGPSNavigation;

// Hash the same UTF-8 JSON without materializing the entire geometry payload.
internal static class JsonContentHash
{
    internal static string Compute<T>(T value)
    {
        using var hash=IncrementalHash.CreateHash(HashAlgorithmName.SHA256);
        using var stream=new Sink(hash);
        JsonSerializer.Serialize(stream,value,LiveProtocol.Json);
        return Convert.ToHexString(hash.GetHashAndReset());
    }
    private sealed class Sink(IncrementalHash hash):Stream
    {
        public override bool CanRead=>false;public override bool CanSeek=>false;public override bool CanWrite=>true;
        public override long Length=>throw new NotSupportedException();public override long Position{get=>throw new NotSupportedException();set=>throw new NotSupportedException();}
        public override void Flush(){}
        public override void Write(byte[] buffer,int offset,int count)=>hash.AppendData(buffer,offset,count);
        public override void Write(ReadOnlySpan<byte> buffer)=>hash.AppendData(buffer);
        public override int Read(byte[] buffer,int offset,int count)=>throw new NotSupportedException();
        public override long Seek(long offset,SeekOrigin origin)=>throw new NotSupportedException();
        public override void SetLength(long length)=>throw new NotSupportedException();
    }
}
